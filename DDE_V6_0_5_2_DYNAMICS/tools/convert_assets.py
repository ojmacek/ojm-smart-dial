"""Offline conversion of supplied Titan PNGs to sparse 4-bit coverage spans.
No image decoding, scaling or allocation is needed on the dial at runtime.
Also derives small DINish subsets from the licensed baseline bitmap font.
"""
from pathlib import Path
import argparse
import json
import math
import re
import io
from PIL import Image, ImageDraw

ROOT=Path(__file__).resolve().parents[1]
FOLD_STEPS=32

def logo(source,sketch,preview):
    import resvg_py
    # This SVG is a black compound path with transparent letter-shaped holes.
    # White compositing reproduces the supplied white-on-black logo faithfully.
    png=resvg_py.svg_to_bytes(svg_path=str((source/'ojm_logo.svg').resolve()),
        background='#ffffff',width=1344,height=1344,dpi=96,skip_system_fonts=True)
    mask=Image.open(io.BytesIO(png)).convert('L')
    box=mask.point(lambda p:255 if p>48 else 0).getbbox()
    mask=mask.crop(box); mask=mask.resize((256,round(mask.height*256/mask.width)),Image.Resampling.LANCZOS)
    runs=spans(mask)
    assert mask.width*mask.height<65536
    out=['#pragma once','#include "TitanAssets.h"',
        'static const AssetSpan OJM_LOGO_SPANS[] PROGMEM = {'+','.join('{'+','.join(map(str,r))+'}' for r in runs)+'};',
        f'static const OutlineAsset OJM_LOGO = {{{mask.width},{mask.height},{len(runs)},OJM_LOGO_SPANS}};']
    (sketch/'OjmLogo.h').write_text('\n'.join(out)+'\n')
    mask.save(preview/'ojm_logo.png')
    return dict(source='ojm_logo.svg',source_crop=box,size=mask.size,span_bytes=len(runs)*4,
        original_bitmap_bytes=204800,conversion='resvg-py 0.5.0, white compositing, 4-bit coverage')

def crop_outline(path):
    image=Image.open(path).convert('L')
    box=image.point(lambda p:255 if p>48 else 0).getbbox()
    if box is None: raise ValueError(f'No outline: {path}')
    return image.crop(box),box

