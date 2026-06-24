"""SymStudio brand assets: crisp multi-resolution .ico (bold S at small sizes,
aperture at large), a high-res aperture PNG, a 1200x630 OG image, and a true
vector SVG logo for the web. All rasters are supersampled (rendered at SSx then
downscaled with LANCZOS) for smooth, anti-aliased edges."""
import os, math
from PIL import Image, ImageDraw, ImageFont

A = r"C:\Users\skyea\claude\SymStudio\assets"
CYAN   = (0, 229, 255, 255)
PURPLE = (150, 90, 255, 255)
WHITE  = (245, 248, 252, 255)
SS = 4  # supersample factor — render big, downscale smooth

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

def _hex_pts(P):
    """Geometry shared by the raster and SVG aperture so they match exactly."""
    cx = cy = P/2
    R, rin = P*0.33, P*0.19
    pin = [(cx+rin*math.cos(math.radians(60*i-90)), cy+rin*math.sin(math.radians(60*i-90)))
           for i in range(6)]
    segs = []
    for i in range(6):
        t = i/5
        col = tuple(int(CYAN[k]*(1-t)+PURPLE[k]*t) for k in range(3))
        nxt = pin[(i+1) % 6]
        out = (cx+R*math.cos(math.radians(60*(i+1)-90)), cy+R*math.sin(math.radians(60*(i+1)-90)))
        segs.append((pin[i], nxt, col))
        segs.append((nxt, out, col))
    return cx, cy, R, segs

def mono(px):
    """Crisp bold cyan S on dark rounded square — legible at tiny sizes."""
    P = px * SS
    img = panel(P)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([2*SS, 2*SS, P-3*SS, P-3*SS], int(94*P/512),
                        outline=(45, 80, 105, 255), width=max(1, int(3*P/512)))
    d.text((P/2, P/2 - P*0.02), "S", font=font(int(P*0.74)), fill=CYAN, anchor="mm")
    return img.resize((px, px), Image.LANCZOS)

def aperture(px):
    """Clean aperture iris + S, supersampled for smooth anti-aliased edges."""
    P = px * SS
    img = panel(P)
    d = ImageDraw.Draw(img)
    cx, cy, R, segs = _hex_pts(P)
    w = max(2, int(P*0.05))
    for a, b, col in segs:
        d.line([a, b], fill=col + (255,), width=w, joint="curve")
    d.ellipse([cx-R-w, cy-R-w, cx+R+w, cy+R+w], outline=(120, 100, 175, 255),
              width=max(1, int(P*0.012)))
    d.text((cx, cy), "S", font=font(int(P*0.30)), fill=WHITE, anchor="mm")
    return img.resize((px, px), Image.LANCZOS)

def svg(px=512):
    """Vector aperture logo — razor-sharp at any size for the web."""
    cx, cy, R, segs = _hex_pts(px)
    w = px*0.05
    rad = int(96*px/512)
    lines = "".join(
        f'<line x1="{a[0]:.1f}" y1="{a[1]:.1f}" x2="{b[0]:.1f}" y2="{b[1]:.1f}" '
        f'stroke="#{c[0]:02x}{c[1]:02x}{c[2]:02x}" stroke-width="{w:.1f}" stroke-linecap="round"/>'
        for a, b, c in segs)
    fsz = px*0.30
    return f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {px} {px}" width="{px}" height="{px}">
<defs><linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
<stop offset="0" stop-color="#100c1e"/><stop offset="1" stop-color="#07060e"/></linearGradient></defs>
<rect x="0" y="0" width="{px}" height="{px}" rx="{rad}" fill="url(#bg)"/>
<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{R+w:.1f}" fill="none" stroke="#7864af" stroke-width="{px*0.012:.1f}"/>
{lines}
<text x="{cx:.1f}" y="{cy:.1f}" font-family="Arial, Segoe UI, sans-serif" font-weight="bold" font-size="{fsz:.0f}" fill="#f5f8fc" text-anchor="middle" dominant-baseline="central">S</text>
</svg>'''

# Multi-resolution ICO: mono S at 16/32/48, aperture at 64/128/256 (all supersampled)
frames = [aperture(256), aperture(128), aperture(64), mono(48), mono(32), mono(16)]
sizes  = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
frames[0].save(os.path.join(A, "symstudio-icon.ico"), format="ICO",
               sizes=sizes, append_images=frames[1:])

aperture(256).save(os.path.join(A, "symstudio-window.png"))
mono(256).save(os.path.join(A, "symstudio-mono.png"))
aperture(1024).save(os.path.join(A, "symstudio-logo.png"))      # hi-res raster logo
with open(os.path.join(A, "symstudio-logo.svg"), "w", encoding="utf-8") as f:
    f.write(svg(512))                                            # vector logo (web)

# Social / OG preview image (1200x630): dark card + aperture + wordmark + tagline
def og():
    W, H = 1200, 630
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    img.paste(vgrad(H, (16, 12, 30), (7, 6, 14)).resize((W, H)), (0, 0))
    d = ImageDraw.Draw(img)
    img.alpha_composite(aperture(300), (110, (H - 300) // 2))
    d.text((460, 232), "SymStudio", font=font(86), fill=WHITE, anchor="lm")
    d.text((462, 322), "Everything you need to go live.", font=font(40), fill=CYAN, anchor="lm")
    d.text((462, 384), "Free, open streaming studio - based on OBS Studio",
           font=font(28), fill=(150, 163, 178, 255), anchor="lm")
    img.convert("RGB").save(os.path.join(A, "symstudio-og.png"))

og()
print("ico frames:", sizes, "| supersample:", SS, "| +svg +hi-res png")
