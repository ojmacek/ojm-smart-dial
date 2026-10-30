"""CANertia Studio widgets, custom-painted after the OJM Systems web style: graphite surfaces, fine borders, soft
white type, a single cobalt accent, short eased transitions. Each animated widget has animate(dt, now) -> bool; the
window calls it every frame and repaints only what moves."""
import math, time
from PySide6.QtCore import Qt, QRectF, QPointF, QSize
from PySide6.QtGui import QPainterPath, QPen, QColor
from PySide6.QtWidgets import QWidget, QSizePolicy
from studio_theme import *
import startup
from studio_paint import QtStartupPainter

class Reveal:
    """UI entry after the startup: groups fade in one after the other, rising 6 px."""
    at = None
    @classmethod
    def alpha(cls, group, now=None):
        if cls.at is None: return 0.
        now = time.monotonic() if now is None else now
        return smooth((now - cls.at - .07 * group) / .32)

class Painted(QWidget):
    group = 0
    def __init__(self, parent=None):
        super().__init__(parent); self.setMouseTracking(True); self.hover = Ease(0, .14); self._hover = 0.
    def enterEvent(self, e): self._hover = 1.; super().enterEvent(e)
    def leaveEvent(self, e): self._hover = 0.; super().leaveEvent(e)
    def animate(self, dt, now):
        moving = self.hover.step(self._hover, dt)
        return moving or (Reveal.at is not None and now - Reveal.at < 1.)
    def begin(self):
        p = painter(self); a = Reveal.alpha(self.group); p.setOpacity(a); p.translate(0, 6 * (1 - a)); return p

# ---------------------------------------------------------------- brand assets (unchanged artwork)
class Wordmark(Painted):
    def __init__(self, rel, height, color=TEXT, parent=None, group=0):
        super().__init__(parent); self.rel = rel; self.h = height; self.color = color; self.group = group
        pm = mask_pixmap(rel, color); self.ratio = pm.width() / pm.height()
        self.setFixedSize(round(height * self.ratio) + 2, height + 8)
    def paintEvent(self, e):
        p = self.begin(); pm = mask_pixmap(self.rel, self.color)
        p.drawPixmap(QRectF(1, 4, self.h * self.ratio, self.h), pm, QRectF(pm.rect())); p.end()

class OjmLogo(Painted):
    def __init__(self, height, color=TEXT_2, parent=None, group=0):
        super().__init__(parent); self.h = height; self.group = group; self.color = color
        img = gray_mask('assets/preview/ojm_logo.png', color); self.ratio = img.width() / img.height()
        self.setFixedSize(round(height * self.ratio) + 2, height + 4)
    def paintEvent(self, e):
        p = self.begin(); p.drawImage(QRectF(1, 2, self.h * self.ratio, self.h), gray_mask('assets/preview/ojm_logo.png', self.color)); p.end()

# ---------------------------------------------------------------- controls
class Button(Painted):
    """Lightly rounded. primary = solid cobalt; toggle = cobalt edge and tinted fill while on."""
    def __init__(self, label, on_click=None, toggle=False, primary=False, width=None, height=36, parent=None, group=0, px=12.5):
        super().__init__(parent); self.label = label; self.on_click = on_click; self.toggle = toggle; self.primary = primary
        self.checked = False; self.enabled = True; self.down = Ease(0, .08); self._down = False; self.on = Ease(0, .16)
        self.group = group; self.px = px
        self.setFixedHeight(height); self.setCursor(Qt.PointingHandCursor)
        if width: self.setFixedWidth(width)
        else: self.setMinimumWidth(72)
    def set_checked(self, v):
        if v != self.checked: self.checked = v; self.update()
    def set_label(self, s):
        if s != self.label: self.label = s; self.update()
    def set_enabled(self, v):
        if v != self.enabled: self.enabled = v; self.setCursor(Qt.PointingHandCursor if v else Qt.ArrowCursor); self.update()
    def mousePressEvent(self, e):
        if self.enabled and e.button() == Qt.LeftButton: self._down = True
    def mouseReleaseEvent(self, e):
        if self._down and self.enabled and self.rect().contains(e.position().toPoint()):
            if self.toggle: self.checked = not self.checked
            if self.on_click: self.on_click()
        self._down = False
    def animate(self, dt, now):
        a = super().animate(dt, now); b = self.down.step(1 if self._down else 0, dt); c = self.on.step(1 if self.checked else 0, dt)
        return a or b or c
    def paintEvent(self, e):
        p = self.begin(); r = QRectF(self.rect()).adjusted(.5, .5, -.5, -.5); h = self.hover.k; d = self.down.k; on = self.on.k
        if not self.enabled: p.setOpacity(p.opacity() * .4)
        if self.primary:
            fill = mix(ACCENT, rgb('#1A4DFF'), h * .6); fill = mix(fill, rgb('#002FD6'), d)
            p.setBrush(fill); p.setPen(Qt.NoPen); p.drawRoundedRect(r, 8, 8); color = TEXT
        else:
            fill = mix(mix(SURFACE_HI, rgb('#262D38'), h), ACCENT_SOFT, on); fill = mix(fill, FIELD, d * .5)
            p.setBrush(fill); p.setPen(QPen(mix(mix(BORDER, BORDER_HI, h), ACCENT, on), 1)); p.drawRoundedRect(r, 8, 8)
            color = mix(TEXT_2, TEXT, max(h, on))
        text(p, r, self.label, body(self.px, 500), color); p.end()

