"""CANertia Studio look, after the OJM Systems web: graphite, soft white, one cobalt accent used sparingly.

Headings Clash Display, everything else Sora; Michroma (the Torus firmware font) only where Studio shows a Torus.
Fonts are loaded privately for this process. Clash Display (ITF Free Font License) may be used in the app but not
redistributed as a file, so release packages leave it out; without it the headings fall back to Sora.
"""
from pathlib import Path
import math, sys
from PySide6.QtCore import Qt, QRectF, QPointF
from PySide6.QtGui import QColor, QFont, QFontDatabase, QFontMetricsF, QPainter, QPen, QPixmap, QImage

ROOT = Path(getattr(sys, '_MEIPASS', Path(__file__).resolve().parents[1]))

def rgb(h, a=255):
    c = QColor(h); c.setAlpha(a); return c

# ---- tokens (web palette)
BG = rgb('#14171C')            # graphite background
SURFACE = rgb('#1B2028')       # cards
SURFACE_HI = rgb('#212731')    # raised / hover inside a card
FIELD = rgb('#161A20')         # wells: selectors, slider track, log
BORDER = rgb('#2A3039')        # fine borders
BORDER_HI = rgb('#3A424D')
TEXT = rgb('#F3F1EB')          # soft white
TEXT_2 = rgb('#B4BDC9')        # secondary text
TEXT_3 = rgb('#7F8996')        # tertiary: captions, units
DISABLED = rgb('#59616D')
ACCENT = rgb('#0038FF')        # cobalt
ACCENT_SOFT = rgb('#18244C')   # cobalt-tinted fill for a selected option
ACCENT_INK = rgb('#7D95FF')    # cobalt readable as small text on graphite
SHADOW = rgb('#000000')
# ---- the Torus face (what the device shows)
FACE = rgb('#000000'); RING = rgb('#1A1F27'); RING_ON = rgb('#1B2A5C'); MARK = rgb('#262C35'); DIAL_WHITE = rgb('#F2F1EC')
DIAL_BLUE = rgb('#2E5BFF'); SEG_OFF = rgb('#262A31')

HEADING = 'Sora'; BODY = 'Sora'; DIAL = 'Michroma'
_loaded = False
def _add(rel):
    path = ROOT / rel
    if not path.exists(): return None
    fams = QFontDatabase.applicationFontFamilies(QFontDatabase.addApplicationFont(str(path)))
    return fams[0] if fams else None
def load_fonts():
    global _loaded, HEADING, BODY, DIAL
    if _loaded: return
    DIAL = _add('assets/fonts/Michroma-Regular.ttf') or DIAL
    BODY = _add('assets/fonts/Sora-Variable.ttf') or 'Segoe UI'
    clash = None
    for name in ('Regular', 'Medium', 'Semibold'): clash = _add(f'assets/fonts/ClashDisplay-{name}.otf') or clash
    HEADING = clash or BODY
    _loaded = True

def _font(family, px, weight, spacing):
    f = QFont(family); f.setPixelSize(max(1, round(px))); f.setWeight(QFont.Weight(weight)); f.setHintingPreference(QFont.PreferNoHinting)
    if spacing: f.setLetterSpacing(QFont.AbsoluteSpacing, spacing)
    return f
def heading(px, weight=500, spacing=0.): return _font(HEADING, px, weight, spacing)
def body(px, weight=400, spacing=0.): return _font(BODY, px, weight, spacing)
def dial(px, spacing=0.): return _font(DIAL, px, 400, spacing)
def label(px=10.5): return body(px, 600, 1.1)  # small uppercase labels

_mono_family = None
def mono(px):
    global _mono_family
    if _mono_family is None:
        _mono_family = 'Consolas'
        if 'Consolas' not in QFontDatabase.families():  # e.g. the offscreen test platform: load it from Windows
            path = Path('C:/Windows/Fonts/consola.ttf')
            if path.exists():
                fams = QFontDatabase.applicationFontFamilies(QFontDatabase.addApplicationFont(str(path)))
                if fams: _mono_family = fams[0]
    f = QFont(_mono_family); f.setPixelSize(px); f.setStyleHint(QFont.Monospace); return f

def fit(make, labels, width, px, floor=.72):
    """make(px) -> QFont; the largest size (down to floor x px) at which every label fits the width."""
    size = px
    while size > px * floor:
        f = make(size); fm = QFontMetricsF(f)
        if max(fm.horizontalAdvance(l) for l in labels) <= width: return f
        size -= .25
    return make(size)

def with_alpha(c, a):
    c = QColor(c); c.setAlphaF(max(0., min(1., c.alphaF() * a))); return c

