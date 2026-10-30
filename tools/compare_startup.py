"""V6.0.6: the new approved startup ("infinity -> laser") rendered by the REAL C++ headers with the fast and the
reference AA backends must be pixel-identical on every frame. Host times are not ESP32 FPS."""
from pathlib import Path
import subprocess,sys,os,csv,json,statistics,argparse
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--node',required=True);args=parser.parse_args()
build=ROOT/'.build/startup_compare';build.mkdir(parents=True,exist_ok=True)
reports={}
for variant in ('fast','reference'):
    exe=build/f'v606_{variant}.wasm'
    command=[sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-Wno-nullability-completeness',
        '-I'+str(ROOT/'tests/host'),'-I'+str(ROOT/'OJM_DDE_Dynamics/src'),
        str(ROOT/'tests/host/startup_probe.cpp'),'-target','wasm32-wasi','-Wl,-z,stack-size=1048576','-o',str(exe)]
    if variant=='reference':command.append('-DDDE_REFERENCE_COVERAGE')
    compiled=subprocess.run(command,capture_output=True,text=True,env=dict(os.environ,ZIG_GLOBAL_CACHE_DIR=str(ROOT/'.build/host/zig-cache')))
    if compiled.returncode:
        print(compiled.stderr[-5000:]);raise SystemExit(compiled.returncode)
    run=subprocess.run([args.node,str(ROOT/'tools/run_renderer_wasi.cjs'),str(exe),str(build)],check=True,capture_output=True,text=True)
    (ROOT/f'validation/startup_v606_{variant}.csv').write_text(run.stdout)
    reports[variant]=list(csv.DictReader(run.stdout.splitlines()))
fast,reference=reports['fast'],reports['reference'];assert len(fast)==len(reference) and len(fast)>300
for a,b in zip(fast,reference):
    assert a['ms']==b['ms'] and a['hash']==b['hash'],f"AA backends differ at {a['ms']} ms"
total=int(fast[-1]['ms'])
def metrics(rows,begin,end):
    moving=[r for r in rows if begin<=int(r['ms'])<end and int(r['pixels'])>0]
    return dict(frames=len(moving),pixels=sum(int(r['pixels']) for r in moving),
        mean_pixels=round(statistics.mean(int(r['pixels']) for r in moving)) if moving else 0,
        max_pixels=max((int(r['pixels']) for r in moving),default=0))
report={'status':'PASS','startup':'V6.0.6 approved "Nekonecno -> laser" (replaces the V6.0.5.x signature by user approval)',
    'compared_frames':len(fast),'fast_vs_reference_identical':True,'last_frame_ms':total,
    'timing':'Host WASI with a simulated 16.6 ms TE; not physical ESP32 FPS','phases':{}}
for name,a,b in [('infinity',0,1100),('laser',1100,2760),('fill',2760,3700),('hold',3700,4600),('close',4600,5400),('titan',5400,total+1)]:
    report['phases'][name]=metrics(fast,a,b)
(ROOT/'validation/startup_comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
