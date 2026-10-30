"""V5.6 latest-value command engine and single-owner SLCAN worker, extended for DDE."""
from collections import deque
from dataclasses import dataclass, replace
import queue
import secrets
import threading
import time
import can
from protocol import *
from ecu import DynamicsEcu
from adapters import MODULES
from torus_rules import TorusRules, NONE, LAUNCH, BOOST

class EventMailbox:
    """Bounded log events plus coalesced state/stats/telemetry for the Tk thread."""
    def __init__(self):
        self.lock=threading.Lock(); self.events=deque(maxlen=256); self.latest={}; self.dropped=0
    def put(self,event):
        kind=event[0]
        with self.lock:
            if kind in ('state','stats','telemetry','inputs'):
                self.latest[kind]=event
            else:
                if kind=='connection' and event[1]!='ONLINE':
                    self.latest.pop('state',None); self.latest.pop('telemetry',None)
                if len(self.events)==self.events.maxlen: self.dropped+=1
                self.events.append(event)
    def get_nowait(self):
        with self.lock:
            if self.events: return self.events.popleft()
            if self.latest: return self.latest.pop(next(iter(self.latest)))
        raise queue.Empty

@dataclass
class Pending:
    command:int
    value:int
    sequence:int
    requested_at:float
    attempts:int=0
    last_sent_at:float=-1e9

class CommandEngine:
    def __init__(self,emit,seed=None,profile=DYNAMICS,validator=valid_value,describer=describe):
        self.emit=emit; self.profile=profile
        self.valid=validator; self.describe=describer
        self.sequence=secrets.randbits(32) if seed is None else seed
        self.pending={}; self.last_sent={}; self.retries=0; self.superseded=0; self.missed=0
    def submit(self,command,value,now):
        if not self.valid(command,value): raise ValueError('Invalid command/value')
        old=self.pending.get(command)
        if old and old.value==value: return
        if old:
            self.superseded+=1; self.emit('superseded',self.describe(old.command,old.value))
        self.sequence=(self.sequence+1)&0xffffffff
        self.pending[command]=Pending(command,value,self.sequence,now)
    def service(self,now,send):
        for command,item in list(self.pending.items()):
            if now-self.last_sent.get(command,-1e9)<SEND_INTERVAL: continue
            if item.attempts and now-item.last_sent_at<ACK_TIMEOUT: continue
            if item.attempts>=MAX_ATTEMPTS:
                del self.pending[command]; self.missed+=1
                self.emit('failed',(command,item.value,'No ACK: '+self.describe(command,item.value))); continue
            if item.attempts: self.retries+=1
            payload=bytes((command,item.value))+item.sequence.to_bytes(4,'little')
            item.attempts+=1; item.last_sent_at=now; self.last_sent[command]=now
            send(frame(self.profile.command_id,payload))
            self.emit('tx',f'{self.describe(command,item.value)} · seq {item.sequence:08X} · try {item.attempts}')
    def acknowledge(self,message,now):
        if not standard(message,self.profile.ack_id,8): return False
        d=message.data
        if checksum(d[:7])!=d[7] or d[6] not in (0,1): return False
        item=self.pending.get(d[0])
        if not item or not item.attempts or item.value!=d[1] or item.sequence!=int.from_bytes(d[2:6],'little'): return False
        del self.pending[item.command]
        if d[6]: self.emit('failed',(item.command,item.value,'Rejected: '+self.describe(item.command,item.value)))
        else: self.emit('confirmed',(item.command,item.value,(now-item.requested_at)*1000,self.describe(item.command,item.value)))
        return True
    def cancel(self): self.pending.clear(); self.last_sent.clear()