class StatusPill(Painted):
    """Connection state: a dot and a word."""
    def __init__(self, parent=None, group=0):
        super().__init__(parent); self.word = 'Offline'; self.live = False; self.k = Ease(0, .2); self.group = group
        self.setFixedSize(196, 36)
    def set(self, word, live):
        if (word, live) != (self.word, self.live): self.word = word; self.live = live; self.update()
    def animate(self, dt, now): a = super().animate(dt, now); return self.k.step(1 if self.live else 0, dt) or a
    def paintEvent(self, e):
        p = self.begin(); r = QRectF(self.rect()).adjusted(.5, .5, -.5, -.5); k = self.k.k
        p.setBrush(FIELD); p.setPen(QPen(BORDER, 1)); p.drawRoundedRect(r, 8, 8)
        dot(p, r.left() + 16, r.center().y(), 3.5, mix(DISABLED, ACCENT, k))
        text(p, r.adjusted(30, 0, -10, 0), self.word, body(12, 500), mix(TEXT_2, TEXT, k), Qt.AlignLeft | Qt.AlignVCenter); p.end()

class Segmented(Painted):
    """Options in a well; the chosen one sits on a cobalt-edged plate that glides with the Torus lead/trail springs.
    pending = sent, waiting for the ACK; locked options are dim and bounce the plate."""
    def __init__(self, labels, on_select=None, height=38, parent=None, group=0, px=12):
        super().__init__(parent); self.labels = list(labels); self.on_select = on_select; self.index = 0
        self.lead = Spring(0, 46, 1); self.trail = Spring(0, 30, 1); self.locked = {}; self.pending = False
        self.enabled = True; self.edge_at = -9.; self.edge_dir = 1; self.hover_i = -1; self.group = group; self.px = px
        self.setFixedHeight(height); self.setMinimumWidth(56 * len(labels)); self.setCursor(Qt.PointingHandCursor); self._fit = None
    def set_index(self, i, animate=True):
        if i != self.index:
            self.index = i
            if not animate: self.lead.snap(i); self.trail.snap(i)
            self.update()
    def set_locked(self, locked):
        if locked != self.locked: self.locked = dict(locked); self.update()
    def set_enabled(self, v):
        if v != self.enabled: self.enabled = v; self.setCursor(Qt.PointingHandCursor if v else Qt.ArrowCursor); self.update()
    def bounce(self, direction=1): self.edge_at = time.monotonic(); self.edge_dir = direction; self.update()
    def _at(self, x): return int(clamp(x / (self.width() / len(self.labels)), 0, len(self.labels) - 1))
    def mouseMoveEvent(self, e):
        i = self._at(e.position().x())
        if i != self.hover_i: self.hover_i = i; self.update()
    def leaveEvent(self, e): self.hover_i = -1; super().leaveEvent(e); self.update()
    def mouseReleaseEvent(self, e):
        if self.enabled and e.button() == Qt.LeftButton and self.on_select: self.on_select(self._at(e.position().x()))
    def animate(self, dt, now):
        a = super().animate(dt, now); e = now - self.edge_at
        bump = self.edge_dir * .14 * math.sin(math.pi * e / .16) if e < .16 else 0
        return self.lead.step(self.index + bump, dt) | self.trail.step(self.index, dt) | a | self.pending | (e < .25)
    def paintEvent(self, e):
        p = self.begin(); w, h = self.width(), self.height(); n = len(self.labels); cw = w / n
        if not self.enabled: p.setOpacity(p.opacity() * .45)
        p.setBrush(FIELD); p.setPen(QPen(BORDER, 1)); p.drawRoundedRect(QRectF(.5, .5, w - 1, h - 1), 9, 9)
        if self.enabled:
            lo, hi = min(self.lead.v, self.trail.v), max(self.lead.v, self.trail.v)
            plate = QRectF(lo * cw + 3, 3, (hi - lo) * cw + cw - 6, h - 6)
            edge = .55 + .45 * (.5 + .5 * math.sin(time.monotonic() * 7)) if self.pending else 1
            p.setBrush(ACCENT_SOFT); p.setPen(QPen(with_alpha(ACCENT, edge), 1)); p.drawRoundedRect(plate, 7, 7)
        if self._fit is None or self._fit[0] != round(cw):
            self._fit = (round(cw), fit(lambda s: body(s, 500), self.labels, cw - 14, self.px))
        f = self._fit[1]
        for i, s in enumerate(self.labels):
            r = QRectF(i * cw, 0, cw, h)
            if i in self.locked: color = DISABLED
            elif i == self.index and self.enabled: color = TEXT
            else: color = mix(TEXT_2, TEXT, .7 if i == self.hover_i and self.enabled else 0)
            text(p, r, s, f, color)
            if i in self.locked:
                p.setPen(QPen(DISABLED, 1)); p.drawLine(QPointF(r.center().x() - 8, h - 9), QPointF(r.center().x() + 8, h - 9))
        p.end()

