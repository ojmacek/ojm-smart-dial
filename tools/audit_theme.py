"""V6.0.7 integrity audit: every earlier revision unchanged; versus V6.0.6.9 the firmware only by its version, the PC app
replaced by CANertia Studio (Qt)."""
from pathlib import Path
import hashlib,json

ROOT=Path(__file__).resolve().parents[1]
PREVIOUS=ROOT.parent/'DDE_V6_0_6_9_DYNAMICS'
# Firmware: DMA panel transfer (FastQspi.h/QspiStream.h), scanline wing fill, chunk-culled polylines, row-indexed
# outline, run-walked glyphs, memset restore, line-following car damage, per-item segments/bar/wheels/ring damage.
# V6.0.6.3: row runs for long lines, cross-product arcs without sqrt in the core, exact arc boxes, optimised ring
# bands, -O2 for the drawing code, Serial breakdown (update/restore/paint/work).
# V6.0.6.4: internal-RAM render pool (render target halo_px), division-free exact blend, transfer from the pool.
# V6.0.6.5: painting on both cores (bands of rows for large rectangles, startup too), packed 5-bit-alpha blend,
# ring/arc tables, glass edge as a scene item, queued segmented transfer overlapped with painting (own device,
# hardware CS), more internal pool regions, edit pulse at 30 Hz while the Aero zoom moves, development bench.
# V6.0.6.6: the zoomed Titan fades out before the ring (radial line fade), the laser head ends exactly at the end of
# the track, every pixel transfer is one segment (big windows as several windows of whole rows), shadow check.
# V6.0.6.7 (Performance Torus): car state 0x100 and Drive Mode 0x202 received (TorusContext.h), RWS shown Off/Auto/
# Stable/Agile, TV Off/Auto/Sport/Drift (CAN values unchanged), Auto picks from speed/brake, Off locked above 120 km/h,
# Drift only in Track, long press in the menu = Launch (standing) / Boost (driving) with the halo ring, state byte 5
# = Launch/Boost bits, read-only button hold state, Serial car simulation.
# V6.0.6.8: action view - Launch and Boost show the side view on every page (start lights, airflow), version.
# V6.0.6.9: locked options drawn as grey segments; an end bounce on a lock shows the option and why (0.8 s).
# V6.0.7: firmware unchanged except the version; the Tk mirror is replaced by CANertia Studio (PySide6): Qt startup
# port, Torus rules mirror, Torus demo bus, scenario, car (0x100) and Drive Torus (0x202) simulation.
FW_CHANGED={'OJM_DDE_Dynamics.ino'}
FW_NEW=set()
PC_CHANGED={'main.py','can_link.py','protocol.py','requirements.txt','INSTALL.bat'}
PC_NEW={'startup.py','studio_paint.py','studio_theme.py','studio_widgets.py','torus_rules.py','scenario.py'}
PC_REMOVED={'halo.py','controls.py','theme.py','ui_order.py','OJM_DDE_Virtual_ECU.spec','BUILD_EXE.bat','OJM_Logo.png'}

def files(base,folder):
    skip=('__pycache__','build','dist','.venv')
    return {p.relative_to(base/folder).as_posix() for p in (base/folder).rglob('*') if p.is_file() and not any(s in p.parts for s in skip)}

def compare(folder,changed,new=frozenset(),removed=frozenset()):
    now,before=files(ROOT,folder),files(PREVIOUS,folder)
    assert now-before==set(new) and before-now==set(removed),('file set changed',folder,now^before)
    unchanged=sorted(now-changed-set(new))
    for rel in unchanged:assert (ROOT/folder/rel).read_bytes()==(PREVIOUS/folder/rel).read_bytes(),rel
    return unchanged

