"""CANertia Studio (V6.0.7): plays the car for the Torus dials and mirrors them over CAN.

Studio sends the car state (0x100) and, until Torus 3 exists, the Drive Torus Drive Mode (0x202); it mirrors and
remote-controls the Performance Torus (0x201/0x301/0x211). The Torus is the authority: Studio shows what it reports.
"""
from pathlib import Path
import argparse, html, math, sys, time
from collections import deque
from dataclasses import replace
from PySide6.QtCore import Qt, QTimer, QRectF, QPointF
from PySide6.QtGui import QPen, QKeySequence, QShortcut
from PySide6.QtWidgets import (QApplication, QMainWindow, QWidget, QHBoxLayout, QVBoxLayout, QComboBox, QPlainTextEdit,
                               QFileDialog, QSizePolicy)
from serial.tools import list_ports
from protocol import *
from can_link import CanWorker, EventMailbox, DemoBus
from torus_rules import TorusRules, TUNING
from scenario import ScenarioRunner, STEPS
from studio_theme import *
from studio_widgets import *

VERSION = '6.0.7'
TITLES = TorusFace.TITLES
ROWS = ((AERO, 'Active Aero', 'Aero'), (RWS, 'Rear Wheel Steering', 'RWS'), (TV, 'Torque Vectoring', 'TV'))  # the Torus page order
WING = {0: 12, 1: 0, 2: 25, 3: 58}  # Aero value -> wing angle (0 = Auto at rest)
def title_case(s): return ' '.join(w.capitalize() for w in s.split())
def vis_of(fw_page): return (fw_page + 1) % 3

class VehicleModel:
    """The car Studio plays. Simplified on purpose: a state and a target speed the car ramps to (accelerating
    and braking at fixed rates). A throttle/brake physics model can replace step() later without touching the UI."""
    def __init__(self): self.state = PARK; self.target = 0.; self.speed = 0.; self.brake = False
    def step(self, dt):
        goal = self.target if self.state == DRIVE_STATE else 0.
        rate = 45. if goal > self.speed else (130. if self.brake else 70.)
        self.speed = goal if abs(goal - self.speed) <= rate * dt else self.speed + math.copysign(rate * dt, goal - self.speed)
    def frame(self): return Vehicle(self.state, round(self.speed, 1), 80 if self.brake else 0)