# ---------------------------------------------------------------- speed
class SpeedReadout(Painted):
    def __init__(self, parent=None, group=0):
        super().__init__(parent); self.value = Spring(0, 9, 1); self.target = 0.; self.group = group; self.setFixedHeight(96)
        self.dim = Ease(0, .2); self.sending = True
    def animate(self, dt, now):
        a = super().animate(dt, now); return self.value.step(self.target, dt) | self.dim.step(0 if self.sending else 1, dt) | a
    def paintEvent(self, e):
        p = self.begin(); w, h = self.width(), self.height(); c = mix(TEXT, DISABLED, self.dim.k)
        text(p, QRectF(0, 0, w, 74), f'{max(0, self.value.v):.0f}', heading(66, 500), c)
        text(p, QRectF(0, 72, w, 20), 'km/h', body(12.5, 400, .4), TEXT_3); p.end()

class SpeedSlider(Painted):
    """0..250 km/h; a short tick marks where the Off lock starts (120 km/h)."""
    MAX = 250
    def __init__(self, on_change, lock_kmh=120, parent=None, group=0):
        super().__init__(parent); self.on_change = on_change; self.value = 0.; self.shown = Spring(0, 26, 1); self.lock = lock_kmh
        self.enabled = True; self.group = group; self.setFixedHeight(56); self.setCursor(Qt.PointingHandCursor); self.drag = False
    def set_value(self, v):
        v = clamp(v, 0, self.MAX)
        if v != self.value: self.value = v; self.update()
    def set_enabled(self, v):
        if v != self.enabled: self.enabled = v; self.update()
    def _x0(self): return 10
    def _x1(self): return self.width() - 10
    def _from_x(self, x): return clamp((x - self._x0()) / (self._x1() - self._x0()), 0, 1) * self.MAX
    def mousePressEvent(self, e):
        if self.enabled and e.button() == Qt.LeftButton: self.drag = True; self.on_change(round(self._from_x(e.position().x())))
    def mouseMoveEvent(self, e):
        super().mouseMoveEvent(e)
        if self.drag: self.on_change(round(self._from_x(e.position().x())))
    def mouseReleaseEvent(self, e): self.drag = False
    def wheelEvent(self, e):
        if self.enabled: self.on_change(clamp(round(self.value + (5 if e.angleDelta().y() > 0 else -5)), 0, self.MAX))
    def animate(self, dt, now): a = super().animate(dt, now); return self.shown.step(self.value, dt) | a
    def paintEvent(self, e):
        p = self.begin(); x0, x1, y = self._x0(), self._x1(), 18
        if not self.enabled: p.setOpacity(p.opacity() * .45)
        k = clamp(self.shown.v / self.MAX, 0, 1); xk = x0 + (x1 - x0) * k
        p.setPen(Qt.NoPen); p.setBrush(FIELD); p.drawRoundedRect(QRectF(x0 - 2, y - 3, x1 - x0 + 4, 6), 3, 3)
        p.setBrush(ACCENT); p.drawRoundedRect(QRectF(x0 - 2, y - 3, xk - x0 + 4, 6), 3, 3)
        for v in range(0, self.MAX + 1, 50):
            x = x0 + (x1 - x0) * v / self.MAX
            text(p, QRectF(x - 22, y + 14, 44, 16), str(v), body(10.5), TEXT_3)
        xl = x0 + (x1 - x0) * self.lock / self.MAX
        p.setPen(QPen(BORDER_HI, 1.2)); p.drawLine(QPointF(xl, y - 10), QPointF(xl, y - 6))
        p.setPen(QPen(with_alpha(SHADOW, .35), 3)); p.setBrush(Qt.NoBrush); p.drawEllipse(QPointF(xk, y + 1), 9, 9)
        p.setPen(QPen(BORDER_HI, 1)); p.setBrush(mix(TEXT, rgb('#FFFFFF'), self.hover.k)); p.drawEllipse(QPointF(xk, y), 8.5, 8.5)
        p.end()

