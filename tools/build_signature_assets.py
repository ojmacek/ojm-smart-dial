"""Compile the approved infinite signature into native geometry and ink arrival.

No PNG decoder, path search, allocation or brush-mask intersection runs on ESP32.
The O/J/M strokes and connectors are the approved browser choreography.
"""
from pathlib import Path
import math, json, hashlib
from bisect import bisect_left
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
SCALE=280/640
LEFT=93.0
TOP=233-280*259/640/2
WIDTH,HEIGHT,Y=280,114,176

def soft(p):
    p=max(0,min(1,p))
    return p*p*p*(10+p*(-15+6*p))

def phase(p,a,b):return max(0,min(1,(p-a)/(b-a)))

def cubic(a,b,c,d,count=72):
    out=[]
    for i in range(count):
        t=i/(count-1);q=1-t
        out.append([q*q*q*a[k]+3*q*q*t*b[k]+3*q*t*t*c[k]+t*t*t*d[k] for k in (0,1)])
    return out

def metric(points):
    lengths=[0.0]
    for a,b in zip(points,points[1:]):lengths.append(lengths[-1]+math.dist(a,b))
    return lengths

def sample(points,count):
    lengths=metric(points);out=[];index=1
    for i in range(count):
        at=lengths[-1]*i/(count-1)
        while index<len(points)-1 and lengths[index]<at:index+=1
        a,b=points[index-1:index+1];q=(at-lengths[index-1])/(lengths[index]-lengths[index-1] or 1)
        out.append([a[k]+(b[k]-a[k])*q for k in (0,1)])
    return out

def strokes():
    # These are the centre lines used in the approved preview, in source pixels.
    # Copy their source data into this release so the generator is standalone.
    source=ROOT/'assets/source/signature_ink_paths.json'
    if not source.exists():
        approved=ROOT.parent/'design/startup-2026-10-14-cinematic/preview-assets.json'
        source.write_text(json.dumps(json.loads(approved.read_text(encoding='utf-8-sig'))['inkPaths']),encoding='utf-8')
    return [[[LEFT+x*SCALE,TOP+y*SCALE] for x,y in points] for points in json.loads(source.read_text())]

def arrival(pixel,points,radius):
    lengths=metric(points);best=math.inf
    for i,(a,b) in enumerate(zip(points,points[1:])):
        dx,dy=b[0]-a[0],b[1]-a[1];length=math.hypot(dx,dy)
        if length<1e-8:continue
        ux,uy=dx/length,dy/length;px,py=pixel[0]-a[0],pixel[1]-a[1]
        along=px*ux+py*uy;normal=px*uy-py*ux
        if abs(normal)>radius:continue
        reach=math.sqrt(max(0,radius*radius-normal*normal))
        enter,leave=along-reach,along+reach
        if leave<0 or enter>length:continue
        best=min(best,lengths[i]+max(0,enter))
    return best

