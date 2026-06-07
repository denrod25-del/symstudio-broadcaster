"""SymStudio icon v2: crisp multi-resolution .ico (bold S at small sizes, clean
aperture at large) + a glow-free aperture window PNG that downsizes cleanly."""
import os, math
from PIL import Image, ImageDraw, ImageFont

A = r"C:\Users\skyea\claude\SymStudio\assets"
CYAN   = (0, 229, 255, 255)
PURPLE = (150, 90, 255, 255)
WHITE  = (245, 248, 252, 255)

def font(sz):
    for c in ("arialbd.ttf", "segoeuib.ttf", "bahnschrift.ttf"):
        try: return ImageFont.truetype(c, sz)
        except OSError: pass
    return ImageFont.load_default()

def vgrad(px, top, bot):
    g = Image.new("RGBA", (1, px))
    for y in range(px):
        t = y / max(1, px-1)
        g.putpixel((0, y), tuple(int(top[i]*(1-t)+bot[i]*t) for i in range(3)) + (255,))
    return g.resize((px, px))

def panel(px):
    img = Image.new("RGBA", (px, px), (0, 0, 0, 0))
    m = Image.new("L", (px, px), 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, px-1, px-1], int(96*px/512), fill=255)
    img.paste(vgrad(px, (16, 12, 30), (7, 6, 14)), (0, 0), m)
    return img

def mono(px):
    """Crisp bold cyan S on dark rounded square — legible at tiny sizes."""
    img = panel(px)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([2, 2, px-3, px-3], int(94*px/512),
                        outline=(45, 80, 105, 255), width=max(1, int(3*px/512)))
    d.text((px/2, px/2 - px*0.02), "S", font=font(int(px*0.74)), fill=CYAN, anchor="mm")
    return img

def aperture(px):
    """Clean, glow-free aperture iris + S; high contrast so it downsizes well."""
    img = panel(px)
    d = ImageDraw.Draw(img)
    cx = cy = px/2
    R, rin, w = px*0.33, px*0.19, max(2, int(px*0.05))
    pin = [(cx+rin*math.cos(math.radians(60*i-90)), cy+rin*math.sin(math.radians(60*i-90)))
           for i in range(6)]
    for i in range(6):
        t = i/5
        col = tuple(int(CYAN[k]*(1-t)+PURPLE[k]*t) for k in range(3)) + (255,)
        nxt = pin[(i+1) % 6]
        out = (cx+R*math.cos(math.radians(60*(i+1)-90)), cy+R*math.sin(math.radians(60*(i+1)-90)))
        d.line([pin[i], nxt], fill=col, width=w)
        d.line([nxt, out], fill=col, width=w)
    d.ellipse([cx-R-3, cy-R-3, cx+R+3, cy+R+3], outline=(120, 100, 175, 255),
              width=max(1, int(px*0.012)))
    d.text((cx, cy), "S", font=font(int(px*0.30)), fill=WHITE, anchor="mm")
    return img

# Multi-resolution ICO: mono S at 16/32/48, clean aperture at 64/128/256
frames = [aperture(256), aperture(128), aperture(64), mono(48), mono(32), mono(16)]
sizes  = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
frames[0].save(os.path.join(A, "symstudio-icon.ico"), format="ICO",
               sizes=sizes, append_images=frames[1:])

# Window/taskbar icon (single PNG Qt scales): clean aperture, plus a mono fallback
aperture(256).save(os.path.join(A, "symstudio-window.png"))
mono(256).save(os.path.join(A, "symstudio-mono.png"))
print("ico frames:", sizes)
