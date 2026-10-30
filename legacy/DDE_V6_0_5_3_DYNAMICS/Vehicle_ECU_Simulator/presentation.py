"""The dial's mode-driven illustration, mirrored on PC; not ECU telemetry."""
import math
from protocol import Telemetry

def local_presentation(state,seconds):
    wave=math.sin((seconds%12)*2*math.pi/12)
    front=22+6*wave
    auto_wave=0 if abs(wave)<.12 else math.copysign((abs(wave)-.12)/.88,wave)
    factor=(0,.26,-.46,.40*auto_wave)[state.rws]
    strength=(0,.32,.68,1)[state.tv];load=(0,26,50,74)[state.tv]
    split=(.32+.18*wave)*strength
    torque=[max(0,min(100,load*(1+.18*strength if i>=2 else 1-.18*strength)*(1+split if i%2 else 1-split))) for i in range(4)]
    aero=(40+25*wave,5,75,100)[state.aero]
    return Telemetry(front=front,rear_actual=front*factor,rear_requested=front*factor,
                     rws_actual=state.rws,fl=torque[0],fr=torque[1],rl=torque[2],rr=torque[3],
                     aero_actual=aero,aero_requested=aero,aero_mode=state.aero)