# ---------------------------------------------------------------- panels
class Panel(Painted):
    """A card: fine border, soft shadow, a header (Clash Display title left, status right) over a hairline."""
    HEAD = 60
    def __init__(self, title, group=0, parent=None, header=True):
        super().__init__(parent); self.title = title; self.status = ''; self.status_live = False; self.group = group; self.header = header
        self.lay = QVBoxLayout(self); self.lay.setContentsMargins(24, self.HEAD + 12 if header else 20, 24, 22); self.lay.setSpacing(8)
        self.live = Ease(0, .2); self.header_widget = None
    def resizeEvent(self, e):
        super().resizeEvent(e)
        if self.header_widget: self.header_widget.move(self.width() - 24 - self.header_widget.width(), (self.HEAD - self.header_widget.height()) // 2 + 2)
    def set_status(self, text, live):
        if (text, live) != (self.status, self.status_live): self.status, self.status_live = text, live; self.update()
    def animate(self, dt, now): a = super().animate(dt, now); return self.live.step(1 if self.status_live else 0, dt) | a
    def paintEvent(self, e):
        p = self.begin(); r = QRectF(self.rect()).adjusted(6, 2, -6, -10); card(p, r)
        if self.header:
            text(p, QRectF(r.left() + 20, r.top(), r.width() - 40, self.HEAD), self.title, heading(19, 500), TEXT, Qt.AlignLeft | Qt.AlignVCenter)
            if self.status:
                f = body(11.5, 500); p.setFont(f); tw = p.fontMetrics().horizontalAdvance(self.status)
                p.setFont(heading(19, 500)); title_w = p.fontMetrics().horizontalAdvance(self.title)
                if 20 + title_w + 36 + tw > r.width() - 20: tw = 0  # no room: the dot alone
                x = r.right() - 20 - tw; k = self.live.k
                dot(p, x - (12 if tw else 4), r.top() + self.HEAD / 2, 3.5, mix(DISABLED, ACCENT, k))
                if tw: text(p, QRectF(x, r.top(), tw + 2, self.HEAD), self.status, f, mix(TEXT_3, TEXT_2, k), Qt.AlignLeft | Qt.AlignVCenter)
            p.setPen(QPen(BORDER, 1)); p.drawLine(QPointF(r.left() + 20, r.top() + self.HEAD), QPointF(r.right() - 20, r.top() + self.HEAD))
        p.end()

class RowLabel(Painted):
    """Function name left (small caps); on the right what Auto picks, a lock, a refusal or 'Sending'."""
    def __init__(self, label, group=0, parent=None, short=None):
        super().__init__(parent); self.label = label; self.short = short or label; self.note = ''; self.note_color = TEXT_3
        self.note_until = None; self.group = group; self.setFixedHeight(22)
    def set_note(self, note, color=None, seconds=None):
        color = color or ACCENT_INK; until = time.monotonic() + seconds if seconds else None
        if seconds is None and self.note_until is not None and time.monotonic() < self.note_until: return  # a flash wins
        if seconds or (note, color.rgba()) != (self.note, self.note_color.rgba()):
            self.note, self.note_color, self.note_until = note, color, until; self.update()
    def animate(self, dt, now):
        a = super().animate(dt, now)
        if self.note_until is not None and now >= self.note_until: self.note_until = None; self.note = ''; return True
        return a
    def paintEvent(self, e):
        p = self.begin(); r = QRectF(self.rect()); fl = label(10.5); fn = body(11.5, 500)
        p.setFont(fn); nw = p.fontMetrics().horizontalAdvance(self.note) if self.note else 0
        p.setFont(fl); name = self.label.upper()
        if p.fontMetrics().horizontalAdvance(name) + nw + 24 > r.width(): name = self.short.upper()
        lw = p.fontMetrics().horizontalAdvance(name)
        text(p, r.adjusted(2, 0, 0, 0), name, fl, TEXT_3, Qt.AlignLeft | Qt.AlignVCenter)
        if self.note:
            p.setFont(fn); note = p.fontMetrics().elidedText(self.note, Qt.ElideRight, max(0, round(r.width() - lw - 28)))
            text(p, r.adjusted(0, 0, -2, 0), note, fn, self.note_color, Qt.AlignRight | Qt.AlignVCenter)
        p.end()

class TorusCard(Panel):
    def __init__(self, kind, title, group, studio):
        super().__init__(title, group); self.kind = kind; self.face = TorusFace(kind, group=group)
        self.lay.addSpacing(6); self.lay.addWidget(self.face, 1); self.lay.addSpacing(10); self.rows = {}
        if kind == 'performance':
            for command, name, short in ROWS:
                labels = [title_case(MODES[command][v]) for v in DISPLAY_ORDER[command]]
                self._row(command, name, labels, lambda i, c=command: studio.select(c, i), short=short)
            if studio.demo:
                self.lay.addSpacing(6); self.hold = Button('Hold knob (demo)', studio.demo_press, height=34, group=group, px=12)
                self.lay.addWidget(self.hold)
        elif kind == 'drive':
            self._row('drive', 'Drive Mode', [title_case(m) for m in DRIVE_MODES], studio.select_drive)
            self._row('ride', 'Ride Height', ['Auto', 'Low', 'Lift'], None, enabled=False)
            self._row('limiter', 'Limiter', ['Off', 'Auto', 'Set'], None, enabled=False)
        else:
            self._row('regen', 'Regen', ['Off', 'Auto', 'One-Pedal'], None, enabled=False)
            self._row('power', 'Power', ['Range', 'Auto', 'Full'], None, enabled=False)
            self._row('battery', 'Battery', ['Auto', 'Track', 'Charge'], None, enabled=False)
        if studio.demo and kind != 'performance':  # same height as the demo knob button: the three faces line up
            self.lay.addSpacing(6); self.lay.addWidget(Footnote('Studio plays this Torus' if kind == 'drive' else 'Arrives with Torus 2', group))
    def _row(self, key, name, labels, on_select, enabled=True, short=None):
        lab = RowLabel(name, self.group, short=short); seg = Segmented(labels, on_select, group=self.group); seg.set_enabled(enabled)
        self.lay.addSpacing(8); self.lay.addWidget(lab); self.lay.addWidget(seg); self.rows[key] = (lab, seg)

class Footnote(Painted):
    def __init__(self, s, group=0, parent=None):
        super().__init__(parent); self.s = s; self.group = group; self.setFixedHeight(34)
    def paintEvent(self, e):
        p = self.begin(); text(p, QRectF(self.rect()), self.s, body(12), TEXT_3); p.end()

class ContextList(Painted):
    """What the Torus concludes from the car Studio plays (the same rules as the firmware)."""
    ROW = 26
    def __init__(self, group=0, parent=None):
        super().__init__(parent); self.rows = (); self.group = group; self.setFixedHeight(26 + 6 * self.ROW)
    def set_rows(self, rows):
        if rows != self.rows: self.rows = rows; self.update()
    def paintEvent(self, e):
        p = self.begin(); w = self.width()
        text(p, QRectF(2, 0, w, 18), 'WHAT THE TORUS SEES', label(10.5), TEXT_3, Qt.AlignLeft | Qt.AlignVCenter)
        for i, (k, v, dim) in enumerate(self.rows):
            y = 26 + i * self.ROW; p.setPen(QPen(BORDER, 1)); p.drawLine(QPointF(2, y), QPointF(w - 2, y))
            text(p, QRectF(2, y, w / 2, self.ROW), k, body(12.5), TEXT_3, Qt.AlignLeft | Qt.AlignVCenter)
            text(p, QRectF(w / 2 - 50, y, w / 2 + 48, self.ROW), v, body(12.5, 500), TEXT_3 if dim else TEXT, Qt.AlignRight | Qt.AlignVCenter)
        p.end()

class VehiclePanel(Panel):
    def __init__(self, studio):
        super().__init__('Vehicle', 1); self.setFixedWidth(352); s = studio
        self.readout = SpeedReadout(group=1); self.lay.addWidget(self.readout); self.lay.setSpacing(6)
        self.slider = SpeedSlider(s.set_speed, TUNING.off_lock_kmh, group=1); self.lay.addWidget(self.slider)
        quick = QHBoxLayout(); quick.setSpacing(6)
        for v in (0, 30, 60, 100, 130, 200):
            b = Button(str(v), lambda v=v: s.set_speed(v), height=30, group=1, px=11.5); b.setMinimumWidth(38); quick.addWidget(b)
        self.lay.addLayout(quick); self.lay.addSpacing(12)
        self.state_label = RowLabel('State', 1); self.lay.addWidget(self.state_label)
        self.state = Segmented(['Park', 'Drive', 'Charging'], s.set_state, group=1); self.lay.addWidget(self.state)
        self.lay.addSpacing(12)
        toggles = QHBoxLayout(); toggles.setSpacing(8)
        self.brake = Button('Brake', s.toggle_brake, toggle=True, group=1)
        self.dropout = Button('Signal dropout', s.toggle_dropout, toggle=True, group=1)
        toggles.addWidget(self.brake); toggles.addWidget(self.dropout, 1); self.lay.addLayout(toggles)
        self.lay.addStretch(1)
        self.context = ContextList(group=1); self.lay.addWidget(self.context)
    def resizeEvent(self, e):
        # Short windows: the controls first; the summary only when it fits completely.
        super().resizeEvent(e); self.context.setVisible(self.height() >= self.needed)
    @property
    def needed(self):
        if not hasattr(self, '_needed'):
            shown = self.context.isVisible(); self.context.setVisible(True); self._needed = self.lay.minimumSize().height(); self.context.setVisible(shown)
        return self._needed

class Caption(Painted):
    def __init__(self, group=0, parent=None):
        super().__init__(parent); self.text = ''; self.prev = ''; self.at = -9.; self.counter = ''; self.group = group
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed); self.setFixedHeight(40)
    def set(self, s, counter):
        if s != self.text: self.prev, self.text, self.at = self.text, s, time.monotonic(); self.update()
        if counter != self.counter: self.counter = counter; self.update()
    def animate(self, dt, now): a = super().animate(dt, now); return a | (now - self.at < .4)
    def paintEvent(self, e):
        p = self.begin(); r = QRectF(self.rect()).adjusted(20, 0, -96, 0); k = smooth((time.monotonic() - self.at) / .36)
        f = heading(20, 400); al = Qt.AlignLeft | Qt.AlignVCenter
        if k < 1: text(p, r.translated(0, -5 * smooth(k * 2)), self.prev, f, with_alpha(TEXT, 1 - smooth(k * 2)), al)
        text(p, r.translated(0, 5 * (1 - smooth(k * 2 - 1))), self.text, f, with_alpha(TEXT, smooth(k * 2 - 1)), al)
        text(p, QRectF(self.width() - 92, 0, 88, self.height()), self.counter, body(12.5, 500, .6), TEXT_3, Qt.AlignRight | Qt.AlignVCenter)
        p.end()

