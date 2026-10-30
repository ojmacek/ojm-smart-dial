"""Check preserved V6.0.1 and unchanged transport/hardware; never claim absent V5.6.1 was checked."""
from pathlib import Path
import hashlib
import json

ROOT=Path(__file__).resolve().parents[1]
PREVIOUS=ROOT.parent/'DDE_V6_0_1_DYNAMICS'
def current_path(name):
    if name.startswith('OJM_DDE_Dynamics/') and name.endswith('.h') and not name.endswith('build_opt.h'):
        name=name.replace('OJM_DDE_Dynamics/','OJM_DDE_Dynamics/src/')
    return ROOT/name
def run():
    checked=[]
    for line in (ROOT/'SHA256SUMS_V6_0_1.txt').read_text().splitlines():
        digest,name=line.split('  ',1)
        assert hashlib.sha256((PREVIOUS/name).read_bytes()).hexdigest()==digest,name
        checked.append(name)
    protected=['OJM_DDE_Dynamics/'+name for name in (
        'CanTransport.h','BaselineGraphics.h',
        'BaselineShapes.h','BaselineInput.h','build_opt.h','TitanAssets.h','DdeFonts.h','OjmLogo.h')]
    protected+=['Vehicle_ECU_Simulator/'+name for name in ('adapters.py',)]
    for name in protected:assert current_path(name).read_bytes()==(PREVIOUS/name).read_bytes(),name
    # Only startup/reset defaults change; all CAN codec bytes/validation stay identical.
    pairs=(('OJM_DDE_Dynamics/DdeProtocol.h',[('page=2,rws=3,tv=1,aero=2','page=0,rws=3,tv=1,aero=0'),('(command==0x10?3:4)','((command==0x10 || command==0x13)?3:4)')]),
           ('Vehicle_ECU_Simulator/protocol.py',[('DEFAULTS = {PAGE: 2, RWS: 3, TV: 1, AERO: 2}','DEFAULTS = {PAGE: 0, RWS: 3, TV: 1, AERO: 0}'),('page: int = 2','page: int = 0'),('aero: int = 2','aero: int = 0'),("AERO_MODES = ('AUTO', 'LOW DRAG', 'DOWNFORCE', 'AIR BRAKE')","AERO_MODES = ('AUTO', 'LOW DRAG', 'DOWNFORCE')"),("AERO_ACTUAL = AERO_MODES","AERO_ACTUAL = (*AERO_MODES, 'AIR BRAKE')")]))
    for name,replacements in pairs:
        content=current_path(name).read_text(encoding='utf-8')
        for new,old in replacements:content=content.replace(new,old)
        assert content==(PREVIOUS/name).read_text(encoding='utf-8'),name
    v602=[]
    for line in (ROOT/'SHA256SUMS_V6_0_2.txt').read_text().splitlines():
        digest,name=line.split('  ',1)
        assert hashlib.sha256((ROOT.parent/'DDE_V6_0_2_DYNAMICS'/name).read_bytes()).hexdigest()==digest,name
        v602.append(name)
    v603=[]
    for line in (ROOT/'SHA256SUMS_V6_0_3.txt').read_text().splitlines():
        digest,name=line.split('  ',1)
        assert hashlib.sha256((ROOT.parent/'DDE_V6_0_3_DYNAMICS'/name).read_bytes()).hexdigest()==digest,name
        v603.append(name)
    old_link=(PREVIOUS/'Vehicle_ECU_Simulator/can_link.py').read_text(encoding='utf-8')
    v604=[]
    for line in (ROOT/'SHA256SUMS_V6_0_4.txt').read_text().splitlines():
        digest,name=line.split('  ',1)
        assert hashlib.sha256((ROOT.parent/'DDE_V6_0_4_DYNAMICS'/name).read_bytes()).hexdigest()==digest,name
        v604.append(name)
    assert (ROOT/'assets/source/titan_top_minimal.png').read_bytes()==(ROOT.parent/'DDE_V6_0_4_DYNAMICS/assets/source/titan_top_minimal.png').read_bytes()
    new_link=(ROOT/'Vehicle_ECU_Simulator/can_link.py').read_text(encoding='utf-8')
    assert old_link.split('class CanWorker')[0]==new_link.split('class CanWorker')[0],'ACK engine and scheduler unchanged'
    assert old_link.split('class DemoBus:')[1]==new_link.split('class DemoBus:')[1],'DemoBus unchanged'
    before=(PREVIOUS/'OJM_DDE_Dynamics/OJM_DDE_Dynamics.ino').read_text()
    after=(ROOT/'OJM_DDE_Dynamics/OJM_DDE_Dynamics.ino').read_text().replace('#include "src/','#include "')
    assert before.split('#include "DynamicsModel.h"')[0]==after.split('#include "DynamicsModel.h"')[0]
    assert before.split('void setup(){')[1].split('Serial.println("OJM SYSTEMS')[0]==after.split('void setup(){')[1].split('Serial.println("OJM SYSTEMS')[0]
    report={'status':'PASS','preserved_v601_files':len(checked),'preserved_v602_files':len(v602),'preserved_v603_files':len(v603),'protocol_changes':['startup defaults','requested Air Brake value 3'],'ack_engine_unchanged':True,'unchanged_modules':protected,
            'hardware_setup_unchanged':True,'v561_present':False,
            'note':'V5.6.1 not present in this workspace; historical V5.6.1 audit remains in the previous revisions only.'}
    report['preserved_v604_files']=len(v604)
    report['original_top_reference_preserved']=True
    for name in ('OJM_DDE_Dynamics/DdeProtocol.h','Vehicle_ECU_Simulator/protocol.py','Vehicle_ECU_Simulator/can_link.py'):
        assert current_path(name).read_bytes()==(ROOT.parent/'DDE_V6_0_4_DYNAMICS'/name).read_bytes(),name
    report['v605_can_protocol_and_worker_unchanged_from_v604']=True
    prior=ROOT.parent/'DDE_V6_0_5_DYNAMICS';count=0
    for line in (ROOT/'SHA256SUMS_V6_0_5.txt').read_text().splitlines():
        digest,name=line.split('  ',1)
        assert hashlib.sha256((prior/name).read_bytes()).hexdigest()==digest,name
        if name.startswith('Vehicle_ECU_Simulator/'):
            assert (ROOT/name).read_bytes()==(prior/name).read_bytes(),name
        count+=1
    report['preserved_v605_files']=count
    report['pc_sources_and_exe_unchanged_from_v605']=True
    prior=ROOT.parent/'DDE_V6_0_5_1_DYNAMICS';count=0
    for line in (ROOT/'SHA256SUMS_V6_0_5_1.txt').read_text().splitlines():
        digest,name=line.split('  ',1)
        assert hashlib.sha256((prior/name).read_bytes()).hexdigest()==digest,name
        count+=1
    report['preserved_v6051_files']=count
    allowed={'OJM_DDE_Dynamics.ino','src/DdeRenderer.h'}
    unchanged=[]
    for source in (ROOT/'OJM_DDE_Dynamics').rglob('*'):
        if not source.is_file():continue
        relative=source.relative_to(ROOT/'OJM_DDE_Dynamics')
        if relative.as_posix() in allowed:continue
        assert source.read_bytes()==(prior/'OJM_DDE_Dynamics'/relative).read_bytes(),relative
        unchanged.append(relative.as_posix())
    report['v6052_firmware_changes']=['dirty rectangle alignment before restore/draw/transfer','serial version string']
    report['v6052_unchanged_firmware_files']=unchanged
    (ROOT/'validation/integrity_audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
if __name__=='__main__':run()
