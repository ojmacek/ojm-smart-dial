"""DDE CAN v2. Classical CAN, explicit profiles, byte-exact codecs."""
from dataclasses import dataclass
import struct
import can

BITRATE = 500000
OFFLINE_SECONDS = 1.2
SEND_INTERVAL = .035
ACK_TIMEOUT = .250
MAX_ATTEMPTS = 3
MAGIC = 0xD2
PAGE, RWS, TV, AERO = 0x10, 0x11, 0x12, 0x13
PAGES = ('RWS', 'TORQUE VECTORING', 'ACTIVE AERO')
RWS_MODES = ('OFF', 'STABLE', 'AGILE', 'AUTO')
TV_MODES = ('OFF', 'NORMAL', 'SPORT', 'MAX')
AERO_MODES = ('AUTO', 'LOW DRAG', 'DOWNFORCE', 'AIR BRAKE')
AERO_ACTUAL = AERO_MODES
MODES = {PAGE: PAGES, RWS: RWS_MODES, TV: TV_MODES, AERO: AERO_MODES}
DEFAULTS = {PAGE: 2, RWS: 3, TV: 1, AERO: 2}

@dataclass(frozen=True)
class DialProfile:
    name: str
    state_id: int
    command_id: int
    ack_id: int
    telemetry_id: int
    implemented: bool = False

PROFILES = {
    'DYNAMICS': DialProfile('DYNAMICS', 0x201, 0x301, 0x211, 0x401, True),
    'DRIVE': DialProfile('DRIVE', 0x202, 0x302, 0x212, 0x402),
    'ENERGY': DialProfile('ENERGY', 0x203, 0x303, 0x213, 0x403),
}
DYNAMICS = PROFILES['DYNAMICS']

def checksum(data):
    value = 0
    for b in data: value ^= b
    return value

def frame(identifier, payload):
    return can.Message(arbitration_id=identifier, is_extended_id=False, data=payload)

def checked_frame(identifier, payload):
    data = bytes(payload)
    return frame(identifier, data + bytes((checksum(data),)))

def standard(message, identifier, length):
    return (message.arbitration_id == identifier and not message.is_extended_id
            and not message.is_remote_frame and not message.is_error_frame
            and not message.is_fd and message.dlc == length and len(message.data) == length)

def valid_value(command, value):
    return type(command) is int and type(value) is int and command in MODES and 0 <= value < len(MODES[command])

def describe(command, value):
    return f'{ {PAGE:"PAGE", RWS:"RWS", TV:"TV", AERO:"AERO"}[command]} {MODES[command][value]}'

@dataclass(frozen=True)
class DialState:
    page: int = 2
    rws: int = 3
    tv: int = 1
    aero: int = 2
    faults: int = 0
    alive: int = 0
    received_at: float = 0.

    def value(self, command):
        return {PAGE:self.page, RWS:self.rws, TV:self.tv, AERO:self.aero}[command]

def encode_state(state, profile=DYNAMICS):
    return checked_frame(profile.state_id, (MAGIC, state.page, state.rws, state.tv, state.aero, state.faults, state.alive & 15))

def decode_state(message, now, profile=DYNAMICS):
    if not standard(message, profile.state_id, 8): return None
    d = message.data
    if d[0] != MAGIC or checksum(d[:7]) != d[7] or d[5] & ~3 or d[6] > 15: return None
    if not all(valid_value(c, d[i]) for i,c in enumerate((PAGE,RWS,TV,AERO), 1)): return None
    return DialState(*d[1:7], now)

@dataclass
class Telemetry:
    speed: float = 0.
    front: float = 0.
    brake: float = 0.
    rear_requested: float = 0.
    rear_actual: float = 0.
    rws_actual: int = 3
    fl: float = 25.
    fr: float = 25.
    rl: float = 25.
    rr: float = 25.
    yaw: float = 0.
    aero_requested: float = 0.
    aero_actual: float = 0.
    aero_mode: int = 0
    tc_mask: int = 0
    faults: int = 0
    lateral: float = 0.
    bias: float = 0.

def _q(value, scale, low, high):
    return max(low, min(high, round(value * scale)))

def encode_telemetry(t, generation, profile=DYNAMICS):
    payloads = (
        struct.pack('<HhB', _q(t.speed,10,0,4000), _q(t.front,10,-450,450), _q(t.brake,1,0,100)),
        struct.pack('<hhB', _q(t.rear_requested,10,-150,150), _q(t.rear_actual,10,-150,150), t.rws_actual),
        struct.pack('<BBBBb', *(_q(v,1,0,100) for v in (t.fl,t.fr,t.rl,t.rr)), _q(t.yaw,1,-100,100)),
        bytes((_q(t.aero_requested,1,0,100), _q(t.aero_actual,1,0,100), t.aero_mode, t.tc_mask, t.faults)),
        struct.pack('<hbBB', _q(t.lateral,100,-300,300), _q(t.bias,1,-100,100), 0, 0),
    )
    return [checked_frame(profile.telemetry_id, bytes((0xA0+i,generation & 255))+p) for i,p in enumerate(payloads)]

def valid_fragment(d):
    if len(d) != 8 or not 0xA0 <= d[0] <= 0xA4 or checksum(d[:7]) != d[7]: return False
    p = d[2:7]
    if d[0] == 0xA0:
        speed,front,brake = struct.unpack('<HhB',p)
        return speed <= 4000 and abs(front) <= 450 and brake <= 100
    if d[0] == 0xA1:
        req,act,mode = struct.unpack('<hhB',p)
        return abs(req) <= 150 and abs(act) <= 150 and mode <= 3
    if d[0] == 0xA2:
        *torque,yaw = struct.unpack('<BBBBb',p)
        return max(torque) <= 100 and abs(yaw) <= 100
    if d[0] == 0xA3:
        return p[0] <= 100 and p[1] <= 100 and p[2] <= 3 and p[3] <= 15 and p[4] <= 3
    lateral,bias,r0,r1 = struct.unpack('<hbBB',p)
    return abs(lateral) <= 300 and abs(bias) <= 100 and r0 == r1 == 0

class TelemetryAssembler:
    """Five bounded slots. Publish only complete, same-generation snapshots."""
    def __init__(self): self.reset()
    def reset(self):
        self.generation = None
        self.parts = {}
        self.last_at = 0.
        self.complete = False
    def accept(self, message, now, profile=DYNAMICS):
        if not standard(message,profile.telemetry_id,8) or not valid_fragment(message.data): return None
        d=message.data; generation=d[1]
        if now-self.last_at > .3: self.reset()
        if self.generation is not None:
            delta=(generation-self.generation)&255
            if delta >= 128 or (delta == 0 and self.complete): return None
        if generation != self.generation:
            self.parts.clear(); self.complete=False; self.generation=generation
        self.last_at=now
        self.parts[d[0]]=bytes(d[2:7])
        if len(self.parts) != 5: return None
        self.complete=True
        speed,front,brake=struct.unpack('<HhB',self.parts[0xA0])
        req,act,rws=struct.unpack('<hhB',self.parts[0xA1])
        fl,fr,rl,rr,yaw=struct.unpack('<BBBBb',self.parts[0xA2])
        aero_req,aero_act,aero_mode,tc,faults=self.parts[0xA3]
        lateral,bias,_,_=struct.unpack('<hbBB',self.parts[0xA4])
        return Telemetry(speed/10,front/10,brake,req/10,act/10,rws,fl,fr,rl,rr,yaw,
                         aero_req,aero_act,aero_mode,tc,faults,lateral/100,bias)
