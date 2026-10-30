from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Vehicle_ECU_Simulator'))
import unittest
import time
from dataclasses import replace
from protocol import *
from can_link import CommandEngine,TelemetryScheduler,DemoBus,CanWorker,EventMailbox
from ecu import DynamicsEcu

def ack(message,status=0,profile=DYNAMICS):return checked_frame(profile.ack_id,bytes(message.data)+bytes((status,)))

class CommandTests(unittest.TestCase):
    def setUp(self):self.events=[];self.sent=[];self.engine=CommandEngine(lambda *e:self.events.append(e),seed=1)
    def test_future_schema_uses_same_ack_engine(self):
        profile=PROFILES['DRIVE']
        engine=CommandEngine(lambda *e:self.events.append(e),seed=0,profile=profile,
            validator=lambda c,v:c==0x20 and type(v) is int and 0<=v<=10,
            describer=lambda c,v:f'EXTENSION {v}')
        engine.submit(0x20,10,0);engine.service(0,self.sent.append)
        self.assertEqual(self.sent[0].arbitration_id,0x302)
        self.assertFalse(engine.acknowledge(ack(self.sent[0]),.01))
        self.assertTrue(engine.acknowledge(ack(self.sent[0],profile=profile),.01))
    def test_first_immediate_and_full_sequence(self):
        self.engine.submit(RWS,2,0);self.engine.service(0,self.sent.append)
        self.assertEqual(bytes(self.sent[0].data),bytes((RWS,2,2,0,0,0)))
    def test_thousand_updates_latest_and_throttled(self):
        for i in range(1000):self.engine.submit(RWS,i%4,i*.001);self.engine.service(i*.001,self.sent.append)
        self.engine.submit(RWS,3,1);self.engine.service(1.05,self.sent.append)
        self.assertLessEqual(len(self.sent),30);self.assertEqual(self.sent[-1].data[1],3)
    def test_independent_functions(self):
        for c,v in ((PAGE,2),(RWS,2),(TV,3),(AERO,1)):self.engine.submit(c,v,0)
        self.engine.service(0,self.sent.append);self.assertEqual(len(self.sent),4)
    def test_stale_ack_same_value(self):
        for i,v in enumerate((1,2,1)):self.engine.submit(RWS,v,i*.04);self.engine.service(i*.04,self.sent.append)
        self.assertFalse(self.engine.acknowledge(ack(self.sent[0]),.1));self.assertTrue(self.engine.acknowledge(ack(self.sent[-1]),.1))
    def test_unsent_ack_not_accepted(self):
        self.engine.submit(RWS,1,0)
        self.assertFalse(self.engine.acknowledge(ack(frame(0x301,bytes((RWS,1,2,0,0,0)))),.01))
    def test_exact_command_value_and_sequence(self):
        self.engine.submit(RWS,1,0);self.engine.service(0,self.sent.append)
        for at in (0,1,2,3,4,5):
            m=ack(self.sent[0]);m.data[at]^=1;m.data[7]=checksum(m.data[:7]);self.assertFalse(self.engine.acknowledge(m,.1))
        self.assertTrue(self.engine.acknowledge(ack(self.sent[0]),.1))
    def test_duplicate_does_not_reset_retry_budget(self):
        self.engine.submit(RWS,1,0)
        for t in (0,.251,.502,.753):self.engine.submit(RWS,1,t);self.engine.service(t,self.sent.append)
        self.assertEqual(len(self.sent),3);self.assertFalse(self.engine.pending);self.assertEqual(self.engine.retries,2)
    def test_superseded_never_retried(self):
        self.engine.submit(RWS,1,0);self.engine.service(0,self.sent.append)
        self.engine.submit(RWS,2,.2);self.engine.service(.251,self.sent.append)
        self.assertEqual([m.data[1] for m in self.sent],[1,2])
    def test_invalid_command_values_and_airbrake(self):
        for c,v in ((1,44),(RWS,4),(TV,-1),(AERO,4),(PAGE,3),(RWS,1.5)):
            with self.assertRaises(ValueError):self.engine.submit(c,v,0)
    def test_airbrake_requested_roundtrip(self):
        self.engine.submit(AERO,3,0);self.engine.service(0,self.sent.append)
        self.assertTrue(self.engine.acknowledge(ack(self.sent[0]),.01))
        state=DialState(aero=3);self.assertEqual(decode_state(encode_state(state),0),state)
    def test_wrap_rejection_and_cancel(self):
        self.engine.sequence=0xffffffff;self.engine.submit(TV,1,0);self.engine.service(0,self.sent.append)
        self.assertEqual(bytes(self.sent[0].data[2:]),bytes(4));self.assertTrue(self.engine.acknowledge(ack(self.sent[0],1),.1))
        self.engine.submit(TV,3,.2);self.engine.cancel();self.engine.service(10,self.sent.append);self.assertEqual(len(self.sent),1)
    def test_profile_isolation_and_legacy_state_rejection(self):
        self.engine.submit(RWS,1,0);self.engine.service(0,self.sent.append)
        self.assertFalse(self.engine.acknowledge(ack(self.sent[0],profile=PROFILES['DRIVE']),.1))
        self.assertIsNone(decode_state(checked_frame(0x201,(3,44,255,0,0,0,1)),0))
    def test_ack_validation(self):
        self.engine.submit(RWS,1,0);self.engine.service(0,self.sent.append)
        m=ack(self.sent[0]);m.data[7]^=1;self.assertFalse(self.engine.acknowledge(m,.1))
        for attr in ('is_extended_id','is_remote_frame','is_error_frame','is_fd'):
            m=ack(self.sent[0]);setattr(m,attr,True);self.assertFalse(self.engine.acknowledge(m,.1))

