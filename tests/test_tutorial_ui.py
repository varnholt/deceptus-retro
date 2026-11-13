from pathlib import Path
import sys,unittest,math
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from PIL import Image,ImageFont
from tutorial_ui import TutorialUI
ROOT=Path(__file__).resolve().parents[1]
DATA=ROOT.parent/'deceptus_engine/data'

@unittest.skipUnless((DATA/'fonts/deceptum.ttf').exists(),'original font required')
class TutorialTextTests(unittest.TestCase):
    def test_glyph_advance_keeps_one_clear_pixel(self):
        font=ImageFont.truetype(str(DATA/'fonts/deceptum.ttf'),12)
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

    def test_dialogue_wrap_uses_rendered_width_and_leaves_button_clear(self):
        ui=TutorialUI.__new__(TutorialUI)
        ui.font=ImageFont.truetype(str(DATA/'fonts/deceptum.ttf'),12)
        pages=ui.wrap_pages('A decrepit wooden bed with an old, filthy mattress riddled with stains. '*3)
        self.assertGreaterEqual(len(pages),2)
        for page in pages:
            self.assertLessEqual(len(page.splitlines()),2)
            for line in page.splitlines():self.assertLessEqual(ui.text_width(line,ui.font),204)

if __name__=='__main__':unittest.main()
