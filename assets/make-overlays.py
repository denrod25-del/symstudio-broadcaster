"""Generate SymStudio starter-scene overlay PNGs (Midnight Cyan style, supersampled)."""
import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

OUT = os.path.join(os.path.dirname(__file__), "..", "frontend", "data", "symstudio-overlays")
os.makedirs(OUT, exist_ok=True)
S = 2  # supersample
W, H = 1920, 1080

def font(sz, heading=True):
    for c in (["bahnschrift.ttf", "segoeuib.ttf", "arialbd.ttf"] if heading else ["segoeui.ttf", "arial.ttf"]):
        try: return ImageFont.truetype(c, sz * S)
        except OSError: pass
    return ImageFont.load_default()

def Hx(s):
    s = s.lstrip("#"); return tuple(int(s[i:i+2], 16) for i in (0, 2, 4)) + (255,)

def backdrop(title, subtitle):
    img = Image.new("RGBA", (W*S, H*S))
    d = ImageDraw.Draw(img)
    # vertical navy gradient
    for y in range(H*S):
        t = y / (H*S - 1)
        c = tuple(int(a*(1-t)+b*t) for a, b in zip((17, 21, 31), (8, 10, 16)))
        d.line([(0, y), (W*S, y)], fill=c + (255,))
    # cyan glow blobs (blurred ellipses)
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([int(-300*S), int(700*S), int(700*S), int(1400*S)], fill=(0, 229, 255, 60))
    gd.ellipse([int(1400*S), int(-300*S), int(2300*S), int(500*S)], fill=(0, 168, 200, 50))
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(120*S)))
    # thin cyan frame inset
    m = int(48*S)
    d.rounded_rectangle([m, m, W*S-m, H*S-m], int(24*S), outline=(0, 229, 255, 160), width=int(3*S))
    # title + subtitle
    f1, f2 = font(150), font(40, False)
    tw = d.textlength(title, font=f1)
    d.text(((W*S - tw)/2, int(380*S)), title, font=f1, fill=Hx("#E6F0F5"))
    sw = d.textlength(subtitle, font=f2)
    d.text(((W*S - sw)/2, int(580*S)), subtitle, font=f2, fill=Hx("#7DF8FF"))
    return img.resize((W, H), Image.LANCZOS)

backdrop("STARTING SOON", "stream starts in a moment").save(os.path.join(OUT, "bg-starting-soon.png"))
backdrop("BE RIGHT BACK", "hold tight").save(os.path.join(OUT, "bg-brb.png"))
backdrop("THANKS FOR WATCHING", "see you next stream").save(os.path.join(OUT, "bg-ending.png"))

# webcam frame: transparent, beveled cyan corners, 640x400
fw, fh, t = 640, 400, 5
frame = Image.new("RGBA", (fw*S, fh*S), (0, 0, 0, 0))
fd = ImageDraw.Draw(frame)
fd.rounded_rectangle([t*S, t*S, (fw-t)*S, (fh-t)*S], 14*S, outline=Hx("#00E5FF"), width=t*S)
fd.rounded_rectangle([(t+4)*S, (t+4)*S, (fw-t-4)*S, (fh-t-4)*S], 11*S, outline=(0, 120, 140, 160), width=2*S)
frame.resize((fw, fh), Image.LANCZOS).save(os.path.join(OUT, "webcam-frame.png"))

# info bar: slim translucent top bar, 1920x72
bar = Image.new("RGBA", (W*S, 72*S), (0, 0, 0, 0))
bd = ImageDraw.Draw(bar)
bd.rounded_rectangle([8*S, 8*S, (W-8)*S, 64*S], 12*S, fill=(13, 17, 27, 210), outline=(0, 229, 255, 120), width=1*S)
bd.text((28*S, 20*S), "SYMSTUDIO", font=font(26), fill=Hx("#00E5FF"))
bar.resize((W, 72), Image.LANCZOS).save(os.path.join(OUT, "info-bar.png"))
print("wrote 5 overlays to", OUT)