class TelemetryScheduler:
    """One replaceable cycle, never a history of cycles; no catch-up after stalls."""
    def __init__(self,profile=DYNAMICS,encoder=encode_telemetry):
        self.profile=profile; self.hz=30; self.due=0.; self.generation=0; self.pending=deque(maxlen=5); self.sent_cycles=0; self.dropped_cycles=0
        self.encoder=encoder
    def schedule(self,now,telemetry):
        if now<self.due: return
        if self.pending: self.dropped_cycles+=1
        self.generation=(self.generation+1)&255
        self.pending=deque(self.encoder(telemetry,self.generation,self.profile),maxlen=5)
        period=1/self.hz
        self.due=now+period if self.due<=0 or now-self.due>period else self.due+period
        self.sent_cycles+=1
    def service(self,send):
        # One bounded, coherent five-frame cycle per worker visit. Sending one
        # fragment per Windows wait tick can starve a 30/60 Hz cycle entirely.
        for _ in range(5):
            if not self.pending:break
            if send(self.pending[0]) is False:break
            self.pending.popleft()
    def cancel(self): self.pending.clear(); self.due=0.

class CanWorker(threading.Thread):
    def __init__(self,port,events,bus_factory=None,profile=DYNAMICS,simulation=True):
        if not profile.implemented: raise ValueError('Profile not implemented')
        super().__init__(name='CAN transport',daemon=True)
        self.port=port; self.events=events; self.bus_factory=bus_factory; self.profile=profile
        self.module=MODULES[profile.name]
        self.stop_event=threading.Event(); self.lock=threading.Lock()
        self.requests={}; self.settings={}; self.action=None; self.online=False
        self.ecu=self.module.ecu_factory(); self.rate=30; self.simulation=simulation
        # V6.0.7 Studio: the car (0x100) and, until Torus 3 exists, the Drive Torus (0x202) are sent from here.
        self.vehicle=Vehicle(); self.vehicle_on=True; self.drive_sim=None
    def emit(self,kind,payload): self.events.put((kind,payload,time.time()))
    def submit(self,command,value):
        if not self.module.command_validator(command,value): return False
        with self.lock:
            if not self.online: return False
            self.requests[command]=(value,time.monotonic())
        return True
    def configure(self,**values):
        with self.lock: self.settings.update(values)
    def scenario(self,action):
        with self.lock: self.action=action
    def stop(self): self.stop_event.set()
    def run(self):
        bus=None; engine=CommandEngine(self.emit,profile=self.profile,validator=self.module.command_validator,describer=self.module.command_describer)
        scheduler=TelemetryScheduler(self.profile,self.module.telemetry_encoder)
        state=self.module.state_factory(); last_state=0.; last_stats=0.; last_model=time.monotonic(); previous_alive=None
        scenario_commands={}; rx=tx=gaps=bad=0
        last_vehicle=last_drive=-1e9; vehicle_alive=drive_alive=0; logged_vehicle=None; logged_drive=-1; logged_at=-1e9
        def send(message):
            nonlocal tx
            try: bus.send(message,timeout=.01); tx+=1; return True
            except (can.CanError,OSError) as exc: self.emit('error','TX: '+str(exc)); return False
        try:
            self.emit('connection','CONNECTING')
            bus=self.bus_factory() if self.bus_factory else can.Bus(interface='slcan',channel=self.port,bitrate=BITRATE,
                timeout=.001,sleep_after_open=2,can_filters=[{'can_id':v,'can_mask':0x7ff,'extended':False}
                    for v in (self.profile.state_id,self.profile.ack_id)])
            self.emit('connection','WAITING FOR DDE V2')
            while not self.stop_event.is_set():
                now=time.monotonic()
                # Expire before processing late ACKs or a recovered state.
                if self.online and now-last_state>OFFLINE_SECONDS:
                    with self.lock: self.online=False; self.requests.clear()
                    engine.cancel(); scheduler.cancel(); self.ecu.stop(); scenario_commands.clear(); previous_alive=None
                    self.emit('connection','OFFLINE')
                for _ in range(32):
                    message=bus.recv(timeout=0)
                    if message is None: break
                    rx+=1; received=time.monotonic()
                    if message.arbitration_id==self.profile.state_id:
                        decoded=self.module.state_decoder(message,received,self.profile)
                        if decoded is None: bad+=1; continue
                        state=decoded; last_state=received
                        if previous_alive is not None:
                            delta=(state.alive-previous_alive)&15
                            if delta>1: gaps+=delta-1
                        previous_alive=state.alive
                        with self.lock: was_online=self.online; self.online=True
                        if not was_online: self.emit('connection','ONLINE')
                        self.emit('state',state)
                    elif message.arbitration_id==self.profile.ack_id and self.online:
                        engine.acknowledge(message,received)
                now=time.monotonic()
                with self.lock:
                    requests,self.requests=self.requests,{}
                    settings,self.settings=self.settings,{}
                    action,self.action=self.action,None
                for key,value in settings.items():
                    if key=='rate' and value in (10,30,60): scheduler.hz=self.rate=value
                    elif key=='vehicle': self.vehicle=value
                    elif key=='vehicle_on': self.vehicle_on=bool(value)
                    elif key=='drive_sim': self.drive_sim=value
                    elif hasattr(self.ecu.inputs,key): setattr(self.ecu.inputs,key,value)
                if action=='LongPress' and hasattr(bus,'long_press'): bus.long_press(); action=None
                # The car and the simulated Drive Torus go out whether or not the Torus answers yet.
                if self.vehicle_on and now-last_vehicle>=VEHICLE_PERIOD:
                    if send(encode_vehicle(self.vehicle,vehicle_alive)): vehicle_alive=(vehicle_alive+1)&15
                    last_vehicle=now
                    v=self.vehicle;key=(v.state,round(v.speed),v.brake)
                    if key!=logged_vehicle and (logged_vehicle is None or key[0]!=logged_vehicle[0] or key[2]!=logged_vehicle[2] or now-logged_at>=.4):
                        self.emit('tx',f'VEHICLE {VEHICLE_STATES[v.state]} {v.speed:.0f} km/h'+(' BRAKE' if v.brake else ''));logged_vehicle=key;logged_at=now
                if self.drive_sim is not None and now-last_drive>=DRIVE_PERIOD:
                    if send(encode_drive_state(self.drive_sim,drive_alive)): drive_alive=(drive_alive+1)&15
                    last_drive=now
                if (self.drive_sim if self.drive_sim is not None else -1)!=logged_drive:
                    logged_drive=self.drive_sim if self.drive_sim is not None else -1
                    self.emit('tx','DRIVE MODE '+(DRIVE_MODES[self.drive_sim] if self.drive_sim is not None else 'not sent'))
                if action=='Stop': self.ecu.stop(); scenario_commands.clear()
                elif action=='Reset':
                    self.ecu.reset(); scenario_commands.clear()
                    if self.online:
                        requests.update({c:(v,now) for c,v in self.module.defaults.items()})
                elif action in self.module.pages and self.online:
                    self.ecu.start(action,now); scenario_commands.clear()
                if self.online:
                    desired=self.ecu.scenario_values(now)
                    for command,value in desired.items():
                        if scenario_commands.get(command)!=value:
                            requests[command]=(value,now); scenario_commands[command]=value
                    for command,(value,requested_at) in requests.items(): engine.submit(command,value,requested_at)
                    engine.service(now,send)  # commands before telemetry
                if self.simulation and now-last_model>=1/60:
                    telemetry=self.ecu.step(now-last_model,state); last_model=now
                    self.emit('telemetry',telemetry)
                if self.online and self.simulation:
                    scheduler.schedule(now,self.ecu.telemetry); scheduler.service(send)
                if now-last_stats>=.1:
                    self.emit('inputs',replace(self.ecu.inputs))
                    self.emit('stats',dict(rx=rx,tx=tx,retry=engine.retries,superseded=engine.superseded,
                        missed=engine.missed,gaps=gaps,invalid=bad,pending=len(engine.pending),
                        cycles=scheduler.sent_cycles,dropped=scheduler.dropped_cycles,hz=scheduler.hz,
                        gui_dropped=getattr(self.events,'dropped',0)))
                    last_stats=now
                self.stop_event.wait(.001)
        except (can.CanError,OSError,ValueError) as exc: self.emit('error',str(exc))
        finally:
            with self.lock: self.online=False; self.requests.clear()
            engine.cancel(); scheduler.cancel()
            if bus:
                try: bus.shutdown()
                except (can.CanError,OSError): pass
            self.emit('connection','DISCONNECTED'); self.emit('stopped',None)

