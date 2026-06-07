"""Generate 5 distinct SymStudio logo concepts + a comparison contact sheet."""
import os, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

OUT = r"C:\Users\skyea\claude\SymStudio\assets\logo-options"
os.makedirs(OUT, exist_ok=True)

CYAN    = (0, 229, 255, 255)
MAGENTA = (255, 60, 170, 255)
PURPLE  = (150, 90, 255, 255)
RED     = (255, 60, 70, 255)
WHITE   = (240, 245, 250, 255)
GREY    = (150, 160, 175, 255)
TEAL    = (40, 230, 200, 255)

def font(size, bold=True):
    for c in (["arialbd.ttf","segoeuib.ttf","bahnschrift.ttf"] if bold
              else ["arial.ttf","segoeui.ttf","bahnschrift.ttf"]):
        try: return ImageFont.truetype(c, size)
        except OSError: continue
    return ImageFont.load_default()

def vgradient(size, top, bottom):
    w, h = size
    g = Image.new("RGBA", (1, h))
    for y in range(h):
        t = y / max(1, h-1)
        g.putpixel((0, y), tuple(int(top[i]*(1-t)+bottom[i]*t) for i in range(3)) + (255,))
    return g.resize((w, h))

def rounded(size, radius):
    m = Image.new("L", size, 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, size[0]-1, size[1]-1], radius, fill=255)
    return m

def panel(size=(512, 512), top=(14, 16, 30), bottom=(6, 7, 14), radius=96, border=None):
    img = Image.new("RGBA", size, (0, 0, 0, 0))
    bg = vgradient(size, top, bottom)
    img.paste(bg, (0, 0), rounded(size, radius))
    if border:
        ImageDraw.Draw(img).rounded_rectangle([3, 3, size[0]-4, size[1]-4], radius-2,
                                              outline=border, width=3)
    return img

def glow(img, draw_fn, color, radius=22, passes=2):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    draw_fn(ImageDraw.Draw(layer), color)
    blur = layer.filter(ImageFilter.GaussianBlur(radius))
    for _ in range(passes):
        img.alpha_composite(blur)

def neon_text(img, xy, text, fnt, color, glow_color, gr=20):
    glow(img, lambda d, c: d.text(xy, text, font=fnt, fill=c, anchor="mm"), glow_color, gr, 2)
    ImageDraw.Draw(img).text(xy, text, font=fnt, fill=color, anchor="mm")

# ---------- 1. Neon Monogram (synthwave chromatic S) ----------
def logo1():
    img = panel(border=CYAN)
    f = font(360)
    # magenta ghost offset for chromatic synthwave feel
    glow(img, lambda d, c: d.text((266+7, 256+6), "S", font=f, fill=c, anchor="mm"), MAGENTA, 16, 1)
    neon_text(img, (256, 256), "S", f, CYAN, CYAN, gr=22)
    return img

# ---------- 2. Record Ring (S inside a glowing record ring) ----------
def logo2():
    img = panel(top=(26, 14, 16), bottom=(10, 6, 8), border=(70, 30, 36, 255))
    def ring(d, c):
        d.ellipse([96, 96, 416, 416], outline=c, width=30)
    glow(img, ring, RED, 24, 2)
    ImageDraw.Draw(img).ellipse([96, 96, 416, 416], outline=RED, width=26)
    neon_text(img, (256, 250), "S", font(230), WHITE, (255, 120, 120, 255), gr=14)
    # small REC dot top
    glow(img, lambda d, c: d.ellipse([238, 120, 274, 156], fill=c), RED, 12, 2)
    ImageDraw.Draw(img).ellipse([240, 122, 272, 154], fill=RED)
    return img

# ---------- 3. Broadcast (S with live signal arcs) ----------
def logo3():
    img = panel(top=(8, 26, 30), bottom=(5, 10, 14), border=TEAL)
    def arcs(d, c):
        for i, r in enumerate((58, 92, 126)):
            w = 16 - i*2
            d.arc([256-r-150, 256-r, 256+r-150, 256+r], 300, 60, fill=c, width=w)  # left
            d.arc([256-r+150, 256-r, 256+r+150, 256+r], 120, 240, fill=c, width=w)  # right
    glow(img, arcs, TEAL, 16, 2)
    arcs(ImageDraw.Draw(img), TEAL)
    neon_text(img, (256, 256), "S", font(300), WHITE, TEAL, gr=18)
    return img

