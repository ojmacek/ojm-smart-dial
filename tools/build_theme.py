"""Offline Michroma subsets. The dial only reads RLE glyphs, never a TTF."""
from pathlib import Path
import sys, json
from PIL import ImageFont
from convert_assets import rle

ROOT=Path(__file__).resolve().parents[1]

def run():
    folder=ROOT/'assets/fonts'
    output=['#pragma once','#include "SmartDial_Fonts_DINish.h"']
    report={}
    # Halo 2 (V6.0.6): title 20 px, mode name 40 px.
    for symbol,size in (('WEB_LABEL',20),('WEB_MODE',40)):
        font=ImageFont.truetype(str(folder/'Michroma-Regular.ttf'),size)
        data=[];glyphs=[]
        for code in range(32,127):
            ch=chr(code);mask,offset=font.getmask2(ch,mode='L',anchor='ls');w,h=mask.size
            encoded=rle([min(15,(v+8)//17) for v in mask])
            glyphs.append((len(data),len(encoded),w,h,round(font.getlength(ch)),*offset,code));data.extend(encoded)
        output.append(f'static const uint8_t {symbol}_DATA[] PROGMEM = {{'+','.join(map(str,data))+'};')
        output.append(f'static const SDGlyph {symbol}_GLYPHS[] PROGMEM = {{'+','.join('{'+','.join(map(str,g))+'}' for g in glyphs)+'};')
        output.append(f'static const SDFont {symbol} = {{{symbol}_DATA,{symbol}_GLYPHS,95,0,0}};')
        report[symbol]={'pixel_size':size,'glyphs':95,'rle_bytes':len(data)}
    (ROOT/'OJM_DDE_Dynamics/src/WebFonts.h').write_text('\n'.join(output)+'\n',encoding='utf-8')
    (ROOT/'assets/preview/web_fonts.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(report)

if __name__=='__main__':run()