class Timeline(Painted):
    def __init__(self, n, group=0, parent=None):
        super().__init__(parent); self.n = n; self.k = Spring(0, 12, 1); self.target = 0.; self.current = -1; self.group = group; self.setFixedHeight(22)
    def animate(self, dt, now): a = super().animate(dt, now); return self.k.step(self.target, dt) | a
    def paintEvent(self, e):
        p = self.begin(); x0, x1, y = 8, self.width() - 8, 11
        p.setPen(Qt.NoPen); p.setBrush(FIELD); p.drawRoundedRect(QRectF(x0, y - 2, x1 - x0, 4), 2, 2)
        xk = x0 + (x1 - x0) * clamp(self.k.v, 0, 1); p.setBrush(ACCENT); p.drawRoundedRect(QRectF(x0, y - 2, xk - x0, 4), 2, 2)
        for i in range(self.n):
            x = x0 + (x1 - x0) * i / self.n
            if i == self.current: dot(p, x, y, 5, ACCENT); dot(p, x, y, 2, TEXT)
            else: dot(p, x, y, 3, ACCENT if i < self.current else BORDER_HI)
        p.end()

class ScenarioBar(Panel):
    def __init__(self, studio):
        super().__init__('Scenario', 4, header=False); self.setFixedHeight(130); s = studio
        self.lay.setContentsMargins(26, 22, 26, 26); self.lay.setSpacing(14)
        row = QHBoxLayout(); row.setSpacing(10); self.lay.addLayout(row)
        self.play = Button('Play', s.toggle_scenario, primary=True, width=112, height=38, group=4); row.addWidget(self.play)
        self.restart = Button('Restart', s.restart_scenario, width=112, height=38, group=4); row.addWidget(self.restart)
        self.caption = Caption(group=4); row.addWidget(self.caption, 1)
        self.timeline = Timeline(len(STEPS), group=4); self.lay.addWidget(self.timeline)

