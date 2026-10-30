"""Export the firmware's exact logo ink (SignatureAssets.h) as assets/preview/halo_logo.png for the PC mirror,
so the PC startup fills exactly the same pixels as the dial (280 x 114, placed at 93,176)."""
from pathlib import Path
import re
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]

def run():
    text=(ROOT/'OJM_DDE_Dynamics/src/SignatureAssets.h').read_text(encoding='utf-8')
    w,h=(int(v) for v in re.search(r'SIGNATURE_W=(\d+),SIGNATURE_H=(\d+)',text).groups())
    body=re.search(r'SignatureInkPixel SIGNATURE_INK\[\] PROGMEM = \{(.*?)\};',text,re.S).group(1)
    image=Image.new('L',(w,h),0);pixels=image.load();count=0
    for offset,_arrival,alpha in re.findall(r'\{(\d+),(\d+),(\d+)\}',body):
        offset=int(offset);pixels[offset%w,offset//w]=int(alpha);count+=1
    target=ROOT/'assets/preview/halo_logo.png';image.save(target)
    print(f'{target.name}: {w}x{h}, {count} ink pixels')

if __name__=='__main__':
    run()