# ---------------------------------------------------------------- the Torus face (what the device shows)
class TorusFace(Painted):
    """A small live Torus in a bezel: its black display, the halo ring, page marks, title and mode in Michroma, and
    (Performance) the car with its wing, the Launch / Boost ring and the start lights, as the Torus reports them."""
    TITLES = ('Active Aero', 'Rear Wheel Steering', 'Torque Vectoring')
    def __init__(self, kind, parent=None, group=0):
        super().__init__(parent); self.kind = kind; self.group = group
        self.title = ''; self.name = ''; self.prev = ('', ''); self.changed_at = -9.; self.page_pos = Spring(0, 24, 1); self.page = 0
        self.wing = Spring(25, 20, .6); self.wing_target = 25.; self.online = Ease(0, .3); self.is_online = False
        self.launch = False; self.launch_at = -9.; self.boost = False; self.boost_at = -9.; self.go_at = -9.; self.sub = ''
        self.dimmed = False
        self.setMinimumSize(60, 60); self.setMaximumHeight(340); self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.car = startup.Assets().paths if kind == 'performance' else None
    def set_text(self, title, name):
        if (title, name) != (self.title, self.name):
            self.prev = (self.title, self.name); self.title, self.name = title, name; self.changed_at = time.monotonic(); self.update()
    def set_actions(self, launch, boost):
        now = time.monotonic()
        if launch and not self.launch: self.launch_at = now
        if self.launch and not launch: self.go_at = now
        if boost and not self.boost: self.boost_at = now
        self.launch, self.boost = launch, boost
    def animate(self, dt, now):
        a = super().animate(dt, now)
        b = self.page_pos.step(self.page, dt) | self.wing.step(self.wing_target, dt) | self.online.step(1 if self.is_online else 0, dt)
        acting = self.launch or self.boost or now - self.go_at < .6
        return a | b | acting | (now - self.changed_at < .3)
    def sizeHint(self): return QSize(240, 240)
    def paintEvent(self, e):
        p = self.begin(); w, h = self.width(), self.height(); d = min(w, h) - 6; cx, cy = w / 2, h / 2
        if d < 72: p.end(); return  # too small to read: the rows below carry everything
        # Bezel: the device, softly lifted from the card.
        for i, a in ((2, .18), (5, .08)): dot(p, cx, cy + i * .7, d / 2 + i * .4, with_alpha(SHADOW, a))
        dot(p, cx, cy, d / 2, rgb('#0E1115')); p.setPen(QPen(BORDER, 1)); p.setBrush(Qt.NoBrush); p.drawEllipse(QPointF(cx, cy), d / 2, d / 2)
        inner = d - 14; s = inner / 466
        p.translate(cx - inner / 2, cy - inner / 2); p.scale(s, s)
        now = time.monotonic(); on = self.online.k
        dot(p, 233, 233, 233, FACE)
        arc(p, 233, 233, 226, 0, 360, 1.4, mix(RING, RING_ON, on))
        a = 0; alpha = 0
        if self.launch: a = 180 * smooth((now - self.launch_at) / .25); alpha = 1
        elif self.boost: a = 180 * clamp(1 - (now - self.boost_at - .3) / 9.7, 0, 1); alpha = 1
        elif now - self.go_at < .45: a = 180; alpha = 1 - smooth((now - self.go_at) / .45)
        if a > 0 and alpha > 0: arc(p, 233, 233, 226, 270 - a, 270 + a, 3.2, with_alpha(DIAL_BLUE, alpha))
        for i in range(3): arc(p, 233, 233, 214, 270 + (i - 1) * 11 - 3.5, 270 + (i - 1) * 11 + 3.5, 3, MARK)
        if self.kind == 'performance':
            pa = 270 + (self.page_pos.v - 1) * 11; arc(p, 233, 233, 214, pa - 3.5, pa + 3.5, 3, with_alpha(DIAL_WHITE, on))
        k = smooth((now - self.changed_at) / .24); tcol = DISABLED if self.dimmed else mix(DISABLED, DIAL_WHITE, on)
        def texts(title, name, alpha):
            if alpha <= 0: return
            text(p, QRectF(40, 62, 386, 40), title, dial(21), with_alpha(tcol, alpha))
            text(p, QRectF(20, 318, 426, 60), name, dial(40), with_alpha(tcol, alpha))
        if k < 1: texts(*self.prev, 1 - smooth(k * 2))
        texts(self.title, self.name, smooth(k * 2 - 1) if k < 1 else 1)
        if self.sub: text(p, QRectF(40, 380, 386, 30), self.sub, dial(15), with_alpha(DIAL_BLUE, on))
        if self.car:
            self._car(p, on)
            if self.launch:  # start lights, lit one by one
                for i in range(3):
                    l = smooth((now - self.launch_at - .22 - .16 * i) / .07); dot(p, 233 + (i - 1) * 22, 142, 3.5, mix(SEG_OFF, DIAL_WHITE, l))
        p.end()
    def _car(self, p, on):
        color = with_alpha(DIAL_WHITE, .9 * max(.35, on))
        for path in self.car:
            pp = QPainterPath(QPointF(*path[0]))
            for x, y in path[1:]: pp.lineTo(x, y)
            pen = QPen(color, 1.7); pen.setCapStyle(Qt.RoundCap); pen.setJoinStyle(Qt.RoundJoin); p.setPen(pen); p.setBrush(Qt.NoBrush); p.drawPath(pp)
        startup.wing(QtStartupPainter(p), 1, 0, 0, self.wing.v, max(.35, on), 0)

