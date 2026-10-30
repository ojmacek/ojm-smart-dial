"""CANertia Studio (Qt) against the in-memory Performance Torus (DemoBus): no CAN port is opened."""
from pathlib import Path
import os, sys, time, unittest
from dataclasses import replace
os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'Vehicle_ECU_Simulator'))
from PySide6.QtWidgets import QApplication
APP = QApplication.instance() or QApplication([])
from main import Studio
from protocol import *
import startup

class GuiTests(unittest.TestCase):
    def pump(self, seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end: APP.processEvents(); time.sleep(.003)
    def wait_for(self, predicate, timeout=4):
        end = time.monotonic() + timeout
        while time.monotonic() < end:
            APP.processEvents()
            if predicate(): return
            time.sleep(.003)
        self.fail('GUI timeout')
    def setUp(self): self.w = Studio(demo=True, startup=False); self.w.show()
    def tearDown(self): self.w.close(); APP.processEvents()
    def test_startup_is_the_torus_startup(self):
        s = startup.Startup(startup.Assets())
        self.assertTrue(6400 <= s.total <= 6600 and 980 <= s.close_at <= 1080, (s.total, s.close_at))
        w = Studio(demo=True, startup=True); w.show()
        try:
            self.pump(.3); self.assertTrue(w.overlay.isVisible()); w.overlay.skip(); self.pump(.6)
            self.assertFalse(w.overlay.isVisible(), 'a skip reveals the UI')
        finally: w.close()
    def test_demo_two_way_sync(self):
        w = self.w; self.wait_for(lambda: w.online)
        w.select(AERO, 3); self.wait_for(lambda: w.state.aero == 3 and w.state.page == 2 and not w.pending)
        w.select(RWS, 2); self.wait_for(lambda: w.state.rws == 1, 3)  # UI position 2 = Stable = CAN 1
        self.assertEqual(w.cards['performance'].rows[RWS][1].index, 2)
        self.assertFalse(w.callback_errors)
    def test_locks_are_not_sent_and_drift_needs_track(self):
        w = self.w; self.wait_for(lambda: w.online)
        w.set_state(1); w.set_speed(140); self.wait_for(lambda: w.rules.off_locked, 6)
        w.select(RWS, 0); w.select(TV, 3); self.pump(.4)
        self.assertNotEqual(w.state.rws, 0); self.assertNotEqual(w.state.tv, 3); self.assertFalse(w.pending)
        w.select_drive(2); self.pump(.4); w.select(TV, 3); self.wait_for(lambda: w.state.tv == 3)
        w.select_drive(0); self.wait_for(lambda: w.state.tv == 2, 3)  # leaving Track: Sport
        self.assertFalse(w.callback_errors)
    def test_launch_and_boost_from_the_demo_knob(self):
        w = self.w; self.wait_for(lambda: w.online)
        w.demo_press(); self.wait_for(lambda: w.state.launch)
        self.assertEqual(w.cards['performance'].face.name, 'Ready')
        w.set_state(1); w.set_speed(60); self.wait_for(lambda: not w.state.launch, 4)
        self.wait_for(lambda: w.vehicle.speed >= 30, 4); w.demo_press(); self.wait_for(lambda: w.state.boost)
        self.assertEqual(w.cards['performance'].face.title, 'Boost')
        self.assertFalse(w.callback_errors)
    def test_dropout_and_scenario(self):
        w = self.w; self.wait_for(lambda: w.online)
        w.vpanel.dropout.set_checked(True); w.toggle_dropout(); self.pump(.2); self.assertFalse(w.rules.known)
        w.vpanel.dropout.set_checked(False); w.toggle_dropout(); self.pump(.2); self.assertTrue(w.rules.known)
        w.restart_scenario(); self.pump(.2); self.assertTrue(w.scenario.running); self.assertEqual(w.scenario.index, 0)
        w.toggle_scenario(); self.assertFalse(w.scenario.running); w.toggle_log(); self.pump(.4); self.assertTrue(w.log.isVisible())
        self.assertFalse(w.callback_errors)
    def test_offline_preview(self):
        w = Studio(demo=False, port='COM_NONE', startup=False); w.show()
        try:
            self.pump(.2); w.select(TV, 2); self.assertEqual(w.state.tv, 2); self.assertIsNone(w.worker)
            self.assertFalse(w.callback_errors)
        finally: w.close()

if __name__ == '__main__': unittest.main()