def spans(image):
    pixels=list(image.getdata()); out=[]; w,h=image.size
    for y in range(h):
        x=0
        while x<w:
            alpha=(pixels[y*w+x]+8)//17; alpha=min(15,alpha)
            if not alpha: x+=1; continue
            end=x+1
            while end<w and end-x<255 and min(15,(pixels[y*w+end]+8)//17)==alpha: end+=1
            out.append((y*w+x,end-x,alpha*17)); x=end
    return out

def rle(values):
    result=[]; at=0
    while at<len(values):
        end=at+1
        while end<len(values) and end-at<16 and values[end]==values[at]: end+=1
        result.append(((end-at-1)<<4)|values[at]); at=end
    return result

def fonts(sketch):
    source=(sketch/'SmartDial_Fonts_DINish.h').read_text()
    raw=re.search(r'SD_FONT_LABEL_DATA\[\].*?\{(.*?)\};',source,re.S).group(1)
    data=[int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]+)',raw)]
    raw=re.search(r'SD_FONT_LABEL_GLYPHS\[\].*?\{(.*?)\};',source,re.S).group(1)
    original=[]
    for row in re.findall(r'\{([^}]+)\}',raw):
        off,length,w,h,advance,x,y,code=map(int,row.split(','))
        pixels=[]
        for b in data[off:off+length]: pixels.extend([(b&15)*17]*((b>>4)+1))
        image=Image.new('L',(max(1,w),max(1,h)))
        if pixels: image.putdata(pixels[:w*h])
        original.append((image,w,h,advance,x,y,code))
    # Minimal missing punctuation, drawn geometrically under this script's license.
    for code in (43,45):
        image=Image.new('L',(12,12)); draw=ImageDraw.Draw(image); draw.rectangle((1,5,10,6),fill=255)
        if code==43: draw.rectangle((5,1,6,10),fill=255)
        original.append((image,12,12,13,0,-15,code))
    output=['#pragma once','#include "SmartDial_Fonts_DINish.h"']
    for name,scale in (('SMALL',.6),('TEXT',.8),('MODE',1.25)):
        blob=[]; glyphs=[]
        for image,w,h,advance,x,y,code in sorted(original,key=lambda g:g[-1]):
            nw,nh=max(1,round(w*scale)),round(h*scale); encoded=[]
            if h:
                resized=image.resize((nw,max(1,nh)),Image.Resampling.LANCZOS)
                encoded=rle([min(15,(p+8)//17) for p in resized.getdata()])
            glyphs.append((len(blob),len(encoded),nw,nh,round(advance*scale),round(x*scale),round(y*scale),code))
            blob.extend(encoded)
        prefix='DDE_FONT_'+name
        output.append(f'static const uint8_t {prefix}_DATA[] PROGMEM = {{'+','.join(map(str,blob))+'};')
        output.append(f'static const SDGlyph {prefix}_GLYPHS[] PROGMEM = {{'+','.join('{'+','.join(map(str,g))+'}' for g in glyphs)+'};')
        output.append(f'static const SDFont {prefix} = {{{prefix}_DATA,{prefix}_GLYPHS,{len(glyphs)},0,0}};')
    (sketch/'DdeFonts.h').write_text('\n'.join(output)+'\n')

def convert(source,sketch,preview):
    preview.mkdir(parents=True,exist_ok=True)
    out=['#pragma once','#include <Arduino.h>',
         'struct AssetSpan { uint16_t offset; uint8_t length; uint8_t alpha; };',
         'struct OutlineAsset { uint16_t width,height,count; const AssetSpan *spans; };']
    report={'logo':logo(source,sketch,preview),'fold_steps':FOLD_STEPS}
    for name,size in (('top',(138,264)),('side',(350,94)),('front',(210,126)),('rear',(210,119))):
        cropped,box=crop_outline(source/f'titan_{name}.png')
        base=cropped.resize(size,Image.Resampling.LANCZOS)
        base.save(preview/f'titan_{name}.png')
        report[name]={'source_crop':box,'size':size}
        variants=range(FOLD_STEPS+1) if name in ('top','side') else range(1)
        for step in variants:
            h=max(2,round(size[1]*(1-step/FOLD_STEPS)))
            image=base if step==0 else base.resize((size[0],h),Image.Resampling.LANCZOS)
            runs=spans(image); symbol=f'TITAN_{name.upper()}_{step}'
            out.append(f'static const AssetSpan {symbol}_SPANS[] PROGMEM = {{'+','.join('{'+','.join(map(str,r))+'}' for r in runs)+'};')
            out.append(f'static const OutlineAsset {symbol} = {{{image.width},{image.height},{len(runs)},{symbol}_SPANS}};')
        if name in ('top','side'):
            out.append(f'static const OutlineAsset *const TITAN_{name.upper()}_FOLDS[{FOLD_STEPS+1}] = {{'+','.join(f'&TITAN_{name.upper()}_{s}' for s in range(FOLD_STEPS+1))+'};')
    out.append(f'static constexpr uint8_t TITAN_FOLD_STEPS={FOLD_STEPS};')
    out.append('static const int16_t DDE_SIN[256] = {'+','.join(str(round(math.sin(i*2*math.pi/256)*32767)) for i in range(256))+'};')
    (sketch/'TitanAssets.h').write_text('\n'.join(out)+'\n')
    (preview/'conversion.json').write_text(json.dumps(report,indent=2)+'\n')
    fonts(sketch)
    print('Generated TitanAssets.h, DdeFonts.h, OjmLogo.h and five preview PNGs.')

if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--source',type=Path,default=ROOT/'assets/source')
    parser.add_argument('--sketch',type=Path,default=ROOT/'OJM_DDE_Dynamics/src')
    parser.add_argument('--preview',type=Path,default=ROOT/'assets/preview')
    args=parser.parse_args(); convert(args.source,args.sketch,args.preview)