# ---------------------------------------------------------------- startup overlay
class StartupOverlay(QWidget):
    """The Torus startup over the whole window; a click or Esc skips it; the UI is revealed at its end."""
    def __init__(self, parent, on_done):
        super().__init__(parent); self.s = startup.Startup(startup.Assets()); self.started = None; self.on_done = on_done
        self.fade_at = None; self.setCursor(Qt.PointingHandCursor)
    def start(self): self.started = time.monotonic(); self.show(); self.raise_()
    def skip(self):
        if self.fade_at is None: self.fade_at = time.monotonic(); self.on_done()
    def mousePressEvent(self, e): self.skip()
    def elapsed(self): return 0 if self.started is None else (time.monotonic() - self.started) * 1000
    def animate(self, dt, now):
        if not self.isVisible(): return False
        if self.fade_at is None and self.elapsed() >= self.s.total + 250: self.skip()
        if self.fade_at is not None and now - self.fade_at > .45: self.hide(); return False
        return True
    def paintEvent(self, e):
        p = painter(self); w, h = self.width(), self.height()
        fade = 1 - smooth((time.monotonic() - self.fade_at) / .4) if self.fade_at is not None else 1
        p.setOpacity(fade); p.fillRect(self.rect(), BG)
        d = min(w * .6, h * .74); s = d / 466; oy = -h * .03
        p.save(); p.translate(w / 2 - 233 * s, h / 2 - 233 * s + oy); p.scale(s, s)
        self.s.paint(QtStartupPainter(p), min(self.elapsed(), self.s.total), 25, {}); p.restore()
        e2 = self.elapsed(); k = smooth((e2 - self.s.fill_start - 300) / 450) * (1 - smooth((e2 - self.s.hold_end) / 250))
        if k > 0:  # the product wordmark under the logo while it is held
            pm = mask_pixmap('assets/studio/canertia_studio.png', TEXT); hh = 22 * s; ww = hh * pm.width() / pm.height()
            p.setOpacity(fade * k); p.drawPixmap(QRectF(w / 2 - ww / 2, h / 2 + 128 * s + oy, ww, hh), pm, QRectF(pm.rect()))
        p.end()
