"""The defense scenario: Studio plays the car, captions explain what the committee sees.

Each step sets the car (state, target speed, brake), the simulated Drive Torus, the signal, may send one command,
and ends after its time or once the Torus reports what the step waits for. The steps are plain data: edit them here.
"""
from dataclasses import dataclass
from protocol import *

@dataclass(frozen=True)
class Step:
    caption: str
    seconds: float
    state: int = None          # PARK / DRIVE_STATE / CHARGING
    speed: float = None        # target km/h (the car model ramps to it)
    brake: bool = None
    drive: int = -2            # Drive Mode sent as the Drive Torus; None = not sent; -2 = unchanged
    dropout: bool = None
    command: tuple = None      # (command, value) sent once, as the Torus' remote control
    wait: str = None           # 'launch', 'launch_clear', 'boost', 'boost_clear'
    min_seconds: float = 0
    press: bool = False        # the presenter holds the knob (the demo bus presses it by itself)

STEPS = (
    Step('The Torus reads the car: parked, 0 km/h.', 4, state=PARK, speed=0, brake=False, drive=0, dropout=False),
    Step('Hold the knob in the menu: Launch Control arms.', 15, wait='launch', press=True),
    Step('Drive away: the start lights go out.', 8, state=DRIVE_STATE, speed=60, wait='launch_clear', min_seconds=3),
    Step('Auto follows the speed: Downforce and Stable from 80 km/h.', 6, speed=130),
    Step('Above 120 km/h, Off is locked: the Torus refuses it.', 5, command=(RWS, 0)),
    Step('Hold the knob again: Boost for 10 seconds.', 15, wait='boost', press=True),
    Step('Boost: full power while the halo counts down.', 12, wait='boost_clear', min_seconds=3),
    Step('Hard braking at speed: the wing becomes an air brake.', 4, brake=True, speed=45),
    Step('Track mode on the Drive Torus unlocks Drift.', 5, brake=False, drive=2, command=(TV, 3)),
    Step('Leaving Track falls back to Sport by itself.', 4, drive=1),
    Step('Parked and charging.', 5, state=CHARGING, speed=0),
    Step('Signal lost: the Torus falls back to safe defaults.', 5, dropout=True),
    Step('Signal back. One controller, aware of the car.', 4, dropout=False, state=PARK, speed=0, drive=0),
)

class ScenarioRunner:
    def __init__(self, steps=STEPS):
        self.steps = steps; self.index = -1; self.started = 0.; self.running = False; self.paused_at = None; self.pressed = False
    @property
    def step(self): return self.steps[self.index] if 0 <= self.index < len(self.steps) else None
    def play(self, now):
        if self.index < 0 or self.index >= len(self.steps): self.index = 0; self.started = now; self.pressed = False; self.entered = False
        elif self.paused_at is not None: self.started += now - self.paused_at
        self.paused_at = None; self.running = True
    def pause(self, now):
        if self.running: self.running = False; self.paused_at = now
    def stop(self): self.index = -1; self.running = False; self.paused_at = None
    def elapsed(self, now):
        if self.index < 0: return 0.
        return (self.paused_at if self.paused_at is not None else now) - self.started
    def progress(self, now):
        """0..1 along the whole scenario (steps count equally)."""
        if self.index < 0: return 0.
        if self.index >= len(self.steps): return 1.
        return (self.index + min(1., self.elapsed(now) / self.step.seconds)) / len(self.steps)
    def tick(self, now, studio):
        """Advance; studio applies a step (apply_step), reports bits (launch, boost) and presses in demo."""
        if not self.running or self.step is None: return
        st = self.step
        if not getattr(self, 'entered', False): studio.apply_step(st); self.entered = True
        e = self.elapsed(now)
        if st.press and not self.pressed and e > 1.6: self.pressed = True; studio.demo_press()
        done = e >= st.seconds
        if st.wait and e >= st.min_seconds:
            launch, boost = studio.action_bits()
            done = done or {'launch': launch, 'launch_clear': not launch, 'boost': boost, 'boost_clear': not boost}[st.wait]
        if done:
            self.index += 1; self.started = now; self.pressed = False; self.entered = False
            if self.index >= len(self.steps): self.running = False
