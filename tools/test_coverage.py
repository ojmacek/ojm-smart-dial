from pathlib import Path
import subprocess,sys,os,argparse
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--node',required=True);args=p.parse_args()
build=ROOT/'.build/host';build.mkdir(parents=True,exist_ok=True);exe=build/'coverage_test.wasm'
command=[sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-Wno-nullability-completeness',
 '-I'+str(ROOT/'tests/host'),'-I'+str(ROOT/'OJM_DDE_Dynamics/src'),str(ROOT/'tests/host/coverage_test.cpp'),
 '-target','wasm32-wasi','-Wl,-z,stack-size=1048576','-o',str(exe)]
r=subprocess.run(command,capture_output=True,text=True,env=dict(os.environ,ZIG_GLOBAL_CACHE_DIR=str(build/'zig-cache')))
if r.returncode:print(r.stderr[-4000:]);raise SystemExit(r.returncode)
r=subprocess.run([args.node,str(ROOT/'tools/run_renderer_wasi.cjs'),str(exe),str(build)],capture_output=True,text=True)
(ROOT/'validation/coverage_test.txt').write_text(r.stdout+r.stderr);print(r.stdout+r.stderr);raise SystemExit(r.returncode)
