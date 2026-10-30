from pathlib import Path
import argparse,subprocess,sys,os
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--node',required=True);args=p.parse_args()
build=ROOT/'.build/host';build.mkdir(parents=True,exist_ok=True);exe=build/'startup_gaps.wasm'
compile=subprocess.run([sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-Wno-nullability-completeness',
 '-I'+str(ROOT/'tests/host'),'-I'+str(ROOT/'OJM_DDE_Dynamics/src'),str(ROOT/'tests/host/startup_gaps.cpp'),
 '-target','wasm32-wasi','-Wl,-z,stack-size=1048576','-o',str(exe)],capture_output=True,text=True,
 env=dict(os.environ,ZIG_GLOBAL_CACHE_DIR=str(build/'zig-cache')))
if compile.returncode:print(compile.stderr[-4000:]);raise SystemExit(compile.returncode)
run=subprocess.run([args.node,str(ROOT/'tools/run_renderer_wasi.cjs'),str(exe),str(build)],capture_output=True,text=True)
(ROOT/'validation/startup_gaps.txt').write_text(run.stdout+run.stderr);print(run.stdout+run.stderr);raise SystemExit(run.returncode)