class ProtocolTests(unittest.TestCase):
    def test_reserved_ids_unique(self):
        ids=[v for p in PROFILES.values() for v in (p.state_id,p.command_id,p.ack_id,p.telemetry_id)]
        self.assertEqual(len(ids),len(set(ids)))
    def test_state_roundtrip_and_invalid(self):
        state=DialState(2,2,3,1,3,15);self.assertEqual(decode_state(encode_state(state),0),state)
        for index,value in ((0,0),(1,3),(2,4),(3,4),(4,4),(5,4),(6,16)):
            m=encode_state(state);m.data[index]=value;m.data[7]=checksum(m.data[:7]);self.assertIsNone(decode_state(m,0))
    def test_all_telemetry_fields_roundtrip(self):
        original=Telemetry(123.4,-21.2,93,-4.8,3.2,2,11,22,33,34,-65,95,38,3,11,3,-1.23,42)
        assembler=TelemetryAssembler();result=None
        for message in encode_telemetry(original,7):result=assembler.accept(message,.1)
        self.assertEqual(result,original)
    def test_no_partial_or_mixed_generations(self):
        a=TelemetryAssembler();old=encode_telemetry(Telemetry(speed=10),1);new=encode_telemetry(Telemetry(speed=90),2)
        for m in old[:3]:self.assertIsNone(a.accept(m,0))
        for m in new[:3]:self.assertIsNone(a.accept(m,.01))
        for m in old[3:]:self.assertIsNone(a.accept(m,.02))
        self.assertIsNone(a.accept(new[3],.03));self.assertEqual(a.accept(new[4],.04).speed,90)
    def test_out_of_order_duplicate_wrap_and_timeout(self):
        a=TelemetryAssembler()
        for generation in (255,0):
            messages=encode_telemetry(Telemetry(speed=generation),generation)
            for m in messages[::-1]:result=a.accept(m,.01)
            self.assertEqual(result.speed,generation)
            self.assertIsNone(a.accept(messages[0],.02))
        for m in encode_telemetry(Telemetry(speed=99),200):result=a.accept(m,1)
        self.assertEqual(result.speed,99)
    def test_malformed_fragment_rejected(self):
        for index in range(5):
            m=encode_telemetry(Telemetry(),1)[index];m.data[7]^=1
            self.assertIsNone(TelemetryAssembler().accept(m,0))
        m=encode_telemetry(Telemetry(),1)[3];m.data[4]=4;m.data[7]=checksum(m.data[:7]);self.assertFalse(valid_fragment(m.data))
    def test_typed_fragment_range_checks(self):
        for index,offset,value in ((0,6,101),(1,6,4),(2,2,101),(3,5,16),(4,5,1)):
            m=encode_telemetry(Telemetry(),1)[index];m.data[offset]=value;m.data[7]=checksum(m.data[:7]);self.assertFalse(valid_fragment(m.data))
    def test_10_30_60_hz_no_catchup_or_backlog(self):
        for hz in (10,30,60):
            with self.subTest(hz=hz):
                scheduler=TelemetryScheduler();scheduler.hz=hz;sent=[]
                for n in range(5000):scheduler.schedule(n*.001,Telemetry());scheduler.service(sent.append)
                self.assertAlmostEqual(scheduler.sent_cycles,hz*5,delta=1);self.assertLessEqual(len(scheduler.pending),5)
                scheduler.schedule(100,Telemetry());self.assertLessEqual(len(scheduler.pending),5)
                before=scheduler.sent_cycles;scheduler.schedule(100.001,Telemetry());self.assertEqual(scheduler.sent_cycles,before)
    def test_event_queue_bounded_latest(self):
        events=EventMailbox()
        for i in range(10000):events.put(('state',i,0));events.put(('tx',i,0))
        self.assertEqual(len(events.events),256);self.assertEqual(len(events.latest),1);self.assertGreater(events.dropped,0)
        events.put(('connection','OFFLINE',0));self.assertNotIn('state',events.latest)

