"""Compile and execute the ACTUAL firmware renderer/model with host-only hardware stubs."""
from pathlib import Path
import argparse
import json
import os
import subprocess
import sys
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--node',help='Run the same C++ test as sandboxed WASI using this Node executable')
args=parser.parse_args()
build=ROOT/'.build/host';build.mkdir(parents=True,exist_ok=True)
frames=build/'frames';frames.mkdir(exist_ok=True)
exe=build/('render_test.wasm' if args.node else 'render_test.exe')
command=[sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-Wno-nullability-completeness','-I'+str(ROOT/'tests/host'),
    '-I'+str(ROOT/'OJM_DDE_Dynamics/src'),str(ROOT/'tests/host/render_test.cpp'),'-o',str(exe)]
if args.node:command.extend(['-target','wasm32-wasi','-Wl,-z,stack-size=1048576'])
environment=dict(os.environ, ZIG_GLOBAL_CACHE_DIR=str(build/'zig-cache'))
result=subprocess.run(command,capture_output=True,text=True,env=environment)
log=result.stdout+result.stderr
if result.returncode:
    (ROOT/'validation/renderer_test.txt').write_text(log);print(log[-6000:]);sys.exit(result.returncode)
run=[args.node,str(ROOT/'tools/run_renderer_wasi.cjs'),str(exe),str(frames)] if args.node else [str(exe),str(frames)]
result=subprocess.run(run,capture_output=True,text=True)
log+=result.stdout+result.stderr
(ROOT/'validation/renderer_test.txt').write_text(log);print(log[-6000:])
if result.returncode:sys.exit(result.returncode)
sequences={}
# One picture per simulated TE refresh (16.6 ms) for every sequence.
for name in ('startup','menu','aero','rws','torque','action','locks'):
    pictures=[Image.open(p).convert('RGB') for p in sorted(frames.glob(name+'_*.ppm'))]
    indices=[round(i*(len(pictures)-1)/5) for i in range(6)]
    sheet=Image.new('RGB',(466*3,492*2),'#10171f');draw=ImageDraw.Draw(sheet)
    for at,index in enumerate(indices):
        x,y=(at%3)*466,(at//3)*492;sheet.paste(pictures[index],(x,y));draw.text((x+12,y+470),f'{name} / frame {index}',fill='white')
    sheet.save(ROOT/'validation'/f'firmware_{name}.png')
    pictures[-1].save(ROOT/'validation'/f'frame_{name}.png')
    if name=='startup':
        for label,indices in (('startup_infinity_laser',(20,39,62,90,130,165)),('startup_fill_close',(175,190,205,280,300,318))):
            strip=Image.new('RGB',(386*3,170*2),'#10171f');sd=ImageDraw.Draw(strip)
            for at,index in enumerate(indices):
                x,y=(at%3)*386,(at//3)*170
                strip.paste(pictures[index].crop((40,140,426,310)),(x,y))
                sd.text((x+6,y+152),f'{round(index*16.6)} ms',fill='white')
            strip.save(ROOT/'validation'/f'{label}.png')
    # GIF timing has 10 ms resolution: alternate 20/10/20 ms to keep the 60 Hz duration.
    durations=[(round((i+1)*100/60)-round(i*100/60))*10 for i in range(len(pictures))]
    pictures[0].copy().save(ROOT/'validation'/f'firmware_{name}.gif',save_all=True,
        append_images=[p.copy() for p in pictures[1:]],duration=durations,loop=0)
    sequences[name]=len(pictures)
(ROOT/'validation/renderer_test.json').write_text(json.dumps({'status':'PASS','source':'actual C++ renderer/model headers (V6.0.6 Halo 2)',
    'execution':'WebAssembly/WASI' if args.node else 'native host',
    'checks':'every pushed frame equals a clean full render; CO5300 even windows; TE-locked pacing with a simulated 16.6 ms TE',
    'hardware':'stubbed QSPI/TE/CAN and clock; not hardware FPS validation','sequences':sequences},indent=2)+'\n')
