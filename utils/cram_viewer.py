from PIL import Image, ImageDraw, ImageFont

COLORS_IN_DUMP = 16
TILE_SIZE = 32
HSPACE_SIZE = 2
VSPACE_SIZE = 15
VSPACE_FONT_SHIFT_SIZE = 10
LEFT_PADDING = 20
RIGHT_PADDING = 200

CRAM_DUMPS = [
    ("[7b2c4] YELLOW SHADES", b"\x00\x00\x00\x22\x00\x44\x00\x66\x00\x88\x00\xaa\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"),
    ("[7b2e4] RED SHADES", b"\x00\x00\x00\x02\x00\x02\x00\x04\x00\x06\x00\x08\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"),
    ("[7b304] BEIGE SHADES", b"\x00\x00\x02\x44\x04\x66\x06\x88\x08\xaa\x0a\xcc\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"),
    ("[7b324] GREEN SHADES", b'\x00\x00\x00\x20\x00\x40\x00\x60\x00\x80\x00\xa0\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00'),
    ("[7b344] RAINBOW 0", b'\x00\x00\x0e\x86\x02\x42\x02\x62\x04\x64\x0e\x0e\x06\x88\x02\x4c\x02\x0e\x00\xce\x00\xc0\x04\x00\x0e\x00\x00\x24\x02\x46\x0e\xee'),
    ("[7b364] RAINBOW 1", b'\x0c\x86\x00\x00\x00\x02\x00\x22\x00\x24\x00\x46\x02\x68\x04\x8a\x06\xac\x08\xce\x0e\xa2\x02\x22\x04\x44\x06\x66\x0e\x80\x02\x86'),
    ("[7b384] RAINBOW 2", b'\x0c\x86\x0a\x42\x0a\x64\x0c\xa8\x0c\xaa\x0e\xca\x0e\xcc\x0e\xee\x02\x22\x04\x8a\x0c\x84\x0c\xe6\x00\x28\x00\x26\x00\x24\x00\x00'),
    ("[7b3a4] RAINBOW 3", b'\x00\x00\x00\x00\x04\x46\x06\x68\x06\x8a\x06\xac\x02\x24\x06\x44\x08\x86\x0c\xa8\x0e\xca\x0c\xec\x00\xae\x00\x6c\x02\xce\x0e\xee'),
    ("[7b3c4] WHITE AND RED SHADES", b'\x00\x00\x00\x00\x04\x44\x06\x66\x08\x88\x0a\xaa\x0c\xcc\x0e\xee\x00\x04\x00\x06\x00\x08\x00\x0a\x00\x0c\x02\x2c\x04\x4c\x08\x8e'),
]

WIDTH = COLORS_IN_DUMP * (TILE_SIZE + HSPACE_SIZE) + LEFT_PADDING + RIGHT_PADDING
HEIGHT = len(CRAM_DUMPS) * 2 * (TILE_SIZE + VSPACE_SIZE)

FONT = ImageFont.truetype("joystix monospace.otf", 30)


if __name__ == "__main__":
    img = Image.new("RGB", (WIDTH, HEIGHT))
    for i, (name, dump) in enumerate(CRAM_DUMPS):
        assert len(dump) == 2 * COLORS_IN_DUMP  # two bytes per color
        for j in range(COLORS_IN_DUMP):
            begin_x = LEFT_PADDING + j * (TILE_SIZE + HSPACE_SIZE)
            begin_y = (2 * i + 1) * (TILE_SIZE + VSPACE_SIZE)
            shape = [(begin_x, begin_y), (begin_x + TILE_SIZE, begin_y + TILE_SIZE)]

            # 0..7 -> 0..255
            def convert(value):
                return int(value * 255 / 0xE)

            blue = convert(dump[j * 2] & 0xF)
            green = convert((dump[j * 2 + 1] & 0xF0) >> 4)
            red = convert(dump[j * 2 + 1] & 0xF)

            rect = ImageDraw.Draw(img)
            rect.rectangle(shape, fill=(red, green, blue), outline ="white")

        text = ImageDraw.Draw(img)
        text.text((LEFT_PADDING, 2 * i * (TILE_SIZE + VSPACE_SIZE) + VSPACE_FONT_SHIFT_SIZE), name, font=FONT)
    img.show()
