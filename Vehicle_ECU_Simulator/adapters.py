"""Extension points for future dials. Only DYNAMICS is implemented in V6.0."""
from dataclasses import dataclass
from protocol import *
from ecu import DynamicsEcu

@dataclass(frozen=True)
class DialModule:
    profile: DialProfile
    pages: tuple
    ecu_factory: object = None
    state_factory: object = None
    state_decoder: object = None
    telemetry_encoder: object = None
    defaults: object = None
    command_validator: object = None
    command_describer: object = None

MODULES={
    'DYNAMICS':DialModule(DYNAMICS,PAGES,DynamicsEcu,DialState,decode_state,encode_telemetry,DEFAULTS,valid_value,describe),
    'DRIVE':DialModule(PROFILES['DRIVE'],('Drive Modes + Traction Control','Performance','G-Meter')),
    'ENERGY':DialModule(PROFILES['ENERGY'],('Regeneration','Energy Flow','Battery / Charging')),
}