def run():
    pen=strokes();loop=[]
    for i in range(460):
        f=i/459;a=math.tau*1.5*f;q=soft(phase(f,.54,1));r=156+(88-156)*q
        loop.append([233+r*math.sin(a),224+55*math.sin(2*a)+8*math.sin(a)*(1-q)])
    o,j,m=pen;segments=[(loop,None),
        (cubic(loop[-1],[loop[-1][0]-42,loop[-1][1]+42],[o[0][0]-38,o[0][1]],o[0]),None),
        (o,0),(cubic(o[-1],[o[-1][0]+48,o[-1][1]],[j[0][0],j[0][1]-42],j[0]),None),
        (j,1),(cubic(j[-1],[j[-1][0],j[-1][1]-42],[m[0][0],m[0][1]+55],m[0]),None),(m,2)]
    track=[];distance=0;ink=[]
    for points,letter in segments:
        if track:assert math.dist(track[-1],points[0])<.05
        begin=distance;distance+=metric(points)[-1]
        track.extend(points if not track else points[1:])
        if letter is not None:ink.append((begin,distance))
    source=ROOT/'assets/source/approved_logo_mask.png'
    alpha=Image.open(source).convert('RGBA').getchannel('A')
    alpha=alpha.transform((WIDTH,HEIGHT),Image.Transform.AFFINE,
        (1/SCALE,0,0,0,1/SCALE,(Y-TOP)/SCALE),resample=Image.Resampling.BICUBIC)
    rgba=Image.new('RGBA',alpha.size,'white');rgba.putalpha(alpha)
    rgba.save(ROOT/'assets/preview/infinity_logo.png')
    pixels=[];counts=[0,0,0,0]
    for y in range(HEIGHT):
        for x in range(WIDTH):
            a=alpha.getpixel((x,y))
            if not a:continue
            world=(LEFT+x+.5,Y+y+.5)
            if world[1]>=TOP+180*SCALE:
                when=65535;counts[3]+=1
            else:
                letter=0 if x<211*SCALE else 1 if x<385.5*SCALE else 2
                entry=arrival(world,pen[letter],20 if letter==2 else 13)
                assert math.isfinite(entry),f'Uncovered letter {letter}: {x},{y}'
                when=round((ink[letter][0]+entry)/distance*65535);counts[letter]+=1
            pixels.append((y*WIDTH+x,when,a))
    feather=max(1,round(1.5/distance*65535))
    assert max(p[1] for p in pixels if p[1]!=65535)+feather<65535
    uniform=sample(track,1025);trace=sample(loop,513)
    assert all(50+4<x<418-4 and 130+4<y<304-4 for x,y in track)
    def coords(name,points):
        return 'static const SignaturePoint '+name+'[] PROGMEM = {\n'+''.join(
            '{%d,%d},'% (round(x*16),round(y*16)) + ('\n' if i%12==11 else '') for i,(x,y) in enumerate(points))+'\n};\n'
    header=['#pragma once\n// Generated from the approved O -> J -> M infinite signature.\n',
        'struct SignaturePoint {int16_t x,y;}; // native pixels in Q4\n',
        'struct SignatureInkPixel {uint16_t offset,arrival;uint8_t alpha;};\n',
        f'static constexpr uint16_t SIGNATURE_W={WIDTH},SIGNATURE_H={HEIGHT},SIGNATURE_X=93,SIGNATURE_Y={Y};\n',
        f'static constexpr uint16_t SIGNATURE_FEATHER={feather};\n',
        f'static constexpr float SIGNATURE_TRACE_FRACTION={metric(loop)[-1]/distance:.9f}f;\n',
        'static constexpr uint16_t SIGNATURE_INK_BEGIN[]={'+','.join(str(round(a/distance*65535)) for a,b in ink)+'};\n',
        coords('SIGNATURE_TRACK',uniform),coords('SIGNATURE_TRACE',trace),
        'static const uint16_t SIGNATURE_ROW_START[] PROGMEM = {'+','.join(
            str(bisect_left([p[0] for p in pixels],row*WIDTH)) for row in range(HEIGHT+1))+'};\n',
        'static const uint16_t SIGNATURE_REVEAL_INDEX[] PROGMEM = {'+','.join(
            str(i) for i in sorted((i for i,p in enumerate(pixels) if p[1]!=65535),key=lambda i:pixels[i][1]))+'};\n',
        'static const SignatureInkPixel SIGNATURE_INK[] PROGMEM = {\n']
    header.extend('{%d,%d,%d},'%p+('\n' if i%10==9 else '') for i,p in enumerate(pixels))
    header.append('\n};\n')
    (ROOT/'OJM_DDE_Dynamics/src/SignatureAssets.h').write_text(''.join(header),encoding='utf-8')
    report={'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'logo_size':[WIDTH,HEIGHT],
        'position':[93,Y],'order':['O','J','M'],'brush_diameters':[26,26,40],
        'track_pixels':distance,'track_samples':len(uniform),'trace_samples':len(trace),
        'ink_pixels':len(pixels),'letter_pixels':counts,'feather_units':feather,
        'native_bytes':len(pixels)*6+(len(uniform)+len(trace))*4+(HEIGHT+1)*2+sum(1 for p in pixels if p[1]!=65535)*2,
        'all_original_ink_covered':True,'runtime_png_svg_decode':False}
    (ROOT/'assets/preview/signature_geometry.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':run()
