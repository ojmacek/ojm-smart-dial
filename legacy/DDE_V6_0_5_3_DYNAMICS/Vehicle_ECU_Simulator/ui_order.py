"""Display order only; CAN mode values are unchanged."""
from protocol import RWS, MODES

def mode_order(command):
    return (3, 1, 2, 0) if command == RWS else (0, 1, 2, 3)

def mode_position(command, value):
    return mode_order(command).index(value)

def mode_value(command, position):
    return mode_order(command)[position]

def mode_labels(command):
    return tuple(MODES[command][value] for value in mode_order(command))
