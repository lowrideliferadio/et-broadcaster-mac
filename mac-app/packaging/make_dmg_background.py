"""Builds the drag-to-Applications DMG window background (660x420 + @2x) from the REAL brand lockup in /workspace/brand.
Icon slots used by build-dmg.sh: app at (170,235), Applications link at (490,235)."""
from PIL import Image, ImageDraw, ImageFilter, ImageFont
import random, os, sys
BRAND = sys.argv[1] if len(sys.argv) > 1 else '/workspace/brand/et-tattoo-lockup.png'
FONTS = os.path.join(os.path.dirname(__file__), '..', '..', 'dashboard', 'assets', 'fonts')
def bg(scale):
    W, H = 660 * scale, 420 * scale
    im = Image.new('RGB', (W, H)); d = ImageDraw.Draw(im); random.seed(4)
    for y in range(H):
        v = 18 + int(6 * random.random()) + int(10 * (1 - y / H)); d.line([(0, y), (W, y)], fill=(v, v - 2, v + 6))
    im = im.filter(ImageFilter.GaussianBlur(0.6 * scale))
    glow = Image.new('L', (W, H), 0); ImageDraw.Draw(glow).ellipse([W * .15, H * .55, W * .85, H * 1.25], fill=120)
    im = Image.composite(Image.new('RGB', (W, H), (60, 40, 140)), im, glow.filter(ImageFilter.GaussianBlur(60 * scale)))
    d = ImageDraw.Draw(im)
    lg = Image.open(BRAND).convert('RGBA'); lg.thumbnail((int(300 * scale), int(90 * scale)), Image.LANCZOS)
    im.paste(lg, ((W - lg.width) // 2, int(22 * scale)), lg)
    f = ImageFont.truetype(os.path.join(FONTS, 'Orbitron.ttf'), 13 * scale); f2 = ImageFont.truetype(os.path.join(FONTS, 'Rajdhani-Bold.ttf'), 15 * scale)
    y = 235 * scale
    for i in range(3):
        x0 = (265 + i * 46) * scale; d.polygon([(x0, y - 10 * scale), (x0 + 22 * scale, y), (x0, y + 10 * scale)], fill=(120 + i * 40, 170 + i * 25, 255))
    d.text((W // 2, 330 * scale), 'DRAG ET BROADCASTER INTO APPLICATIONS', font=f, fill=(200, 210, 235), anchor='mm')
    d.text((W // 2, 356 * scale), 'First launch: right-click the app, then choose Open (unsigned personal build)', font=f2, fill=(150, 156, 175), anchor='mm')
    d.rectangle([2 * scale, 2 * scale, W - 3 * scale, H - 3 * scale], outline=(60, 62, 72), width=scale)
    return im
out = os.path.dirname(os.path.abspath(__file__))
bg(1).save(os.path.join(out, 'dmg-background.png')); bg(2).save(os.path.join(out, 'dmg-background@2x.png')); print('ok')
