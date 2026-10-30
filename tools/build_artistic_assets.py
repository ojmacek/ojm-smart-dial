"""Compile native stroke geometry and the top view once, never on the dial.
The supplied side image is a visual reference; these editable Bezier paths define
the stroke reveal and deliberately omit the movable rear-deck segment.
"""
from pathlib import Path
import json, math
from PIL import ImageFont, Image, ImageFilter
from convert_assets import rle, spans

ROOT=Path(__file__).resolve().parents[1]
# Coordinates in the 466 px dial. Roof starts at its crown and splits both ways.
CURVES=[
 [(233,199),(194,198),(175,208),(149,222),(115,224),(71,239),(55,252),(52,255),(54,263),(54,269)],
 [(233,199),(294,198),(329,213),(368,226),(378,230),(385,232),(392,232)],
 [(155,226),(178,211),(202,206),(232,206),(263,206),(292,209),(315,217),(329,222),(317,227),(296,228),(254,230),(201,230),(166,231)],
 [(300,237),(333,226),(358,229),(386,233)],
 [(410,231),(407,237),(413,241),(415,252),(415,257),(414,262),(414,265)],
 [(389,240),(397,236),(403,233),(410,231)],
]
def sample(control):
    points=[]
    for i in range(0,len(control)-1,3):
        a,b,c,d=control[i:i+4]
        for n in range(20):
            t=n/20;u=1-t
            points.append([round(u**3*a[k]+3*u*u*t*b[k]+3*u*t*t*c[k]+t**3*d[k],2) for k in (0,1)])
    points.append(list(control[-1]));return points

def run():
    paths=[sample(c) for c in CURVES]
    (ROOT/'assets/preview/titan_paths.json').write_text(json.dumps(paths))
    # Always use floating point literals, including exact integer coordinates.
    out=['#pragma once','struct ArtPoint {float x,y;};','struct ArtPath {const ArtPoint *points; uint16_t count;};']
    for i,p in enumerate(paths):out.append(f'static const ArtPoint ART_{i}[] = {{'+','.join('{'+f'{float(x):.2f}f,{float(y):.2f}f'+'}' for x,y in p)+'};')
    out.append('static const ArtPath ART_PATHS[] = {'+','.join('{'+f'ART_{i},{len(p)}'+'}' for i,p in enumerate(paths))+'};')
    (ROOT/'OJM_DDE_Dynamics/src/TitanStrokes.h').write_text('\n'.join(out)+'\n')
    # Offline resampling avoids forward-map aliasing and per-frame scaling.
    source=Image.open(ROOT/'assets/source/titan_top_minimal.png').convert('L')
    box=source.point(lambda p:255 if p>96 else 0).getbbox()
    # Preserve a readable ~1 px stroke after the 5:1 reduction on the small panel.
    top=source.crop(box).filter(ImageFilter.MaxFilter(5)).resize((100,198),Image.Resampling.LANCZOS)
    top=top.point(lambda p:min(255,round(p*1.2)))
    top.save(ROOT/'assets/preview/titan_top_minimal.png')
    data=spans(top)
    header='#pragma once\n#include "AssetTypes.h"\nstatic const AssetSpan ART_TOP_SPANS[] PROGMEM = {'+','.join('{'+','.join(map(str,r))+'}' for r in data)+'};\n'
    header+=f'static const OutlineAsset ART_TOP = {{100,198,{len(data)},ART_TOP_SPANS}};\n'
    (ROOT/'OJM_DDE_Dynamics/src/ArtTop.h').write_text(header)
    print('Compiled Titan paths and the top view (V6.0.6: ArtFont/ArtLogo retired).')
if __name__=='__main__':run()
