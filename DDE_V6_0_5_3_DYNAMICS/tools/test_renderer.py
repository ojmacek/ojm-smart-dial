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
for name in ('startup','menu','rws','torque','aero','standalone_rws','standalone_vectoring','standalone_aero'):
    pictures=[Image.open(p).convert('RGB') for p in sorted(frames.glob(name+'_*.ppm'))]
    indices=[round(i*(len(pictures)-1)/5) for i in range(6)]
    sheet=Image.new('RGB',(466*3,492*2),'#10171f');draw=ImageDraw.Draw(sheet)
    for at,index in enumerate(indices):
        x,y=(at%3)*466,(at//3)*492;sheet.paste(pictures[index],(x,y));draw.text((x+12,y+470),f'{name} / frame {index}',fill='white')
    sheet.save(ROOT/'validation'/f'firmware_{name}.png')
    pictures[-1].save(ROOT/'validation'/f'frame_{name}.png')
    if name=='startup':
        for label,indices in (('logo_open',(28,32,36,40,44,47)),('logo_close',(106,109,112,116,119,122))):
            strip=Image.new('RGB',(224*3,122*2),'#10171f');sd=ImageDraw.Draw(strip)
            for at,index in enumerate(indices):
                x,y=(at%3)*224,(at//3)*122
                strip.paste(pictures[index].crop((121,187,345,279)),(x,y))
                sd.text((x+6,y+98),f'{round((index+1)*1000/60)} ms',fill='white')
            strip.save(ROOT/'validation'/f'{label}.png')
    # GIF timing has 10 ms resolution. Alternate 20/10/20 ms to preserve the
    # simulated 60 Hz duration instead of rounding every 16.667 ms frame to 10.
    durations=[(round((i+1)*5/3)-round(i*5/3))*10 for i in range(len(pictures))]
    pictures[0].copy().save(ROOT/'validation'/f'firmware_{name}.gif',save_all=True,
        append_images=[p.copy() for p in pictures[1:]],duration=durations,loop=0)
    sequences[name]=len(pictures)
(ROOT/'validation/renderer_test.json').write_text(json.dumps({'status':'PASS','source':'actual C++ renderer/model headers',
    'execution':'WebAssembly/WASI' if args.node else 'native host',
    'hardware':'stubbed QSPI/TE/CAN and clock; not hardware FPS validation','sequences':sequences},indent=2)+'\n')
