from pathlib import Path
import sys, time, unittest
from dataclasses import replace
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Vehicle_ECU_Simulator'))
from main import App
from protocol import *
from can_link import DemoBus
from presentation import local_presentation
class GuiTests(unittest.TestCase):
    def wait_for(self,predicate,timeout=3):
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            self.app.update()
            if predicate():return
            time.sleep(.005)
        self.fail('GUI timeout')
    def setUp(self):self.app=App(demo=True);self.app.withdraw()
    def tearDown(self):
        worker=self.app.worker;self.app.close()
        if worker:worker.join(2)
    def test_standalone_preview_all_modes(self):
        a=self.app
        for command in (RWS,TV,AERO):
            for mode in range(4):
                a.select_mode(command,mode);a.update()
                self.assertEqual(a.state.value(command),mode)
                self.assertEqual(a.state.page,command-RWS)
        self.assertIsNone(a.worker);self.assertFalse(a.requested);self.assertEqual(a.ack.get(),'—')
        self.assertFalse(a.callback_errors)
    def test_two_way_sync_without_telemetry(self):
        a=self.app;bus=DemoBus()
        with patch('main.DemoBus',return_value=bus):a.connect()
        self.wait_for(lambda:a.online)
        a.mode_widgets[1].choose(3)
        self.wait_for(lambda:bus.state.tv==3 and a.state.tv==3 and a.state.page==1 and not a.requested)
        a.mode_widgets[2].choose(3)
        self.wait_for(lambda:bus.state.aero==3 and not a.requested)
        # Simulate local knob changes in the dial; its heartbeat updates PC.
        bus.state=replace(bus.state,page=0,rws=1);bus.dirty=True
        self.wait_for(lambda:a.state.page==0 and a.mode_vars[RWS].get()=='STABLE')
        self.assertEqual(a.page_var.get(),'RWS')
        self.assertEqual(bus.cycles,0,'mirror must not send simulated telemetry')
        self.assertFalse(a.worker.simulation);self.assertFalse(a.callback_errors)
    def test_latest_wins_disconnect_and_reconnect(self):
        a=self.app;a.connect();self.wait_for(lambda:a.online)
        for i in range(1000):a.send(TV,i%4)
        a.select_mode(TV,3);self.wait_for(lambda:a.state.tv==3 and not a.requested)
        a.connect();self.wait_for(lambda:a.worker is None)
        self.assertEqual(a.ack.get(),'—');a.select_mode(RWS,2)
        self.assertEqual(a.state.rws,2)
        a.connect();self.wait_for(lambda:a.online and a.state.rws==3)
        self.assertFalse(a.requested,'offline preview must not overwrite reconnected dial')
        self.assertFalse(a.callback_errors)
    def test_presentation_modes_distinct_and_log(self):
        a=self.app;a.set_diagnostics(True);a.update()
        self.assertEqual(a.diagnostics_footer.winfo_manager(),'pack')
        loads=[local_presentation(DialState(tv=i),1).rr for i in range(4)]
        self.assertEqual(sorted(set(loads)),loads)
        self.assertEqual(local_presentation(DialState(aero=3),1).aero_actual,100)
        self.assertLess(local_presentation(DialState(rws=2),1).rear_actual,0)
        self.assertGreater(local_presentation(DialState(rws=1),1).rear_actual,0)
        a.set_diagnostics(False);self.assertFalse(a.diagnostics_footer.winfo_manager())
        self.assertFalse(a.callback_errors)
