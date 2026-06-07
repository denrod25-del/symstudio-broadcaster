"""Generate a simple placeholder SymStudio icon (neon 'S' on dark rounded square)."""
from PIL import Image, ImageDraw, ImageFont
import os

SIZES = [16, 32, 48, 64, 128, 256]
BG = (18, 18, 28, 255)        # near-black
ACCENT = (0, 229, 255, 255)   # cyan accent
base = 256

img = Image.new("RGBA", (base, base), (0, 0, 0, 0))
d = ImageDraw.Draw(img)
# rounded-square background
d.rounded_rectangle([8, 8, base - 8, base - 8], radius=48, fill=BG)
# big 'S' glyph
try:
    font = ImageFont.truetype("arialbd.ttf", 180)
except OSError:
    font = ImageFont.load_default()
text = "S"
l, t, r, b = d.textbbox((0, 0), text, font=font)
w, h = r - l, b - t
d.text(((base - w) / 2 - l, (base - h) / 2 - t), text, font=font, fill=ACCENT)

out = os.path.join(os.path.dirname(__file__), "symstudio-icon.ico")
img.save(out, format="ICO", sizes=[(s, s) for s in SIZES])
print("wrote", out)
