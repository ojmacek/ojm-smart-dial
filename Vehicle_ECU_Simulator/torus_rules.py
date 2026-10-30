"""The Performance Torus rules as Studio needs them (mirror of OJM_DDE_Dynamics/src/TorusContext.h).

Studio uses them to show what the Torus will lock and what its Auto modes pick from the car state Studio sends,
and the demo bus uses them to behave like the Torus. Keep TUNING equal to the firmware's TorusTuning.
"""
from dataclasses import dataclass
from protocol import *

@dataclass(frozen=True)
class Tuning:
    stand_kmh: float = 1
    launch_go_kmh: float = 5
    boost_min_kmh: float = 20
    off_lock_kmh: float = 120
    off_unlock_kmh: float = 115
    aero_downforce_kmh: float = 80
    aero_downforce_off_kmh: float = 75
    aero_brake_pct: int = 40
    aero_brake_off_pct: int = 25
    aero_brake_kmh: float = 60
    aero_brake_off_kmh: float = 40
    rws_stable_kmh: float = 60
    rws_stable_off_kmh: float = 55
    vehicle_timeout: float = .5
    drive_timeout: float = 1.2
    launch_timeout: float = 30
    boost: float = 10
    boost_cooldown: float = 20

TUNING = Tuning()
NONE, LAUNCH, BOOST = 0, 1, 2

class TorusRules:
    """Hysteresis state from the car state; same decisions as the firmware."""
    def __init__(self, tuning=TUNING):
        self.t = tuning; self.known = False; self.vehicle = Vehicle(); self.drive_mode = None
        self.off_locked = False; self.aero_auto = 0; self.rws_auto = 3
    def update(self, vehicle, drive_mode):
        t = self.t; self.known = vehicle is not None; self.vehicle = vehicle or Vehicle(); self.drive_mode = drive_mode
        v = self.vehicle
        self.off_locked = self.known and (v.speed > t.off_unlock_kmh if self.off_locked else v.speed > t.off_lock_kmh)
        if not self.known: self.aero_auto = 0
        else:
            braking = (v.brake >= t.aero_brake_off_pct and v.speed >= t.aero_brake_off_kmh) if self.aero_auto == 3 else \
                (v.brake >= t.aero_brake_pct and v.speed >= t.aero_brake_kmh)
            fast = v.speed >= (t.aero_downforce_off_kmh if self.aero_auto in (2, 3) else t.aero_downforce_kmh)
            self.aero_auto = 3 if braking else 2 if fast else 1
        # RWS Auto as a CAN value: 3 = neutral (no car data), 1 Stable, 2 Agile.
        if not self.known: self.rws_auto = 3
        else: self.rws_auto = 1 if v.speed >= (t.rws_stable_off_kmh if self.rws_auto == 1 else t.rws_stable_kmh) else 2
    def lock_reason(self, command, value):
        """None when allowed; otherwise the text the Torus shows."""
        if command in (RWS, TV) and value == 0 and self.off_locked: return f'Above {self.t.off_lock_kmh:.0f} km/h'
        if command == TV and value == 3 and self.drive_mode != 2: return 'Track only'
        return None
    def standing(self): return not self.known or self.vehicle.speed < self.t.stand_kmh
    def available_action(self, cooling):
        if self.standing(): return NONE if self.known and self.vehicle.state == CHARGING else LAUNCH
        if self.vehicle.state == DRIVE_STATE and self.vehicle.speed >= self.t.boost_min_kmh and not cooling: return BOOST
        return NONE
    def auto_name(self, command):
        """What Auto shows right now (None without car data)."""
        if not self.known: return None
        if command == AERO: return AERO_MODES[self.aero_auto]
        if command == RWS: return RWS_MODES[self.rws_auto]
        return None
