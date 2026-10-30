"""CANertia Studio smoke test (V6.0.7): runs the real window in demo mode (in-memory Torus, no port), drives the
startup, the controls and part of the scenario, and saves window captures to validation/ with --capture."""
from pathlib import Path
import os, sys, argparse, json, time
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'Vehicle_ECU_Simulator'))
if '--show' not in sys.argv: os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
from PySide6.QtWidgets import QApplication
from PySide6.QtGui import QFontInfo

def run(capture=False, sizes=((1600, 920), (1280, 720), (1920, 1080))):
    app = QApplication.instance() or QApplication([])
    from main import Studio
    from protocol import RWS, TV, AERO, DRIVE_STATE
    import studio_theme as th
    w = Studio(demo=True); w.resize(*sizes[0]); w.show(); report = {}
    def pump(seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end: app.processEvents(); time.sleep(.004)
    def wait(predicate, timeout=4):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            app.processEvents()
            if predicate(): return
            time.sleep(.004)
        raise AssertionError('GUI timeout')
    def snap(name):
        if capture: w.grab().save(str(ROOT / 'validation' / name))
    try:
        fonts = {k: QFontInfo(f).family() for k, f in (('heading', th.heading(20)), ('body', th.body(12)), ('dial', th.dial(20)))}
        assert fonts == {'heading': 'Clash Display', 'body': 'Sora', 'dial': 'Michroma'}, fonts
        (ROOT / 'validation/font_check.json').write_text(json.dumps({'actual_font': {'family': fonts['dial']}, 'studio_fonts': fonts, 'process_private': True}, indent=2) + '\n')
        pump(.65); snap('gui_startup_infinity.png'); pump(2.7); snap('gui_startup_laser.png'); pump(.9); snap('gui_startup_logo.png')
        wait(lambda: not w.overlay.isVisible(), 6); wait(lambda: w.online, 4); pump(1.4); snap('gui_main.png')
        # The car: Drive at 100 km/h -> Auto picks (Aero Downforce), the Off lock above 120 km/h.
        w.set_state(1); w.set_speed(100); pump(3.0); assert w.rules.aero_auto == 2, w.rules.aero_auto
        w.select(AERO, 0); wait(lambda: w.state.aero == 0 and not w.pending); pump(.8); snap('gui_drive_auto.png')
        w.set_speed(140); pump(1.6); assert w.rules.off_locked
        w.select(RWS, 0); assert w.state.rws != 0, 'a locked Off is not sent'
        w.select(TV, 3); assert w.state.tv != 3, 'Drift without Track is not sent'
        pump(.3); snap('gui_locked.png')
        w.select_drive(2); pump(.5); w.select(TV, 3); wait(lambda: w.state.tv == 3); report['drift_in_track'] = True
        w.select_drive(1); wait(lambda: w.state.tv == 2); report['drift_falls_back'] = True
        # Boost from the demo knob.
        w.demo_press(); wait(lambda: w.state.boost); pump(1.0); snap('gui_boost.png'); report['boost'] = True
        w.set_speed(0); w.set_state(0); wait(lambda: not w.state.boost, 6)
        w.demo_press(); wait(lambda: w.state.launch); pump(.8); snap('gui_launch.png'); report['launch'] = True
        w.toggle_log(); pump(.8); snap('gui_log.png'); w.toggle_log(); pump(.6)
        w.restart_scenario(); pump(1.2); snap('gui_scenario.png'); report['scenario_running'] = w.scenario.running
        for size in sizes[1:]:
            w.resize(*size); pump(.5); snap(f'gui_{size[0]}x{size[1]}.png')
            for card in w.cards.values():
                for label, seg in card.rows.values():
                    assert seg.height() >= 30 and seg.width() >= 150, ('compressed control', size, seg.width())
        assert not w.callback_errors, w.callback_errors
        report.update(callback_errors=w.callback_errors, fonts=fonts, sizes=[list(s) for s in sizes])
        (ROOT / 'validation/gui_smoke.json').write_text(json.dumps(report, indent=2) + '\n'); print(json.dumps(report, indent=2))
    finally:
        w.close()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(); parser.add_argument('--capture', action='store_true'); parser.add_argument('--show', action='store_true')
    run(parser.parse_args().capture)