class DemoBus:
    """Bounded in-memory Performance Torus. No port access, with real codec validation and the Torus rules
    (V6.0.6.7: car state 0x100, Drive Mode 0x202, locks with ACK 1, Auto picks, Launch/Boost via long_press)."""
    def __init__(self):
        self.state=DialState(); self.acks={}; self.dirty=True; self.last_state=-1e9
        self.assembler=TelemetryAssembler(); self.telemetry=Telemetry(); self.cycles=0
        self.enabled=True; self.rules=TorusRules(); self.vehicle=None; self.vehicle_at=-1e9; self.drive=None; self.drive_at=-1e9
        self.action=NONE; self.action_at=0.; self.cooldown_until=-1e9
    def _status(self,bits):
        if bits!=self.state.faults: self.state=replace(self.state,faults=bits); self.dirty=True
    def _service(self,now):
        t=self.rules.t
        vehicle=self.vehicle if now-self.vehicle_at<=t.vehicle_timeout else None
        drive=self.drive if now-self.drive_at<=t.drive_timeout else None
        self.rules.update(vehicle,drive)
        if self.state.tv==3 and drive!=2: self.state=replace(self.state,tv=2); self.dirty=True
        v=self.rules.vehicle
        if self.action==LAUNCH:
            if self.rules.known and v.speed>=t.launch_go_kmh: self.action=NONE
            elif now-self.action_at>=t.launch_timeout or (self.rules.known and v.state==CHARGING): self.action=NONE
        elif self.action==BOOST and (now-self.action_at>=t.boost or not self.rules.known or v.speed<t.stand_kmh):
            self.action=NONE; self.cooldown_until=now+t.boost_cooldown
        self._status((1 if self.action==LAUNCH else 0)|(2 if self.action==BOOST else 0))
    def long_press(self):
        """The knob held in the menu (demo only: the real Torus is pressed by hand)."""
        now=time.monotonic(); self._service(now)
        if self.action==LAUNCH: self.action=NONE
        elif self.action==NONE:
            self.action=self.rules.available_action(now<self.cooldown_until); self.action_at=now
        self._service(now)
    def send(self,message,timeout=0):
        if not self.enabled: return
        now=time.monotonic()
        if message.arbitration_id==VEHICLE_ID:
            v=decode_vehicle(message)
            if v: self.vehicle=v; self.vehicle_at=now
        elif message.arbitration_id==PROFILES['DRIVE'].state_id:
            m=decode_drive_mode(message)
            if m is not None: self.drive=m; self.drive_at=now
        elif standard(message,DYNAMICS.command_id,6) or standard(message,DYNAMICS.command_id,2):
            d=message.data
            if d[0] not in MODES: return
            self._service(now)
            accepted=valid_value(d[0],d[1]) and self.rules.lock_reason(d[0],d[1]) is None
            if accepted:
                key={PAGE:'page',RWS:'rws',TV:'tv',AERO:'aero'}[d[0]]
                self.state=replace(self.state,**{key:d[1]}); self.dirty=True
            if len(d)==6: self.acks[d[0]]=checked_frame(DYNAMICS.ack_id,bytes(d)+bytes((0 if accepted else 1,)))
        elif message.arbitration_id==DYNAMICS.telemetry_id:
            t=self.assembler.accept(message,now)
            if t: self.telemetry=t; self.cycles+=1
    def recv(self,timeout=0):
        if not self.enabled: return None
        if self.acks: return self.acks.pop(next(iter(self.acks)))
        now=time.monotonic(); self._service(now)
        if self.dirty or now-self.last_state>=.2:
            self.last_state=now; self.dirty=False
            result=encode_state(self.state)
            self.state=replace(self.state,alive=(self.state.alive+1)&15)
            return result
        return None
    def shutdown(self): pass
