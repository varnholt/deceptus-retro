"""Read the original Deceptum bitmap glyphs at their authored 13px size."""
from PIL import Image, ImageFont


class NativePixelFont:
    def __init__(self, folder):
        self.atlas = Image.open(folder / 'deceptum.png').convert('RGBA')
        self.metrics = ImageFont.truetype(str(folder / 'deceptum.ttf'), 13)
        self.cache = {}

    def glyph(self, char):
        if not 32 <= ord(char) <= 126:
            raise ValueError(f'unsupported native bitmap glyph: {char!r}')
        if char not in self.cache:
            index = ord(char) - 32
            left, top = index % 42 * 12, index // 42 * 18
            self.cache[char] = self.atlas.crop((left, top, left + 12, top + 18))
        return self.cache[char]

    def getlength(self, text):
        return sum(round(self.metrics.getlength(char)) for char in text)

    def getbbox(self, char):
        bounds = self.glyph(char).getbbox()
        if not bounds:
            return (0, 0, 0, 0)
        # the bitmap atlas has one extra pixel of left padding relative to the TTF.
        return (bounds[0] - 1, bounds[1], bounds[2] - 1, bounds[3])
