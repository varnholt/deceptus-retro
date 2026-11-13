"""Compile native-size source dialogue windows, fonts and interaction indicators."""
import json
import math
import re
import struct
from PIL import Image, ImageDraw, ImageFont
from assets import indexed
from gba_assets import linear4
from palettes import palette_rgb555
from psd_layers import layers


class TutorialUI:
    def __init__(self, data):
        self.data=data
        self.font=ImageFont.truetype(str(data/'fonts/deceptum.ttf'),12)
        self.window=layers(data/'game/messagebox.psd')
        self.title_layers=layers(data/'menus/titlescreen.psd')
        self.pause_layers=layers(data/'menus/pause.psd')
        self.animations=json.loads((data/'sprites/interaction_animations.json').read_text())
        key_source=(data.parent/'src/game/mechanisms/controllerkeymap.h').read_text()
        self.keys=re.findall(r'"([^"]*)"',key_source.split('key_map{',1)[1].split('};',1)[0])
        self.icons=Image.open(data/'game/ui_icons.png').convert('RGBA')
        self.blob=bytearray();self.resources=[];self.cues={};self.presentations=[]
        self.menu_ids={}

    @staticmethod
    def text_width(text, font):
        return sum(max(math.ceil(font.getlength(char)), font.getbbox(char)[2]) + 1 for char in text) - (1 if text else 0)

    @staticmethod
    def text(image, position, text, font, fill):
        # keep each glyph on the native pixel grid, with one clear pixel between glyphs.
        draw = ImageDraw.Draw(image)
        draw.fontmode = '1'
        x, y = position
        origin = round(x)
        x = origin
        for char in text:
            if char == '\n':
                x = origin
                y += 16
                continue
            draw.text((x, round(y)), char, font=font, fill=fill)
            x += max(math.ceil(font.getlength(char)), font.getbbox(char)[2]) + 1

    def wrap_pages(self, text):
        lines=[];line=''
        for word in text.split():
            candidate=(line+' '+word).strip()
            if line and self.text_width(candidate,self.font)>204:
                lines.append(line);line=word
            else:line=candidate
        if line:lines.append(line)
        return ['\n'.join(lines[index:index+2]) for index in range(0,len(lines),2)]

    def encode(self, image):
        image=indexed(image,transparent=True)
        patterns=[bytes(32)];lookup={patterns[0]:0};cells=[]
        for y in range(0,image.height,8):
            for x in range(0,image.width,8):
                tile=linear4(image.crop((x,y,x+8,y+8)))
                if tile not in lookup:lookup[tile]=len(patterns);patterns.append(tile)
                pattern=lookup[tile]
                cells.append(pattern if pattern<128 else pattern+64)
        if len(patterns)>192:raise ValueError(f'UI page exceeds 192 resident patterns: {len(patterns)}')
        graphics=len(self.blob);self.blob.extend(b''.join(patterns))
        palette=len(self.blob);self.blob.extend(palette_rgb555(image,1))
        maps=len(self.blob);self.blob.extend(struct.pack('<'+str(len(cells))+'H',*cells))
        while len(self.blob)%4:self.blob.append(0)
        self.resources.append((graphics,palette,maps,len(patterns),image.width//8,image.height//8))
        return len(self.resources)-1

    def bubble(self, text, background='#ff101ad4', next_page=False):
        frame=self.window['window']['image']
        image=Image.new('RGBA',(224,80))
        colour=tuple(bytes.fromhex(background[-6:]))+(255,)
        image.paste(colour,(3,3,221,71))
        image.alpha_composite(frame.crop((0,0,112,78)),(0,0))
        image.alpha_composite(frame.crop((frame.width-112,0,frame.width,78)),(112,0))
        self.text(image,(8,4),text,self.font,(255,255,255,255))
        image.alpha_composite(self.window['yes_xbox_1']['image'],(164,48))
        if next_page:image.alpha_composite(self.window['next_page']['image'],(204,57))
        return self.encode(image)

    def cue(self, label, animation='examine'):
        key=label,animation
        if key in self.cues:return self.cues[key]
        spec=self.animations[animation]
        atlas=Image.open(self.data.parent/spec['texture']).convert('RGBA')
        button_index=self.keys.index('dpad_u')
        button=self.icons.crop((button_index%16*24,button_index//16*24,button_index%16*24+24,button_index//16*24+24))
        text_width=self.text_width(label,self.font)
        width=math.ceil(max(48,28+text_width)/8)*8
        if width>112:raise ValueError('interaction label exceeds its 112px native UI budget')
        first=len(self.resources)
        for index in range(spec['sprite_count']):
            image=Image.new('RGBA',(width,72))
            x,y=spec['frame_offset'];w,h=spec['frame_size']
            image.alpha_composite(atlas.crop((x+index*w,y,x+(index+1)*w,y+h)),(0,0))
            image.alpha_composite(button,(width-24,48))
            self.text(image,(0,51),label,self.font,(255,255,255,255))
            self.encode(image)
        self.cues[key]=(first,spec['sprite_count'])
        return self.cues[key]

    def presentation(self, action, first_page, cue, anchor):
        self.presentations.append((action,first_page,*cue,*anchor))

    def menu(self, kind, selection=0):
        image=Image.new('RGBA',(240,160));draw=ImageDraw.Draw(image)
        title_font=ImageFont.truetype(str(self.data/'fonts/deceptum.ttf'),24)
        title={'main':'Deceptus','options':'Options','controls':'Controls','inventory':'Inventory','confirm':'Quit Game?','exit':'Quit','pause':''}[kind]
        self.text(image,((240-self.text_width(title,title_font))/2,12),title,title_font,(127,171,253,255))
        labels=[]
        if kind=='main':labels=['New Game','Options','Quit']
        elif kind=='pause':
            header=self.pause_layers['header']['image'];image.alpha_composite(header,((240-header.width)//2,2));labels=['Resume','Options','Quit Game']
        elif kind=='options':labels=['Controls','Back']
        elif kind=='confirm':labels=['No','Yes']
        elif kind=='exit':
            for row,text in enumerate(['You can turn off the system.','B: Back']):self.text(image,((240-self.text_width(text,self.font))/2,70+row*24),text,self.font,'white')
        elif kind=='controls':
            for row,text in enumerate(['D-pad: Move','A / B: Jump','Down: Bend down','Down + A: Drop through','Select: Inventory','Start: Pause','B: Back']):self.text(image,(12,42+row*16),text,self.font,'white')
        elif kind=='inventory':
            if not selection:self.text(image,(100,78),'Empty',self.font,'white')
            animations=json.loads((self.data/'sprites/extra_animations.json').read_text())
            for bit,key,name in [(1,'locker_key_regular','Locker Key'),(2,'handle_regular','Iron Lever')]:
                if not selection&bit:continue
                spec=animations[key];x,y=spec['frame_offset'];w,h=spec['frame_size']
                icon=Image.open(self.data.parent/spec['texture']).convert('RGBA').crop((x,y,x+w,y+h))
                image.alpha_composite(icon,(16,48 if bit==1 else 96));self.text(image,(76,58 if bit==1 else 108),name,self.font,'white')
            self.text(image,((240-self.text_width('B / Select: Back',self.font))/2,142),'B / Select: Back',self.font,'white')
        for row,text in enumerate(labels):
            y=(80 if kind=='pause' else 68)+row*24
            width=self.text_width(text,self.font);colour=(127,171,253,255) if row==selection else (67,69,91,255)
            self.text(image,((240-width)/2,y),text,self.font,colour)
            if row==selection:
                left=self.title_layers['deco_l']['image'];right=self.title_layers['deco_r']['image']
                image.alpha_composite(left,(round((240-width)/2-left.width-4),y+3))
                image.alpha_composite(right,(round((240+width)/2+4),y+3))
        return self.encode(image)

    def finish(self, output):
        for kind,count in [('main',3),('pause',3),('options',2),('confirm',2),('controls',1),('exit',1),('inventory',4)]:
            self.menu_ids[kind]=[self.menu(kind,index) for index in range(count)]
        (output/'ui.bin').write_bytes(self.blob)
        with (output/'data.s').open('a') as handle:
            handle.write('.balign 4\n.global gba_ui_blob\ngba_ui_blob:\n.incbin "'+(output/'ui.bin').as_posix()+'"\n')
        header=(output/'gba_assets.h').read_text()
        declarations=['typedef struct gba_ui_resource { retro_u32 graphics, palette, map; retro_u16 patterns; retro_u8 columns, rows; } gba_ui_resource;',
            'typedef struct gba_ui_presentation { const retro_interaction *action; retro_u16 page_first, cue_first, cue_count; retro_i16 x,y; } gba_ui_presentation;',
            'extern const retro_u8 gba_ui_blob[];', 'extern const gba_ui_resource gba_ui_resources[];',
            'extern const gba_ui_presentation gba_ui_presentations[];', f'#define GBA_UI_PRESENTATIONS {len(self.presentations)}']
        (output/'gba_assets.h').write_text(header.replace('#endif','\n'.join(declarations)+'\n#endif'))
        lines=['const gba_ui_resource gba_ui_resources[] = {']
        lines += ['    {'+','.join(map(str,entry))+'},' for entry in self.resources]
        lines += ['};','const gba_ui_presentation gba_ui_presentations[] = {']
        lines += ['    {&'+action+', '+','.join(map(str,values))+'},' for action,*values in self.presentations]
        lines += ['};']
        for kind,resources in self.menu_ids.items():
            declarations.append('extern const retro_u16 gba_menu_'+kind+'[];')
            lines.append('const retro_u16 gba_menu_'+kind+'[] = {'+','.join(map(str,resources))+'};')
        (output/'gba_assets.h').write_text(header.replace('#endif','\n'.join(declarations)+'\n#endif'))
        return '\n'.join(lines)+'\n'