def mix(a, b, k):
    k = max(0., min(1., k))
    return QColor(round(a.red() + (b.red() - a.red()) * k), round(a.green() + (b.green() - a.green()) * k),
                  round(a.blue() + (b.blue() - a.blue()) * k), round(a.alpha() + (b.alpha() - a.alpha()) * k))

def clamp(v, lo, hi): return lo if v < lo else hi if v > hi else v
def smooth(t): t = clamp(t, 0., 1.); return t * t * (3 - 2 * t)

class Spring:
    """The Torus spring (HaloUi.h ui_spring): four sub-steps, snaps when settled."""
    def __init__(self, value=0., w=20., zeta=1.):
        self.v = float(value); self.vel = 0.; self.w = w; self.zeta = zeta
    def step(self, target, dt):
        for _ in range(4):
            h = dt / 4; a = self.w * self.w * (target - self.v) - 2 * self.zeta * self.w * self.vel
            self.vel += a * h; self.v += self.vel * h
        if abs(target - self.v) < .0005 and abs(self.vel) < .005: self.v = float(target); self.vel = 0.; return False
        return True
    def snap(self, value): self.v = float(value); self.vel = 0.

class Ease:
    """A short eased transition between two values (for colours and opacities): ~180 ms."""
    def __init__(self, value=0., seconds=.18): self.v = float(value); self.target = float(value); self.speed = 1 / seconds
    def step(self, target, dt):
        self.target = target
        if self.v == target: return False
        d = self.speed * dt; self.v = target if abs(target - self.v) <= d else self.v + math.copysign(d, target - self.v)
        return True
    @property
    def k(self): return smooth(self.v)

def painter(device):
    p = QPainter(device)
    p.setRenderHints(QPainter.Antialiasing | QPainter.TextAntialiasing | QPainter.SmoothPixmapTransform)
    return p

def text(p, rect, s, f, color, align=Qt.AlignCenter):
    p.setFont(f); p.setPen(color); p.drawText(rect, align, s)

def card(p, r, radius=12, fill=SURFACE, border=BORDER, shadow=True):
    """A surface with a fine border and a soft, low shadow (a few stacked translucent layers, no blur pass)."""
    if shadow:
        p.setPen(Qt.NoPen)
        for i, a in ((1, .22), (3, .12), (6, .07), (10, .04)):
            p.setBrush(with_alpha(SHADOW, a)); p.drawRoundedRect(r.adjusted(-i * .3, i * .6, i * .3, i * 1.1), radius + i * .5, radius + i * .5)
    p.setBrush(fill); p.setPen(QPen(border, 1)); p.drawRoundedRect(r, radius, radius)

def arc(p, cx, cy, r, a0, a1, width, color):
    """Arc in the Torus convention: degrees clockwise from 3 o'clock (screen y down)."""
    if a1 - a0 <= .01 or color.alpha() == 0: return
    pen = QPen(color, width); pen.setCapStyle(Qt.RoundCap); p.setPen(pen); p.setBrush(Qt.NoBrush)
    rect = QRectF(cx - r, cy - r, 2 * r, 2 * r)
    if a1 - a0 >= 359.99: p.drawEllipse(rect); return
    p.drawArc(rect, round(-a0 * 16), round(-(a1 - a0) * 16))

def dot(p, x, y, r, color):
    if color.alpha() == 0: return
    p.setPen(Qt.NoPen); p.setBrush(color); p.drawEllipse(QPointF(x, y), r, r)

_masks = {}
def mask_pixmap(rel, color):
    """A white-on-transparent asset (wordmarks) tinted once and cached."""
    key = (rel, color.rgba())
    if key not in _masks:
        img = QImage(str(ROOT / rel)).convertToFormat(QImage.Format_ARGB32_Premultiplied)
        if img.isNull(): raise RuntimeError(f'missing asset {rel}')
        tinted = QImage(img.size(), QImage.Format_ARGB32_Premultiplied); tinted.fill(Qt.transparent)
        q = QPainter(tinted); q.drawImage(0, 0, img); q.setCompositionMode(QPainter.CompositionMode_SourceIn)
        q.fillRect(tinted.rect(), color); q.end()
        _masks[key] = QPixmap.fromImage(tinted)
    return _masks[key]

def gray_mask(rel, color=None):
    """An 8-bit coverage image (black background) turned into a tinted alpha image once."""
    key = ('gray', rel, color.rgba() if color else None)
    if key not in _masks:
        img = QImage(str(ROOT / rel)).convertToFormat(QImage.Format_Grayscale8)
        alpha = QImage(img.size(), QImage.Format_ARGB32_Premultiplied); alpha.fill(color or Qt.white)
        alpha.setAlphaChannel(img)
        _masks[key] = alpha
    return _masks[key]
