from pathlib import Path
import struct
import sys
import unittest
from PIL import Image
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from tiled import TiledMap
from tileanimation import bake_tile_animations
from gba_assets import compile_streamed_background
ROOT=Path(__file__).resolve().parents[1]

class SourceTileAnimationTests(unittest.TestCase):
    def setUp(self):
        self.folder=ROOT/'build/tests/tile-animation'
        self.folder.mkdir(parents=True,exist_ok=True)
        image=Image.new('RGBA',(16,8),'red');image.paste('blue',(8,0,16,8));image.save(self.folder/'tiles.png')
        (self.folder/'map.tmx').write_text('''<map orientation="orthogonal" width="1" height="1" tilewidth="8" tileheight="8"><tileset firstgid="1" name="sample" tilewidth="8" tileheight="8" tilecount="2" columns="2"><image source="tiles.png" width="16" height="8"/><tile id="0"><animation><frame tileid="0" duration="80"/><frame tileid="1" duration="120"/></animation></tile></tileset><layer name="foam" width="1" height="1"><data encoding="csv">1</data></layer></map>''')

    def test_source_frames_and_loop_duration(self):
        tiled=TiledMap(self.folder/'map.tmx')
        for time,colour in [(0,(255,0,0)),(79,(255,0,0)),(80,(0,0,255)),(199,(0,0,255)),(200,(255,0,0))]:
            self.assertEqual(tiled.tile(1,time_ms=time).getpixel((0,0))[:3],colour)

    def test_bake_uses_original_frames_and_cumulative_timing(self):
        recipe=dict(map='map.tmx',crop_pixels=[0,0,8,8],visual_layers=['foam'],foreground_layers=['foam'],tile_animations=[dict(name='foam',layers=['foam'],plane=1)])
        clip=bake_tile_animations(self.folder,recipe)[0]
        self.assertEqual(clip['frame_durations'],[5,7])
        self.assertEqual(clip['frames'][0].getpixel((0,0)),(255,0,0,255))
        self.assertEqual(clip['frames'][1].getpixel((0,0)),(0,0,255,255))

    def test_fast_source_frames_never_emit_zero_tick_durations(self):
        path=self.folder/'map.tmx'
        source=path.read_text().replace('<frame tileid="0" duration="80"/><frame tileid="1" duration="120"/>',''.join('<frame tileid="0" duration="10"/>' for _ in range(5)))
        path.write_text(source)
        recipe=dict(map='map.tmx',crop_pixels=[0,0,8,8],visual_layers=['foam'],foreground_layers=['foam'],tile_animations=[dict(name='foam',layers=['foam'],plane=1)])
        clip=bake_tile_animations(self.folder,recipe)[0]
        self.assertEqual(clip['frame_durations'],[1,1,1])
        self.assertEqual(clip['source_frame_indices'],[0,1,3])

    def test_foreground_clip_preserves_transparency_and_separate_residency(self):
        background=Image.new('RGB',(8,8),'green');front=Image.new('RGBA',(8,8))
        clip=dict(name='foreground',plane=1,bounds=[0,0,8,8],frames=[background.convert('RGBA'),front],frame_count=2,frame_ticks=1)
        compile_streamed_background(background,self.folder,[clip],front)
        base=struct.unpack('<2H',(self.folder/'stream-cells.bin').read_bytes())[0]
        animated=struct.unpack('<4H',(self.folder/'animation-cells.bin').read_bytes())
        self.assertNotEqual(base,animated[0])
        self.assertEqual(animated[2],0)

if __name__=='__main__':unittest.main()