class EcuTests(unittest.TestCase):
    def test_rws_counter_to_same_phase_and_off(self):
        ecu=DynamicsEcu();ecu.inputs.speed=10;ecu.inputs.front=20
        low=ecu.step(.02);self.assertLess(low.rear_requested,0);self.assertEqual(low.rws_actual,2)
        ecu.inputs.speed=120;high=ecu.step(.02);self.assertGreater(high.rear_requested,0);self.assertEqual(high.rws_actual,1)
        off=ecu.step(.02,DialState(rws=0));self.assertEqual(off.rear_requested,0)
    def test_actual_never_teleports(self):
        ecu=DynamicsEcu();ecu.inputs.front=30;t=ecu.step(.01)
        self.assertLess(abs(t.rear_actual),abs(t.rear_requested))
        t=ecu.step(.01,DialState(aero=2));self.assertLess(t.aero_actual,t.aero_requested)
    def test_airbrake_automatic_and_returns_to_requested_mode(self):
        ecu=DynamicsEcu();ecu.inputs.speed=140;ecu.inputs.brake=90
        t=ecu.step(.02,DialState(aero=1));self.assertEqual(t.aero_mode,3);self.assertEqual(t.aero_requested,100)
        ecu.inputs.brake=0;t=ecu.step(.02,DialState(aero=1));self.assertEqual(t.aero_mode,1);self.assertEqual(t.aero_requested,5)
    def test_stuck_fault_timeout_and_clear(self):
        ecu=DynamicsEcu();ecu.inputs.aero_stuck=True
        for _ in range(130):t=ecu.step(.02,DialState(aero=2))
        self.assertTrue(t.faults&2);self.assertEqual(t.aero_actual,0)
        ecu.inputs.aero_stuck=False
        for _ in range(200):t=ecu.step(.02,DialState(aero=2))
        self.assertFalse(t.faults&2)
    def test_scenarios_and_reset(self):
        ecu=DynamicsEcu()
        for name,page in zip(PAGES,range(3)):
            ecu.start(name,0);self.assertEqual(ecu.scenario_values(5)[PAGE],page)
        self.assertEqual(ecu.scenario_values(16)[AERO],2);self.assertGreater(ecu.inputs.brake,75)
        ecu.stop();self.assertEqual(ecu.scenario_values(17),{});ecu.reset();self.assertFalse(ecu.inputs.aero_stuck)

class WorkerTests(unittest.TestCase):
    def wait_for(self,fn,timeout=3):
        deadline=time.monotonic()+timeout
        while time.monotonic()<deadline:
            if fn():return
            time.sleep(.003)
        self.fail('Timed out')
    def test_real_time_10_30_60_and_burst(self):
        bus=DemoBus();worker=CanWorker('DEMO',EventMailbox(),lambda:bus);worker.start()
        try:
            self.wait_for(lambda:worker.online)
            for hz in (10,30,60):
                worker.configure(rate=hz);time.sleep(.12);start=bus.cycles;time.sleep(1)
                self.assertAlmostEqual(bus.cycles-start,hz,delta=5)
            for n in range(1000):worker.submit(RWS,n%4)
            worker.submit(RWS,2);worker.submit(TV,3);worker.submit(AERO,1)
            self.wait_for(lambda:bus.state.rws==2 and bus.state.tv==3 and bus.state.aero==1)
        finally:worker.stop();worker.join(2)
        self.assertFalse(worker.is_alive())
    def test_offline_cancel_and_reconnect_no_replay(self):
        bus=DemoBus();worker=CanWorker('DEMO',EventMailbox(),lambda:bus);worker.start()
        try:
            self.wait_for(lambda:worker.online);bus.enabled=False;worker.submit(RWS,0)
            self.wait_for(lambda:not worker.online);self.assertFalse(worker.submit(RWS,1))
            bus.enabled=True;self.wait_for(lambda:worker.online);time.sleep(.35)
            self.assertEqual(bus.state.rws,3)
        finally:worker.stop();worker.join(2)
    def test_no_ack_alone_online(self):
        class AckBus(DemoBus):
            def recv(self,timeout=0):return checked_frame(0x211,(RWS,1,0,0,0,0,0))
        worker=CanWorker('DEMO',EventMailbox(),AckBus);worker.start()
        try:time.sleep(.1);self.assertFalse(worker.online)
        finally:worker.stop();worker.join(2)

if __name__=='__main__':unittest.main(verbosity=2)
