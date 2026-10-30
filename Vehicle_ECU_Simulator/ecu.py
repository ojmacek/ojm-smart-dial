"""Deterministic demonstration ECU; no claim of a production vehicle control law."""
from dataclasses import dataclass, replace
import math
from protocol import Telemetry, DialState

@dataclass
class EcuInputs:
    speed: float = 35.
    front: float = 12.
    brake: float = 0.
    rear_requested: float = 0.
    rear_actual: float = 0.
    rear_manual_target: bool = False
    rear_manual_actual: bool = False
    rws_lag: float = .35
    rws_fault: bool = False
    fl: float = 25.
    fr: float = 25.
    rl: float = 25.
    rr: float = 25.
    torque_manual: bool = False
    lateral: float = .25
    yaw: float = 0.
    bias: float = 0.
    tc_mask: int = 0
    aero_requested: float = 0.
    aero_actual: float = 0.
    aero_manual_target: bool = False
    aero_manual_actual: bool = False
    aero_lag: float = .7
    aero_stuck: bool = False
    aero_fault: bool = False

class DynamicsEcu:
    def __init__(self):
        self.inputs=EcuInputs(); self.telemetry=Telemetry()
        self.scenario=None; self.scenario_at=0.; self.mismatch=0.
    def start(self,name,now):
        if name not in ('RWS','TORQUE VECTORING','ACTIVE AERO'): raise ValueError(name)
        self.inputs=EcuInputs(); self.scenario=name; self.scenario_at=now
    def stop(self): self.scenario=None
    def reset(self): self.__init__()
    def scenario_values(self,now):
        """Returns desired dial modes; caller submits through the usual ACK engine."""
        if not self.scenario: return {}
        from protocol import PAGE,RWS,TV,AERO
        t=now-self.scenario_at; i=self.inputs
        if self.scenario=='RWS':
            t%=20
            i.speed=15+min(1,max(0,(t-6)/8))*110
            i.front=22*math.sin(math.pi*min(1,max(0,(t-1)/17)))
            return {PAGE:0,RWS:3}
        if self.scenario=='TORQUE VECTORING':
            t%=16; i.speed=95; i.front=18*math.sin(t*math.pi/8)
            i.lateral=.9*math.sin(t*math.pi/8); i.yaw=i.lateral*50; i.bias=i.lateral*35
            i.tc_mask=2 if 5<t<6 else 4 if 12<t<13 else 0
            return {PAGE:1,TV:2}
        t%=24
        i.speed=min(190,t*15) if t<15 else max(30,190-(t-15)*35)
        i.brake=95 if 15<t<17 else 0
        mode=1 if t<8 else 2 if t<19 else 0
        return {PAGE:2,AERO:mode}
    def step(self,dt,state=DialState()):
        dt=max(0,min(.1,dt)); i=self.inputs; t=self.telemetry
        t.speed=max(0,min(400,i.speed)); t.front=max(-45,min(45,i.front)); t.brake=max(0,min(100,i.brake))
        # AUTO blends counter-phase to same-phase with speed. No discontinuous wheel jump.
        blend=max(0,min(1,(t.speed-35)/55)); blend=blend*blend*(3-2*blend)
        factor={0:0.,1:.16,2:-.28,3:-.28+.44*blend}[state.rws]
        t.rws_actual=0 if i.rws_fault else (2 if blend<.5 else 1) if state.rws==3 else state.rws
        rear=max(-15,min(15,i.rear_requested if i.rear_manual_target else t.front*factor))
        if state.rws==0 or i.rws_fault: rear=0.
        t.rear_requested=rear
        actual=i.rear_actual if i.rear_manual_actual else t.rear_actual+(rear-t.rear_actual)*(1-math.exp(-dt/max(.03,i.rws_lag)))
        t.rear_actual=max(-15,min(15,actual))
        if not i.rear_manual_target:i.rear_requested=t.rear_requested
        if not i.rear_manual_actual:i.rear_actual=t.rear_actual
        strength=(0,.25,.65,1.)[state.tv]
        split=max(-.85,min(.85,i.lateral*.7))*strength
        torque=(i.fl,i.fr,i.rl,i.rr) if i.torque_manual else (25*(1-split),25*(1+split),25*(1-split),25*(1+split))
        if state.tv==0: torque=(25,25,25,25)
        for index,(name,value) in enumerate(zip(('fl','fr','rl','rr'),torque)):
            if not i.torque_manual and i.tc_mask&(1<<index):value*=.55
            old=getattr(t,name); setattr(t,name,max(0,min(100,old+(value-old)*(1-math.exp(-dt/.12)))))
        t.yaw=max(-100,min(100,i.yaw)); t.bias=max(-100,min(100,i.bias)); t.lateral=max(-3,min(3,i.lateral))
        t.tc_mask=i.tc_mask & 15
        airbrake=state.aero==3 or (t.brake>=75 and t.speed>=35)
        t.aero_mode=3 if airbrake else state.aero
        target=100. if airbrake else (max(0,min(75,(t.speed-50)*.48)) if state.aero==0 else 5. if state.aero==1 else 75.)
        if i.aero_manual_target and not airbrake: target=i.aero_requested
        t.aero_requested=max(0,min(100,target))
        lag=.12 if airbrake else max(.35,i.aero_lag)
        if i.aero_manual_actual: t.aero_actual=max(0,min(100,i.aero_actual))
        elif not i.aero_stuck: t.aero_actual+=(t.aero_requested-t.aero_actual)*(1-math.exp(-dt/lag))
        self.mismatch=self.mismatch+dt if abs(t.aero_requested-t.aero_actual)>12 else 0.
        if not i.aero_manual_target:i.aero_requested=t.aero_requested
        if not i.aero_manual_actual:i.aero_actual=t.aero_actual
        if not i.torque_manual:
            for name in ('fl','fr','rl','rr'):setattr(i,name,getattr(t,name))
        t.faults=(1 if i.rws_fault else 0)|(2 if i.aero_fault or self.mismatch>=2 else 0)
        return replace(t)