def run():
    preserved={}
    for manifest in sorted(ROOT.glob('SHA256SUMS_V*.txt')):
        version=manifest.stem.removeprefix('SHA256SUMS_');previous=ROOT.parent/('DDE_'+version+'_DYNAMICS');count=0
        for line in manifest.read_text(encoding='utf-8').splitlines():
            digest,name=line.split('  ',1)
            assert hashlib.sha256((previous/name).read_bytes()).hexdigest()==digest,(version,name);count+=1
        preserved[version]=count
    assert 'V6_0_6_9' in preserved,'V6.0.6.9 manifest missing'
    fw=compare('OJM_DDE_Dynamics',FW_CHANGED,FW_NEW);pc=compare('Vehicle_ECU_Simulator',PC_CHANGED,PC_NEW,PC_REMOVED)
    for keep in ('src/BaselineGraphics.h','src/FastCoverage.h','src/FastQspi.h','src/HaloStartup.h','src/DdeRenderer.h','src/HaloDraw.h','src/CanTransport.h','src/BaselineInput.h','src/HaloUi.h','src/TorusContext.h','src/DdeNavigation.h','src/DynamicsModel.h','src/DdeProtocol.h'):
        assert keep in fw,keep
    # The encoder hysteresis and the button debounce/long press are unchanged (only a read-only hold view was added).
    read=lambda base,name:(base/'OJM_DDE_Dynamics/src'/name).read_text(encoding='utf-8').replace(chr(13),'')
    enc=lambda t:t.split('static int16_t consume_encoder_delta() {')[1].split('static ButtonEvent read_encoder_button()')[0].split('// V6.0.6.7')[0]
    assert enc(read(ROOT,'BaselineInput.h'))==enc(read(PREVIOUS,'BaselineInput.h')),'encoder hysteresis unchanged'
    button=lambda t:''.join(l for l in t.split('static ButtonEvent read_encoder_button() {')[1].split(chr(10)) if 'g_button_down' not in l)
    assert button(read(ROOT,'BaselineInput.h'))==button(read(PREVIOUS,'BaselineInput.h')),'debounce and long press unchanged'
    # The CAN worker keeps its driver setup, bus recovery, ACK priority and heartbeat; it only receives two more IDs.
    can_now,can_before=read(ROOT,'CanTransport.h'),read(PREVIOUS,'CanTransport.h')
    for line in can_before.split(chr(10)):assert line in can_now,('CAN worker line removed or changed',line)
    for keep in ('ecu.py','adapters.py'):assert keep in pc,keep
    before=(PREVIOUS/'OJM_DDE_Dynamics/OJM_DDE_Dynamics.ino').read_text(encoding='utf-8')
    after=(ROOT/'OJM_DDE_Dynamics/OJM_DDE_Dynamics.ino').read_text(encoding='utf-8')
    nl=chr(10)
    added=('#pragma GCC optimize','HALO_COUNT','HALO_WORK_COUNTER','halo_work','V6.0.6.3: the drawing code','Device work counter','SERIAL_TEST_INPUT','UI_REPORT_MS','DevBench.h','Development','SHADOW_CHECK')
    head=lambda t:nl.join(l for l in t.replace(chr(13),'').split('#include "src/BaselineGraphics.h"')[0].split(nl) if not any(a in l for a in added))
    assert head(before)==head(after),'pins, QSPI, TE, display and encoder configuration unchanged'
    setup=lambda t:t.split('void setup(){')[1].split('void loop(){')[0]
    # Setup may differ only by the version and the transfer start (FAST_QSPI, the queued device and its report).
    keep=lambda t:nl.join(l for l in t.replace(chr(13),'').split(nl) if 'FAST_QSPI' not in l and 'fast_qspi_begin' not in l and 'render_pool_begin' not in l and 'qspi' not in l and 'fillRect' not in l and l.strip()!='#endif')
    assert keep(setup(after)).replace('V6.0.7 /','V6.0.6.9 /')==keep(setup(before)),'setup: version only'
    loop=after.split('void loop(){')[1]
    assert loop.index('consume_encoder_delta')<loop.index('model_service')<loop.index('navigation_input')<loop.index('render_tick'),'loop order kept'
    renderer=(ROOT/'OJM_DDE_Dynamics/src/DdeRenderer.h').read_text(encoding='utf-8')
    assert 'x&=~1;y&=~1;right=(right+1)&~1;bottom=(bottom+1)&~1;' in renderer,'CO5300 even windows before restore/draw/pack'
    for name in ('CAN_PROTOCOL.md','assets/source/logo_contours.json','assets/preview/halo_logo.png','assets/fonts/Michroma-Regular.ttf'):
        assert (ROOT/name).read_bytes()==(PREVIOUS/name).read_bytes(),name
    assert json.loads((ROOT/'validation/font_check.json').read_text())['actual_font']['family']=='Michroma'
    report={'status':'PASS','baseline':'V6.0.6.9','new_firmware_modules':sorted(FW_NEW),'preserved_revisions':preserved,'v561_present':False,
        'unchanged_firmware_modules':fw,'unchanged_pc_modules':pc,'can_protocol':'unchanged since V6.0.6.7',
        'can_worker':'unchanged lines; receives two more IDs','unchanged_encoder_fix':True,'unchanged_hardware_te_qspi':True,'unchanged_co5300_alignment':True,
        'startup_look':'unchanged (frame-exact host checks)','ui':'Performance Torus functions (host: every frame equals a clean render)',
        'intentional_firmware_changes':sorted(FW_CHANGED),'intentional_pc_changes':sorted(PC_CHANGED),'new_pc_modules':sorted(PC_NEW),'removed_pc_files':sorted(PC_REMOVED),'font':'Michroma'}
    (ROOT/'validation/integrity_audit.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':run()