# ---------- 4. Wordmark (SymStudio logotype) ----------
def logo4():
    W, H = 1280, 420
    img = panel((W, H), top=(14, 16, 30), bottom=(7, 8, 16), radius=70)
    fb = font(150)
    # play-triangle accent glyph on the left
    def tri(d, c):
        d.polygon([(150, 150), (150, 270), (256, 210)], fill=c)
    glow(img, tri, CYAN, 16, 2)
    tri(ImageDraw.Draw(img), CYAN)
    # "Sym" cyan + "Studio" grey, baseline aligned
    d = ImageDraw.Draw(img)
    sym_w = d.textlength("Sym", font=fb)
    x0 = 330
    glow(img, lambda dd, c: dd.text((x0, H//2), "Sym", font=fb, fill=c, anchor="lm"), CYAN, 18, 2)
    d.text((x0, H//2), "Sym", font=fb, fill=CYAN, anchor="lm")
    d.text((x0+sym_w, H//2), "Studio", font=fb, fill=WHITE, anchor="lm")
    return img

# ---------- 5. Aperture (camera iris + S) ----------
def logo5():
    img = panel(top=(16, 12, 30), bottom=(7, 6, 14), border=(60, 50, 90, 255))
    cx, cy, R, rin = 256, 256, 168, 96
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for i in range(6):
        a0 = math.radians(60*i - 90)
        a1 = math.radians(60*(i+1) - 90)
        # blade: from inner hexagon vertex to outer circle, lerp cyan->purple
        t = i/5
        col = tuple(int(CYAN[k]*(1-t)+PURPLE[k]*t) for k in range(3)) + (255,)
        p_in0 = (cx+rin*math.cos(a0), cy+rin*math.sin(a0))
        p_in1 = (cx+rin*math.cos(a1), cy+rin*math.sin(a1))
        p_out1 = (cx+R*math.cos(a1), cy+R*math.sin(a1))
        d.line([p_in0, p_in1], fill=col, width=14)
        d.line([p_in1, p_out1], fill=col, width=14)
    blur = layer.filter(ImageFilter.GaussianBlur(10))
    img.alpha_composite(blur)
    img.alpha_composite(layer)
    ImageDraw.Draw(img).ellipse([cx-R-6, cy-R-6, cx+R+6, cy+R+6], outline=(90,80,140,255), width=3)
    neon_text(img, (cx, cy), "S", font(150), WHITE, CYAN, gr=12)
    return img

logos = [
    ("1 - Neon Monogram", "logo1-neon-monogram.png", logo1()),
    ("2 - Record Ring",   "logo2-record-ring.png",   logo2()),
    ("3 - Broadcast",     "logo3-broadcast.png",     logo3()),
    ("4 - Wordmark",      "logo4-wordmark.png",      logo4()),
    ("5 - Aperture",      "logo5-aperture.png",      logo5()),
]
for _, fn, im in logos:
    im.save(os.path.join(OUT, fn))

# ---------- contact sheet ----------
SW, SH = 1200, 940
sheet = Image.new("RGBA", (SW, SH), (13, 13, 20, 255))
sd = ImageDraw.Draw(sheet)
sd.text((40, 28), "SymStudio  -  logo options", font=font(46), fill=WHITE)
sd.text((42, 86), "pick a number (1-5)", font=font(26, False), fill=GREY)

def place(im, box_x, box_y, box_w, box_h, label):
    im2 = im.copy()
    im2.thumbnail((box_w, box_h))
    ox = box_x + (box_w - im2.width)//2
    oy = box_y + (box_h - im2.height)//2
    sheet.alpha_composite(im2, (ox, oy))
    sd.text((box_x + box_w//2, box_y + box_h + 22), label, font=font(28), fill=CYAN, anchor="mm")

# row 1: logos 1,2,3 (squares)
place(logos[0][2], 60,  150, 320, 320, logos[0][0])
place(logos[1][2], 440, 150, 320, 320, logos[1][0])
place(logos[2][2], 820, 150, 320, 320, logos[2][0])
# row 2: logo 5 (square) + logo 4 (wordmark, wide)
place(logos[4][2], 60,  560, 320, 320, logos[4][0])
place(logos[3][2], 440, 560, 700, 320, logos[3][0])

sheet.convert("RGB").save(os.path.join(OUT, "symstudio-logos.png"))
print("wrote", len(logos), "logos + contact sheet to", OUT)
