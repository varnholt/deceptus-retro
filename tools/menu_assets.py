"""Prepare the source starmap texture for the GBA affine background."""
import math
import struct
from PIL import Image
from palettes import palette_rgb555


def compile_menu(data, output):
    source=Image.open(data/'effects/starmap_color.png').convert('RGB')
    x,y=source.width//2-64,source.height//2-64
    image=source.crop((x,y,x+128,y+128)).quantize(colors=240,dither=Image.Dither.NONE)
    palette=image.getpalette() or []
    image.putpalette(palette[:720]+[0]*(768-min(720,len(palette))))
    image.save(output/'menu-texture.png')
    pixels=bytes(image.getdata());graphics=bytearray()
    for top in range(0,128,8):
        for left in range(0,128,8):
            for row in range(8):graphics.extend(pixels[(top+row)*128+left:(top+row)*128+left+8])
    (output/'menu-texture.bin').write_bytes(graphics)
    (output/'menu-texture.pal').write_bytes(palette_rgb555(image,16))
    (output/'menu-map.bin').write_bytes(bytes(range(256)))
    (output/'menu-sine.bin').write_bytes(struct.pack('<4096h',*[round(math.sin(index*2*math.pi/4096)*256) for index in range(4096)]))
    with (output/'data.s').open('a') as handle:
        for label,filename in [('gba_menu_texture','menu-texture.bin'),('gba_menu_palette','menu-texture.pal'),('gba_menu_map','menu-map.bin'),('gba_menu_sine','menu-sine.bin')]:
            handle.write('.balign 4\n.global '+label+'\n'+label+':\n.incbin "'+(output/filename).as_posix()+'"\n')
    header=(output/'gba_assets.h').read_text()
    declarations='\n'.join('extern const deceptus_u8 '+label+'[];' for label in ('gba_menu_texture','gba_menu_palette','gba_menu_map','gba_menu_sine'))+'\n'
    (output/'gba_assets.h').write_text(header.replace('#endif',declarations+'#endif'))
