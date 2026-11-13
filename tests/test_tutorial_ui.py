from pathlib import Path
import sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from PIL import Image
from tutorial_ui import TutorialUI
from pixel_font import NativePixelFont
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT.parent/'deceptus_engine/data'

@unittest.skipUnless((DATA/'fonts/deceptum.ttf').exists(),'original font required')
class TutorialTextTests(unittest.TestCase):
    def test_glyph_advance_keeps_one_clear_pixel(self):
        font=NativePixelFont(DATA/'fonts')
        for char in 'New Game wooden mattress':
            image=Image.new('RGBA',(32,32))
            TutorialUI.text(image,(0,0),char,font,'white')
            bounds=image.getbbox()
            if bounds:
                advance=TutorialUI.text_width(char,font)+1
                self.assertLessEqual(bounds[2]+1,advance)
        image=Image.new('RGBA',(240,24))
        TutorialUI.text(image,(0,0),'New Game',font,'white')
        self.assertEqual(set(image.getchannel('A').getdata()),{0,255})

    def test_large_title_is_an_exact_integer_copy_of_original_pixels(self):
        font = NativePixelFont(DATA/'fonts')
        native = Image.new('RGBA', (120, 24))
        large = Image.new('RGBA', (240, 48))
        TutorialUI.text(native, (0, 0), 'Deceptus', font, 'white')
        TutorialUI.text(large, (0, 0), 'Deceptus', font, 'white', 2)
        expected = native.resize(large.size, Image.Resampling.NEAREST)
        self.assertEqual(large.tobytes(), expected.tobytes())
        self.assertEqual(TutorialUI.text_width('Deceptus',font,2), 2*TutorialUI.text_width('Deceptus',font))

    def test_source_menus_encode_with_native_bitmap_font(self):
        ui = TutorialUI(DATA)
        for kind in ('main','pause','options','controls','inventory','confirm','exit'):
            resource = ui.menu(kind)
            self.assertLessEqual(ui.resources[resource][3],192)

    def test_dialogue_wrap_uses_rendered_width_and_leaves_button_clear(self):
        ui=TutorialUI.__new__(TutorialUI)
        ui.font=NativePixelFont(DATA/'fonts')
        pages=ui.wrap_pages('A decrepit wooden bed with an old, filthy mattress riddled with stains. '*3)
        self.assertGreaterEqual(len(pages),2)
        for page in pages:
            self.assertLessEqual(len(page.splitlines()),2)
            for line in page.splitlines():self.assertLessEqual(ui.text_width(line,ui.font),204)

if __name__=='__main__':unittest.main()