class Footer(Painted):
    def __init__(self, parent=None):
        super().__init__(parent); self.group = 5; self.setFixedHeight(26); self.stats = ''
        lay = QHBoxLayout(self); lay.setContentsMargins(6, 0, 6, 0); lay.setSpacing(16)
        lay.addWidget(OjmLogo(15, TEXT_2, group=5)); lay.addWidget(Wordmark('assets/studio/ojmsystems_web.png', 9, TEXT_3, group=5)); lay.addStretch(1)
    def paintEvent(self, e):
        p = self.begin(); text(p, QRectF(self.rect()).adjusted(0, 0, -6, 0), self.stats, body(11.5), TEXT_3, Qt.AlignRight | Qt.AlignVCenter); p.end()

class LogPanel(Painted):
    def __init__(self, studio, parent):
        super().__init__(parent); self.studio = studio; self.x = Spring(1, 22, 1); self.open = False; self.group = 0
        self.view = QPlainTextEdit(self); self.view.setReadOnly(True); self.view.setMaximumBlockCount(1500); self.view.setFont(mono(12))
        self.view.setStyleSheet('QPlainTextEdit{background:#161A20;color:#F3F1EB;border:1px solid #2A3039;border-radius:8px;padding:8px}'
                                'QScrollBar:vertical{background:transparent;width:8px;margin:4px}QScrollBar::handle:vertical{background:#3A424D;border-radius:4px}'
                                'QScrollBar::add-line,QScrollBar::sub-line{height:0}')
        self.close_b = Button('Close', self.toggle, width=78, height=32, px=12, parent=self)
        self.save_b = Button('Save', self.save, width=78, height=32, px=12, parent=self)
        self.clear_b = Button('Clear', self.view.clear, width=78, height=32, px=12, parent=self)
    def toggle(self): self.open = not self.open; self.raise_(); self.show()
    def save(self):
        path, _ = QFileDialog.getSaveFileName(self, 'Save CAN log', 'CANertia_CAN.log', 'Log (*.log *.txt)')
        if path: Path(path).write_text(self.view.toPlainText(), encoding='utf-8')
    def add(self, t, kind, line):
        color = {'tx': '#7D95FF', 'rx': '#F3F1EB', 'ack': '#7F8996', 'bad': '#B4BDC9', 'sys': '#7F8996'}.get(kind, '#F3F1EB')
        stamp = f'{t:9.3f}'.replace(' ', '&nbsp;')
        self.view.appendHtml(f'<span style="color:#59616D">{stamp}</span>&nbsp;&nbsp;<span style="color:{color}">{html.escape(line)}</span>')
    def place(self):
        par = self.parent(); w = min(540, par.width() * .42); top = 84; h = par.height() - top - 24
        x = par.width() - 26 - w + (w + 60) * self.x.v
        self.setGeometry(round(x), top, round(w), round(h))
        self.view.setGeometry(26, 70, round(w) - 52, round(h) - 104)
        for i, b in enumerate((self.close_b, self.save_b, self.clear_b)): b.move(round(w) - 32 - 78 - i * 86, 16)
    def animate(self, dt, now):
        moving = self.x.step(0 if self.open else 1, dt); self.place()
        if not self.open and not moving and self.x.v >= 1: self.hide()
        return moving
    def paintEvent(self, e):
        p = painter(self); r = QRectF(self.rect()).adjusted(6, 2, -6, -10); card(p, r)
        text(p, QRectF(r.left() + 20, r.top(), 240, 60), 'CAN log', heading(19, 500), TEXT, Qt.AlignLeft | Qt.AlignVCenter); p.end()

