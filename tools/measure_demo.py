"""Real-time software-only telemetry measurements; never opens CAN hardware."""
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Vehicle_ECU_Simulator'))
import json
import time
from can_link import *

bus=DemoBus();events=EventMailbox();worker=CanWorker('DEMO',events,lambda:bus);worker.start();result=[]
try:
    deadline=time.monotonic()+2
    while not worker.online and time.monotonic()<deadline:time.sleep(.005)
    assert worker.online
    for hz in (10,30,60):
        worker.configure(rate=hz);time.sleep(.2);start=time.monotonic();before=bus.cycles;time.sleep(2)
        elapsed=time.monotonic()-start;cycles=bus.cycles-before
        result.append({'target_hz':hz,'complete_cycles':cycles,'seconds':round(elapsed,4),'measured_hz':round(cycles/elapsed,2)})
    print(json.dumps(result,indent=2))
    (Path(__file__).resolve().parents[1]/'validation/telemetry_rates.json').write_text(json.dumps(result,indent=2)+'\n')
finally:worker.stop();worker.join(2)