# ---------------------------------------------------------------- the window
class Studio(QMainWindow):
    def __init__(self, demo=False, port='COM8', startup=True):
        super().__init__(); load_fonts(); self.demo = demo
        self.setWindowTitle('CANertia Studio' + (' · Demo' if demo else '')); self.resize(1600, 920); self.setMinimumSize(1280, 720)
        self.state = DialState(); self.online = False; self.connection = 'OFFLINE'; self.worker = None; self.mailbox = EventMailbox()
        self.vehicle = VehicleModel(); self.vehicle_on = True; self.drive_sim = 0; self.rules = TorusRules(); self.sent_vehicle = None
        self.pending = {}; self.ack_ms = None; self.stats = {}; self.t0 = time.monotonic(); self.callback_errors = []
        self.scenario = ScenarioRunner(); self.last = time.monotonic()
        self.launch_seen = -9.; self.boost_seen_on = False; self.cooling_until = -9.
        root = QWidget(); root.setObjectName('root'); root.setStyleSheet('#root{background:#14171C}'); self.setCentralWidget(root); self.root = root
        outer = QVBoxLayout(root); outer.setContentsMargins(30, 20, 30, 14); outer.setSpacing(18)
        top = QHBoxLayout(); top.setSpacing(10); outer.addLayout(top)
        top.addWidget(Wordmark('assets/studio/canertia_studio.png', 22, TEXT, group=0)); top.addSpacing(18)
        self.version = RowLabel(f'v{VERSION}' + ('  ·  demo' if demo else ''), 0); self.version.setFixedWidth(170); top.addWidget(self.version)
        top.addStretch(1)
        self.port = QComboBox(); self.port.setEditable(True); self.port.setFixedSize(150, 36); self.port.setFont(body(12.5, 500))
        self.port.setStyleSheet('QComboBox{background:#161A20;border:1px solid #2A3039;border-radius:8px;padding-left:14px;color:#F3F1EB}'
                                'QComboBox:hover{border-color:#3A424D}QComboBox::drop-down{border:none;width:26px}'
                                'QComboBox QAbstractItemView{background:#1B2028;color:#F3F1EB;selection-background-color:#18244C;'
                                'border:1px solid #2A3039;outline:none;padding:4px}')
        self.refresh_ports(port); top.addWidget(self.port)
        if demo: self.port.setEnabled(False)
        self.connect_b = Button('Connect', self.connect_toggle, primary=True, width=124, group=0); top.addWidget(self.connect_b)
        self.pill = StatusPill(group=0); top.addWidget(self.pill)
        self.log_b = Button('CAN log', self.toggle_log, toggle=True, width=104, group=0); top.addWidget(self.log_b)
        body_row = QHBoxLayout(); body_row.setSpacing(14); outer.addLayout(body_row, 1)
        self.vpanel = VehiclePanel(self); body_row.addWidget(self.vpanel)
        self.cards = {k: TorusCard(k, t, g, self) for k, t, g in (('energy', 'Energy', 2), ('drive', 'Drive', 2), ('performance', 'Performance', 3))}
        for k in ('energy', 'drive', 'performance'): body_row.addWidget(self.cards[k], 1)
        self.drive_toggle = Button('Simulate', self.toggle_drive_sim, toggle=True, width=96, height=30, px=11.5, parent=self.cards['drive'])
        self.drive_toggle.set_checked(True); self.cards['drive'].header_widget = self.drive_toggle
        self.bar = ScenarioBar(self); outer.addWidget(self.bar)
        self.footer = Footer(); outer.addWidget(self.footer)
        self.log = LogPanel(self, root); self.log.hide()
        self.overlay = StartupOverlay(root, self.reveal)
        for key, fn in (('Space', self.toggle_scenario), ('L', self.toggle_log), ('Escape', self.overlay.skip),
                        ('Up', lambda: self.set_speed(self.vehicle.target + 10)), ('Down', lambda: self.set_speed(self.vehicle.target - 10))):
            QShortcut(QKeySequence(key), self).activated.connect(fn)
        self.animated = None; self.timer = QTimer(self); self.timer.setTimerType(Qt.PreciseTimer); self.timer.timeout.connect(self.tick)
        self.timer.start(8)
        if startup: self.overlay.start()
        else: self.overlay.hide(); self.reveal()
        self.sync(); self.place_overlays()
    # ---- layout helpers
    def resizeEvent(self, e):
        super().resizeEvent(e)
        compact = self.height() < 820  # small screens: a lower scenario bar and tighter gaps leave room for the faces
        self.bar.setFixedHeight(112 if compact else 130); self.root.layout().setSpacing(12 if compact else 18)
        self.bar.lay.setContentsMargins(26, 16 if compact else 22, 26, 20 if compact else 26)
        self.place_overlays()
    def place_overlays(self):
        self.overlay.setGeometry(self.root.rect()); self.log.place()
    def reveal(self):
        Reveal.at = time.monotonic()
        if self.demo and self.worker is None: self.connect_toggle()
    def refresh_ports(self, default):
        self.port.clear(); names = [p.device for p in list_ports.comports()]
        for n in ([default] if default not in names else []) + names: self.port.addItem(n)
        self.port.setCurrentText(default)
    # ---- user actions
    def connect_toggle(self):
        if self.worker:
            self.worker.stop(); self.connect_b.set_enabled(False); self.set_connection('CLOSING'); return
        port = 'DEMO' if self.demo else self.port.currentText().strip()
        self.worker = CanWorker(port, self.mailbox, bus_factory=DemoBus if self.demo else None, simulation=False)
        self.worker.configure(vehicle=self.vehicle.frame(), vehicle_on=self.vehicle_on, drive_sim=self.drive_sim)
        self.sent_vehicle = self.vehicle.frame(); self.worker.start(); self.connect_b.set_label('Disconnect')
        self.logline('sys', f'CONNECT {port}')
    def set_connection(self, word):
        self.connection = word; self.online = word == 'ONLINE'
        word_s = {'ONLINE': 'Online', 'WAITING FOR DDE V2': 'Waiting for Torus'}.get(word, word.capitalize())
        if self.online and self.ack_ms is not None: word_s += f' · {self.ack_ms:.0f} ms'
        self.pill.set(word_s, self.online)
    def select(self, command, index):
        value = DISPLAY_ORDER[command][index]; label, seg = self.cards['performance'].rows[command]
        reason = self.rules.lock_reason(command, value)
        if reason:
            seg.bounce(1 if index > seg.index else -1); label.set_note(f'Locked · {reason}', TEXT_2, 1.8); return
        if self.online:
            self.worker.submit(PAGE, command - RWS); self.worker.submit(command, value); self.pending[command] = value
        else:  # offline: a local preview only
            key = {RWS: 'rws', TV: 'tv', AERO: 'aero'}[command]; self.state = replace(self.state, page=command - RWS, **{key: value})
    def select_drive(self, index):
        self.drive_sim = index; self.drive_toggle.set_checked(True)
        if self.worker: self.worker.configure(drive_sim=index)
    def toggle_drive_sim(self):
        self.drive_sim = (self.drive_sim if self.drive_sim is not None else 0) if self.drive_toggle.checked else None
        if self.drive_toggle.checked and self.drive_sim is None: self.drive_sim = 0
        if self.worker: self.worker.configure(drive_sim=self.drive_sim)
    def set_speed(self, v):
        self.vehicle.target = float(clamp(v, 0, SpeedSlider.MAX))
        if self.vehicle.state != DRIVE_STATE and self.vehicle.target > 0: self.vehicle.state = DRIVE_STATE
    def set_state(self, index): self.vehicle.state = (PARK, DRIVE_STATE, CHARGING)[index]
    def toggle_brake(self): self.vehicle.brake = self.vpanel.brake.checked
    def toggle_dropout(self):
        self.vehicle_on = not self.vpanel.dropout.checked
        if self.worker: self.worker.configure(vehicle_on=self.vehicle_on)
    def toggle_log(self): self.log.toggle(); self.log_b.set_checked(self.log.open)
    def demo_press(self):
        if self.worker and self.demo: self.worker.scenario('LongPress')
    def toggle_scenario(self):
        now = time.monotonic()
        if self.scenario.running: self.scenario.pause(now)
        else: self.scenario.play(now)
    def restart_scenario(self): self.scenario.stop(); self.scenario.play(time.monotonic())
    # ---- scenario hooks
    def apply_step(self, st):
        v = self.vehicle
        if st.state is not None: v.state = st.state
        if st.speed is not None: v.target = st.speed
        if st.brake is not None: v.brake = st.brake; self.vpanel.brake.set_checked(st.brake)
        if st.dropout is not None:
            self.vehicle_on = not st.dropout; self.vpanel.dropout.set_checked(st.dropout)
            if self.worker: self.worker.configure(vehicle_on=self.vehicle_on)
        if st.drive != -2:
            self.drive_sim = st.drive; self.drive_toggle.set_checked(st.drive is not None)
            if self.worker: self.worker.configure(drive_sim=st.drive)
        if st.command and self.online:  # straight to the Torus, so the committee sees the Torus refuse a locked value
            c, value = st.command; self.worker.submit(PAGE, c - RWS); self.worker.submit(c, value); self.pending[c] = value
    def action_bits(self): return (self.state.launch, self.state.boost) if self.online else (False, False)
    # ---- events from the CAN worker
    def logline(self, kind, line): self.log.add(time.monotonic() - self.t0, kind, line)
    def handle(self, kind, payload):
        if kind == 'connection':
            self.set_connection(payload); self.logline('sys', payload)
            if payload != 'ONLINE': self.pending.clear()
        elif kind == 'state':
            old = self.state; self.state = payload
            for c, name in ((PAGE, 'PAGE'), (RWS, 'RWS'), (TV, 'TV'), (AERO, 'AERO')):
                if old.value(c) != payload.value(c): self.logline('rx', f'TORUS {name} -> {MODES[c][payload.value(c)]}')
            if old.launch != payload.launch: self.logline('rx', 'TORUS LAUNCH ' + ('ARMED' if payload.launch else 'OFF'))
            if old.boost != payload.boost: self.logline('rx', 'TORUS BOOST ' + ('ON' if payload.boost else 'OFF'))
        elif kind == 'confirmed':
            c, v, ms, desc = payload; self.pending.pop(c, None); self.ack_ms = ms; self.logline('ack', f'ACK {desc} · {ms:.0f} ms')
            self.set_connection(self.connection)
        elif kind == 'failed':
            c, v, msg = payload; self.pending.pop(c, None); self.logline('bad', msg.upper())
            if c in self.cards['performance'].rows:
                label, seg = self.cards['performance'].rows[c]; seg.bounce()
                reason = self.rules.lock_reason(c, v)
                label.set_note('Refused · ' + (reason or 'by the Torus') if msg.startswith('Rejected') else 'No answer', TEXT_2, 2.4)
        elif kind == 'tx': self.logline('tx', payload)
        elif kind == 'error': self.logline('bad', 'ERROR ' + str(payload))
        elif kind == 'stats': self.stats = payload
        elif kind == 'stopped':
            self.worker = None; self.connect_b.set_label('Connect'); self.connect_b.set_enabled(True); self.set_connection('OFFLINE')
    # ---- every frame
    def sync(self):
        s = self.state; r = self.rules; card = self.cards['performance']
        for command, _, _s in ROWS:
            label, seg = card.rows[command]; value = s.value(command)
            seg.set_index(DISPLAY_ORDER[command].index(value)); seg.pending = command in self.pending
            seg.set_locked({i: r.lock_reason(command, v) for i, v in enumerate(DISPLAY_ORDER[command]) if r.lock_reason(command, v)})
            auto = r.auto_name(command) if value == (0 if command == AERO else 3) else None
            if command in self.pending: label.set_note('Sending', ACCENT_INK)
            elif auto: label.set_note(f'Auto · {title_case(auto)}', ACCENT_INK)
            else: label.set_note('')
        face = card.face; vis = vis_of(s.page); c = (AERO, RWS, TV)[vis]; value = s.value(c)
        face.page = vis; face.is_online = self.online or self.worker is None
        auto = r.auto_name(c) if value == (0 if c == AERO else 3) else None
        launch, boost = self.action_bits(); now = time.monotonic()
        if launch: self.launch_seen = now
        if self.boost_seen_on and not boost: self.cooling_until = now + TUNING.boost_cooldown
        self.boost_seen_on = boost
        went = not launch and now - self.launch_seen < .9 and r.known and r.vehicle.speed >= TUNING.launch_go_kmh
        if boost: face.set_text('Boost', 'Active'); face.sub = ''
        elif launch or went: face.set_text('Launch Control', 'Go' if went else 'Ready'); face.sub = ''
        else: face.set_text(TITLES[vis], title_case(MODES[c][value])); face.sub = title_case(auto) if auto else ''
        face.wing_target = WING[r.aero_auto] if s.aero == 0 else WING[s.aero]  # Auto: what it picks (rest 12 deg without car data)
        face.set_actions(*self.action_bits())
        card.set_status(('Demo · live' if self.demo else 'Live') if self.online else 'Offline', self.online)
        if self.demo: card.hold.set_enabled(self.online)
        drive = self.cards['drive']; dl, dseg = drive.rows['drive']
        if self.drive_sim is not None: dseg.set_index(self.drive_sim)
        dseg.set_locked({}); drive.face.is_online = self.drive_sim is not None
        drive.face.set_text('Drive Mode', title_case(DRIVE_MODES[self.drive_sim]) if self.drive_sim is not None else 'Not sent')
        drive.face.sub = 'Simulated' if self.drive_sim is not None else ''
        drive.set_status('', False); dl.set_note('Torus 3 simulated' if self.drive_sim is not None else 'Not sent', TEXT_3)
        for key in ('ride', 'limiter'): drive.rows[key][0].set_note('Next', DISABLED)
        energy = self.cards['energy']; energy.face.dimmed = True; energy.face.set_text('Energy', 'Next'); energy.set_status('Torus 2 · next', False)
        for key in ('regen', 'power', 'battery'): energy.rows[key][0].set_note('Next', DISABLED)
        vp = self.vpanel; v = self.vehicle
        vp.readout.target = v.speed; vp.readout.sending = self.vehicle_on
        vp.slider.set_value(v.target); vp.slider.set_enabled(v.state == DRIVE_STATE)
        vp.state.set_index((PARK, DRIVE_STATE, CHARGING).index(v.state))
        vp.set_status(f'Sending at {1 / VEHICLE_PERIOD:.0f} Hz' if self.vehicle_on and self.worker else 'Dropout' if not self.vehicle_on else 'Not connected',
                      bool(self.vehicle_on and self.worker))
        cooling = time.monotonic() < self.cooling_until
        if launch: act = 'Launch armed'
        elif boost: act = 'Boost running'
        elif cooling and not r.standing(): act = 'Boost cooling'
        else: act = {0: '-', 1: 'Launch Control', 2: 'Boost'}[r.available_action(cooling)]
        car = 'No signal' if not r.known else 'Charging' if v.state == CHARGING else 'Standing' if r.standing() else 'Driving'
        vp.context.set_rows((
            ('Car', car, not r.known),
            ('Long press', act, act == '-'),
            ('Off (RWS, TV)', f'Locked > {TUNING.off_lock_kmh:.0f} km/h' if r.off_locked else 'Allowed', r.off_locked),
            ('Drift', 'Allowed (Track)' if self.drive_sim == 2 else 'Track only', self.drive_sim != 2),
            ('Aero Auto', title_case(AERO_MODES[r.aero_auto]) if r.known else 'At rest', not r.known),
            ('RWS Auto', title_case(RWS_MODES[r.rws_auto]) if r.known else 'Neutral', not r.known)))
        now = time.monotonic(); sc = self.scenario; st = sc.step
        if st: self.bar.caption.set(st.caption, f'{sc.index + 1:02d} / {len(STEPS):02d}')
        elif sc.index >= len(STEPS): self.bar.caption.set('Scenario complete.', f'{len(STEPS):02d} / {len(STEPS):02d}')
        else: self.bar.caption.set('Press Play to run the defense scenario.', f'00 / {len(STEPS):02d}')
        self.bar.timeline.target = sc.progress(now); self.bar.timeline.current = sc.index
        self.bar.play.set_label('Pause' if sc.running else 'Play')
        st = self.stats
        self.footer.stats = f'CAN 500 kbit/s   ·   TX {st.get("tx", 0)}   ·   RX {st.get("rx", 0)}   ·   retries {st.get("retry", 0)}' if st else 'CAN 500 kbit/s'
    def tick(self):
        try:
            now = time.monotonic(); dt = min(.05, now - self.last); self.last = now
            for _ in range(256):
                try: kind, payload, _t = self.mailbox.get_nowait()
                except Exception: break
                self.handle(kind, payload)
            self.vehicle.step(dt); frame = self.vehicle.frame()
            if self.worker and frame != self.sent_vehicle: self.worker.configure(vehicle=frame); self.sent_vehicle = frame
            self.rules.update(frame if self.vehicle_on else None, self.drive_sim)
            self.scenario.tick(now, self); self.sync()
            if self.animated is None: self.animated = self.root.findChildren(Painted) + [self.overlay]
            for w in self.animated:
                if w.isVisible() and w.animate(dt, now): w.update()
        except Exception as exc:  # never let one frame kill the window; tests read this list
            self.callback_errors.append(repr(exc))
            if len(self.callback_errors) == 1: raise
    def closeEvent(self, e):
        if self.worker: self.worker.stop(); self.worker.join(2)
        super().closeEvent(e)

App = Studio

def main():
    parser = argparse.ArgumentParser(); parser.add_argument('--port', default='COM8'); parser.add_argument('--demo', action='store_true')
    parser.add_argument('--no-startup', action='store_true'); args = parser.parse_args()
    app = QApplication(sys.argv); w = Studio(demo=args.demo, port=args.port, startup=not args.no_startup); w.show(); sys.exit(app.exec())

if __name__ == '__main__': main()
