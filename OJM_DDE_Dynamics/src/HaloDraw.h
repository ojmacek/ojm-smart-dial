#pragma once
// Halo 2 drawing primitives. Every primitive is clipped to art_clip, so a scene can be repainted
// rectangle by rectangle after its rows were cleared, without touching pixels outside the damage.
static constexpr float HALO_DEG=PI/180.f;
static uint16_t mix565(uint16_t a,uint16_t b,float k){return blend_rgb565(a,b,(uint8_t)lroundf(clamp_float(k,0,1)*255));}
static uint8_t alpha8(float a){return (uint8_t)lroundf(clamp_float(a,0,1)*255);}

// ---- damage tracking: every animated item reports its box and a signature of its look ----
struct HaloSig {
 uint32_t h=2166136261u;
 void add(int32_t v){for(int i=0;i<4;++i){h^=(uint8_t)(v>>(8*i));h*=16777619u;}}
 // Exact bits: any change of an input repaints its item, so the panel always equals a clean render.
 void addf(float v){int32_t bits;memcpy(&bits,&v,4);add(bits);}
};
struct HaloBox {
 float x0=1e9f,y0=1e9f,x1=-1e9f,y1=-1e9f;
 void add(float x,float y,float pad=0){x0=min(x0,x-pad);y0=min(y0,y-pad);x1=max(x1,x+pad);y1=max(y1,y+pad);}
 void add(const HaloBox &b){if(b.empty())return;x0=min(x0,b.x0);y0=min(y0,b.y0);x1=max(x1,b.x1);y1=max(y1,b.y1);}
 bool empty()const{return x1<x0 || y1<y0;}
};
struct DamageSlot {bool on=false;int16_t x=0,y=0,w=0,h=0;uint32_t sig=0;};
// Adopt: the panel already shows exactly this (Titan -> UI): record every item's state without repainting it.
static bool damage_adopt=false;
static void damage_item(DamageSlot &slot,bool on,const HaloBox &box,uint32_t sig){
 int16_t x=0,y=0,w=0,h=0;
 if(on && !box.empty()){x=(int16_t)floorf(box.x0)-1;y=(int16_t)floorf(box.y0)-1;w=(int16_t)ceilf(box.x1)+2-x;h=(int16_t)ceilf(box.y1)+2-y;}
 else on=false;
 if(damage_adopt){slot.on=on;slot.x=x;slot.y=y;slot.w=w;slot.h=h;slot.sig=sig;return;}
 if(slot.on==on && (!on || (slot.sig==sig && slot.x==x && slot.y==y && slot.w==w && slot.h==h)))return;
 if(slot.on)dirty(slot.x,slot.y,slot.w,slot.h);
 if(on)dirty(x,y,w,h);
 slot.on=on;slot.x=x;slot.y=y;slot.w=w;slot.h=h;slot.sig=sig;
}
// A full ring is damaged as horizontal bands: per band only the left and the right part of the annulus.
// The boxes of one ring never overlap each other (they only touch), so they are never merged into one screen.
// Ring band of rows [y0,y1): the left and right part of the annulus (one box when they meet).
static void damage_ring_band(float cx,float cy,float r0,float r1,int y0,int y1){
 const float near=(y0<=cy && y1>=cy)?0:min(fabsf(y0-cy),fabsf(y1-cy)),far=max(fabsf(y0-cy),fabsf(y1-cy));
 if(near>r1)return;
 const float outer=sqrtf(max(0.f,r1*r1-near*near)),inner=far<r0?sqrtf(r0*r0-far*far):0;
 const int lx0=((int)floorf(cx-outer)-1)&~1,lx1=((int)ceilf(cx-inner)+2)&~1,rx0=((int)floorf(cx+inner)-1)&~1,rx1=((int)ceilf(cx+outer)+2)&~1;
 if(lx1>=rx0)dirty(lx0,y0,rx1-lx0,y1-y0);
 else{dirty(lx0,y0,lx1-lx0,y1-y0);dirty(rx0,y0,rx1-rx0,y1-y0);}
}
// The upper half of a ring centred on the screen in bands with given row boundaries (optimised offline for the
// least area: thin bands where the ring runs flat), the lower half mirrored.
static void damage_ring_bounds(float cx,float cy,float r0,float r1,const int16_t *bounds,int count){
 for(int i=0;i+1<count;++i)damage_ring_band(cx,cy,r0,r1,bounds[i],bounds[i+1]);
 const int mid=bounds[count-1];
 for(int i=count-1;i>0;--i){const int a=i==count-1?mid:2*(int)cy-bounds[i],b=2*(int)cy-bounds[i-1];if(b>a)damage_ring_band(cx,cy,r0,r1,a,b);}
}
static constexpr int16_t RING_PULSE_BOUNDS[7]={-4,20,38,64,100,148,234},RING_HAIR_BOUNDS[7]={2,22,44,72,110,158,234};
static void damage_ring(float cx,float cy,float r0,float r1,int bands=8){
 const int top=((int)floorf(cy-r1)-1)&~1,bottom=((int)ceilf(cy+r1)+2)&~1,height=(((bottom-top)/bands)+1)&~1;
 for(int y0=top;y0<bottom;y0+=height){
  const int y1=min(bottom,y0+height);
  // Rows of this band nearest to and farthest from the centre give the x extents of the annulus.
  const float near=(y0<=cy && y1>=cy)?0:min(fabsf(y0-cy),fabsf(y1-cy)),far=max(fabsf(y0-cy),fabsf(y1-cy));
  if(near>r1)continue;
  const float outer=sqrtf(max(0.f,r1*r1-near*near)),inner=far<r0?sqrtf(r0*r0-far*far):0;
  const int lx0=((int)floorf(cx-outer)-1)&~1,lx1=((int)ceilf(cx-inner)+2)&~1,rx0=((int)floorf(cx+inner)-1)&~1,rx1=((int)ceilf(cx+outer)+2)&~1;
  if(lx1>=rx0)dirty(lx0,y0,rx1-lx0,y1-y0);
  else{dirty(lx0,y0,lx1-lx0,y1-y0);dirty(rx0,y0,rx1-rx0,y1-y0);}
 }
}
// A thin arc (angles in degrees) as chunks of 15 degrees.
static void damage_arc(float cx,float cy,float r,float a0,float a1,float pad){
 for(float a=a0;a<a1;a+=15){const float b=min(a+15,a1);HaloBox box;
  for(int s=0;s<=2;++s){const float t=(a+(b-a)*s/2)*HALO_DEG;box.add(cx+r*cosf(t),cy+r*sinf(t),pad);}
  dirty((int16_t)floorf(box.x0),(int16_t)floorf(box.y0),(int16_t)(ceilf(box.x1)-floorf(box.x0))+1,(int16_t)(ceilf(box.y1)-floorf(box.y0))+1);}
}
static bool clip_box(int &x0,int &y0,int &x1,int &y1){
 x0=max(x0,(int)art_clip.x);y0=max(y0,(int)art_clip.y);
 x1=min(x1,(int)(art_clip.x+art_clip.w-1));y1=min(y1,(int)(art_clip.y+art_clip.h-1));
 return x0<=x1 && y0<=y1;
}

// ---- square roots of the ring band (V6.0.6.5) ----
// Every arc of the UI is centred on the pixel grid (233,233): the squared distance of a pixel centre is then an
// integer + 0.5, so sqrt is a table lookup (r 204..238, 1/128 px steps: below one 8-bit coverage step).
static constexpr int ARC_LUT_R0=204,ARC_LUT_R1=238,ARC_LUT_K0=ARC_LUT_R0*ARC_LUT_R0,ARC_LUT_K1=ARC_LUT_R1*ARC_LUT_R1;
static uint16_t arc_sqrt_lut[ARC_LUT_K1-ARC_LUT_K0+1];static bool arc_lut_ready=false;
static void arc_lut_prepare(){
 if(arc_lut_ready)return;
 for(int k=ARC_LUT_K0;k<=ARC_LUT_K1;++k)arc_sqrt_lut[k-ARC_LUT_K0]=(uint16_t)lroundf(sqrtf(k+.5f)*128);
 arc_lut_ready=true;
}
// cos/sin of arc ends, remembered: every damaged rectangle repaints the same arcs (marks, segments, bar).
static void halo_cos_sin(float deg,float &c,float &s){
 struct Entry {float deg,c,s;};static Entry caches[2][24];static uint8_t nexts[2]={0,0};
 auto &cache=caches[halo_core()];uint8_t &next=nexts[halo_core()];  // one cache per core
 for(const auto &e:cache)if(e.deg==deg){c=e.c;s=e.s;return;}
 c=cosf(deg*HALO_DEG);s=sinf(deg*HALO_DEG);cache[next]={deg,c,s};next=(next+1)%24;
}
// ---- arc stroke with round caps (angles in degrees, 0 = right, 90 = down, like canvas) ----
static void HALO_HOT art_arc(uint16_t *dst,float cx,float cy,float r,float a0,float a1,float half,uint16_t color,uint8_t alpha){
 if(!alpha || a1-a0<.05f)return;
 const bool full=a1-a0>=359.9f;
 const float outer=half+.5f,band=half+1.f,rin=max(0.f,r-band),rout=r+band,rin2=rin*rin,rout2=rout*rout;
 // Exact box: both ends plus every axis extreme the arc passes (one cos/sin pair per end, no sampling).
 float u0x,u0y,u1x,u1y;halo_cos_sin(a0,u0x,u0y);halo_cos_sin(a1,u1x,u1y);
 HaloBox box;
 if(full)box.add(cx,cy,rout);
 else{
  box.add(cx+r*u0x,cy+r*u0y,band);box.add(cx+r*u1x,cy+r*u1y,band);
  static constexpr float AX[4][2]={{1,0},{0,1},{-1,0},{0,-1}};
  for(int k=0;k<4;++k){float ax=k*90.f;while(ax<a0)ax+=360;if(ax<=a1)box.add(cx+r*AX[k][0],cy+r*AX[k][1],band);}
 }
 int x0=(int)floorf(box.x0),y0=(int)floorf(box.y0),x1=(int)ceilf(box.x1),y1=(int)ceilf(box.y1);
 if(!clip_box(x0,y0,x1,y1))return;
 const float e0x=cx+r*u0x,e0y=cy+r*u0y,e1x=cx+r*u1x,e1y=cy+r*u1y;
 const bool wide=a1-a0>180;  // more than half a turn: inside = not in the (narrow) complement
 const bool core=half>.5f;const float core_in=max(0.f,r-(half-.5f)),core_out=r+(half-.5f),core2_in=core_in*core_in,core2_out=core_out*core_out;
 arc_lut_prepare();
 const bool lut=cx==233 && cy==233 && rin>=ARC_LUT_R0+1 && rout<=ARC_LUT_R1-1;
 // Per row only the two runs that can touch the annulus are visited (a full ring used to test every pixel
 // of every damaged rectangle); the per-pixel test below is unchanged, so the pixels are identical.
 for(int y=y0;y<=y1;++y){
  const float dy=y+.5f-cy,dy2=dy*dy;if(dy2>rout2)continue;
  const float xo=halo_fast_sqrt(rout2-dy2)+.01f,xi=dy2<rin2?max(0.f,halo_fast_sqrt(rin2-dy2)-.01f):-1.f;
  int runs[2][2];int n=0;uint16_t *row=halo_row(y);
  const int l0=(int)floorf(cx-xo-.5f)-1,r1=(int)ceilf(cx+xo-.5f)+1;
  if(xi<0){runs[0][0]=l0;runs[0][1]=r1;n=1;}
  else{const int l1=(int)ceilf(cx-xi-.5f)+1,r0=(int)floorf(cx+xi-.5f)-1;
   if(l1>=r0){runs[0][0]=l0;runs[0][1]=r1;n=1;}else{runs[0][0]=l0;runs[0][1]=l1;runs[1][0]=r0;runs[1][1]=r1;n=2;}}
  for(int k=0;k<n;++k)for(int x=max(x0,runs[k][0]);x<=min(x1,runs[k][1]);++x){
  HALO_COUNT(1);const float dx=x+.5f-cx,d2=dx*dx+dy2;
  if(d2<rin2 || d2>rout2)continue;
  // Inside the angular range: cross products instead of atan2 per pixel; the solid core needs no sqrt.
  bool inside=full;
  if(!full){const float c0=u0x*dy-u0y*dx,c1=dx*u1y-dy*u1x;inside=wide?!(dx*u0y-dy*u0x>0 && u1x*dy-u1y*dx>0):(c0>=0 && c1>=0);}
  uint8_t coverage;
  if(inside && core && d2>=core2_in && d2<=core2_out)coverage=255;
  else{
   float dist;
   if(inside)dist=fabsf((lut?arc_sqrt_lut[(int)d2-ARC_LUT_K0]*(1.f/128):sqrtf(d2))-r);
   else{
    const float ax=x+.5f-e0x,ay=y+.5f-e0y,bx=x+.5f-e1x,by=y+.5f-e1y;
    dist=sqrtf(min(ax*ax+ay*ay,bx*bx+by*by));
   }
   if(dist>=outer)continue;
   coverage=dist<=half-.5f?255:(lut && inside?(uint8_t)(int)((outer-dist)*255+.5f):fast_aa_coverage(dist*dist,outer));
  }
  halo_put(row,x,color,halo_mul(alpha,coverage));
  }
 }
}

// ---- hairline + glow + pulse of the ring in one pass (the edit pulse, V6.0.6.5) ----
// All three depend only on the distance of a pixel centre from 233,233. Once per frame the three layers are
// composed per 1/64 px of distance into one linear blend: out = p*T + C (T = what shows through, C = the
// premultiplied colour, both in the same 5-bit steps as halo_blend565). A pixel is then one table lookup and,
// over a non-black background, one packed multiply-add; the distance comes from integer steps of the squared
// distance (exactly the float value, which is a quarter-integer here).
static constexpr int RING_TAB_MAX=1300;  // indexed by the distance in 1/64 px
struct RingEntry {uint32_t c32;uint16_t c565;uint8_t t5,pad;};
static RingEntry ring_tab[RING_TAB_MAX];
struct RingStackKey {float rh,hh,r,hg,hp;uint16_t ch,cg,cp;uint8_t ah,ag,ap;int s0,s1;};
static RingStackKey ring_key{-1,0,0,0,0,0,0,0,0,0,0,0,0};
static inline uint8_t ring_cov(float dist,float half){
 const float outer=half+.5f;if(dist>=outer)return 0;return dist<=half-.5f?255:(uint8_t)(int)((outer-dist)*255+.5f);
}
static RingEntry ring_entry(float s,const RingStackKey &k){
 const uint16_t col[3]={k.ch,k.cg,k.cp};
 const uint8_t a[3]={halo_mul(k.ah,ring_cov(fabsf(s-k.rh),k.hh)),halo_mul(k.ag,ring_cov(fabsf(s-k.r),k.hg)),halo_mul(k.ap,ring_cov(fabsf(s-k.r),k.hp))};
 float t=1,cr=0,cg=0,cb=0;
 for(int i=0;i<3;++i){if(!a[i])continue;const float f=a[i]>=252?1.f:(((uint32_t)a[i]+4)>>3)*(1.f/32);
  cr=cr*(1-f)+(col[i]>>11)*f;cg=cg*(1-f)+((col[i]>>5)&63)*f;cb=cb*(1-f)+(col[i]&31)*f;t*=1-f;}
 RingEntry e;e.t5=(uint8_t)(int)(t*32+.5f);e.pad=0;
 const int room=32-e.t5;  // p*T5 + C32 never overflows a channel
 const uint32_t r32=(uint32_t)min((int)(cr*32+.5f),31*room),g32=(uint32_t)min((int)(cg*32+.5f),63*room),b32=(uint32_t)min((int)(cb*32+.5f),31*room);
 e.c32=(r32<<11)|b32|(g32<<21);
 const uint32_t w=(e.c32>>5)&0x07E0F81Fu;e.c565=(uint16_t)(w|(w>>16));
 return e;
}
static bool HALO_HOT art_ring_stack(float rh,float hh,uint16_t ch,uint8_t ah,float r,float hg,uint16_t cg,uint8_t ag,float hp,uint16_t cp,uint8_t ap){
 const float band_in=min(rh-hh,r-max(hg,hp))-1.f,band_out=max(rh+hh,r+max(hg,hp))+1.f;
 if(band_in<ARC_LUT_R0+1 || band_out>ARC_LUT_R1-1)return false;
 const int k0=(int)(band_in*band_in),k1=(int)(band_out*band_out)+1;
 const int s0=(int)floorf(band_in*64)-1,s1=(int)ceilf(band_out*64)+1;
 if(s1-s0+1>RING_TAB_MAX)return false;
 arc_lut_prepare();
 RingStackKey key{rh,hh,r,hg,hp,ch,cg,cp,ah,ag,ap,s0,s1};
 if(memcmp(&key,&ring_key,sizeof(key))){
  ring_key=key;
  for(int q=s0;q<=s1;++q)ring_tab[q-s0]=ring_entry((q+.5f)*(1.f/64),key);
 }
 const float cx=233,cy=233,rin2=band_in*band_in,rout2=band_out*band_out;
 int x0=(int)floorf(cx-band_out),y0=(int)floorf(cy-band_out),x1=(int)ceilf(cx+band_out),y1=(int)ceilf(cy+band_out);
 if(!clip_box(x0,y0,x1,y1))return true;
 const uint16_t *const sq=arc_sqrt_lut-ARC_LUT_K0;const RingEntry *const tab=ring_tab-s0;
 const uint32_t span=(uint32_t)(k1-k0);
 for(int y=y0;y<=y1;++y){
  const float dy=y+.5f-cy,dy2=dy*dy;if(dy2>rout2)continue;
  const float xo=halo_fast_sqrt(rout2-dy2)+.01f,xi=dy2<rin2?max(0.f,halo_fast_sqrt(rin2-dy2)-.01f):-1.f;
  int runs[2][2];int n=0;uint16_t *row=halo_row(y);
  const int l0=(int)floorf(cx-xo-.5f)-1,r1=(int)ceilf(cx+xo-.5f)+1;
  if(xi<0){runs[0][0]=l0;runs[0][1]=r1;n=1;}
  else{const int l1=(int)ceilf(cx-xi-.5f)+1,r0=(int)floorf(cx+xi-.5f)-1;
   if(l1>=r0){runs[0][0]=l0;runs[0][1]=r1;n=1;}else{runs[0][0]=l0;runs[0][1]=l1;runs[1][0]=r0;runs[1][1]=r1;n=2;}}
  const int32_t y2=2*y-465,yy=y2*y2;  // 4*dy*dy
  for(int q=0;q<n;++q){const int xa=max(x0,runs[q][0]),xb=min(x1,runs[q][1]);
   int32_t x2=2*xa-465,d4=x2*x2+yy;  // 4*(dx*dx+dy*dy), exact
   for(int x=xa;x<=xb;++x,d4+=4*x2+4,x2+=2){
    HALO_COUNT(1);const int k=d4>>2;
    if((uint32_t)(k-k0)>span)continue;
    uint16_t &p=row[x];const RingEntry &e=tab[sq[k]>>1];
    if(!p)p=e.c565;
    else{const uint32_t w=(((p|((uint32_t)p<<16))&0x07E0F81Fu)*e.t5+e.c32)>>5&0x07E0F81Fu;p=(uint16_t)(w|(w>>16));}
   }
  }
 }
 return true;
}
// ---- two concentric full rings in one pass (the edit pulse and its glow): the distance is computed once per
// pixel and both are blended in the same order as two art_arc calls would (glow first, then the pulse).
static void HALO_HOT art_ring2(float cx,float cy,float r,float half_g,uint16_t cg,uint8_t ag,float half_p,uint16_t cp,uint8_t ap){
 if(!ag || !ap){if(ag)art_arc(g_frame,cx,cy,r,0,360,half_g,cg,ag);if(ap)art_arc(g_frame,cx,cy,r,0,360,half_p,cp,ap);return;}
 const float half=max(half_g,half_p),band=half+1.f,rin=max(0.f,r-band),rout=r+band,rin2=rin*rin,rout2=rout*rout;
 const float og=half_g+.5f,op=half_p+.5f;
 int x0=(int)floorf(cx-rout),y0=(int)floorf(cy-rout),x1=(int)ceilf(cx+rout),y1=(int)ceilf(cy+rout);
 if(!clip_box(x0,y0,x1,y1))return;
 arc_lut_prepare();
 const bool lut=cx==233 && cy==233 && rin>=ARC_LUT_R0+1 && rout<=ARC_LUT_R1-1;
 for(int y=y0;y<=y1;++y){
  const float dy=y+.5f-cy,dy2=dy*dy;if(dy2>rout2)continue;
  const float xo=halo_fast_sqrt(rout2-dy2)+.01f,xi=dy2<rin2?max(0.f,halo_fast_sqrt(rin2-dy2)-.01f):-1.f;
  int runs[2][2];int n=0;uint16_t *row=halo_row(y);
  const int l0=(int)floorf(cx-xo-.5f)-1,r1=(int)ceilf(cx+xo-.5f)+1;
  if(xi<0){runs[0][0]=l0;runs[0][1]=r1;n=1;}
  else{const int l1=(int)ceilf(cx-xi-.5f)+1,r0=(int)floorf(cx+xi-.5f)-1;
   if(l1>=r0){runs[0][0]=l0;runs[0][1]=r1;n=1;}else{runs[0][0]=l0;runs[0][1]=l1;runs[1][0]=r0;runs[1][1]=r1;n=2;}}
  for(int k=0;k<n;++k)for(int x=max(x0,runs[k][0]);x<=min(x1,runs[k][1]);++x){
   HALO_COUNT(1);const float dx=x+.5f-cx,d2=dx*dx+dy2;
   if(d2<rin2 || d2>rout2)continue;
   const float dist=fabsf((lut?arc_sqrt_lut[(int)d2-ARC_LUT_K0]*(1.f/128):sqrtf(d2))-r);
   uint16_t &p=row[x];
   if(dist<og){const uint8_t c=dist<=half_g-.5f?255:(uint8_t)(int)((og-dist)*255+.5f);p=halo_blend565(p,cg,halo_mul(ag,c));}
   if(dist<op){const uint8_t c=dist<=half_p-.5f?255:(uint8_t)(int)((op-dist)*255+.5f);p=halo_blend565(p,cp,halo_mul(ap,c));}
  }
 }
}
// ---- line whose opacity fades from a0 at the start to a1 at the end (tyre tracks) ----
static void HALO_HOT art_line_fade(uint16_t *dst,float x0,float y0,float x1,float y1,float radius,uint16_t color,float a0,float a1){
 if(a0<=0 && a1<=0)return;
 const float dx=x1-x0,dy=y1-y0,len2=dx*dx+dy*dy,inv=len2>.001f?1.f/len2:0.f,outer=radius+.5f,outer2=outer*outer,inner=max(0.f,radius-.5f),inner2=inner*inner;
 int left=(int)floorf(min(x0,x1)-radius-1),right=(int)ceilf(max(x0,x1)+radius+1),top=(int)floorf(min(y0,y1)-radius-1),bottom=(int)ceilf(max(y0,y1)+radius+1);
 if(!clip_box(left,top,right,bottom))return;
 const LineGeo geo=line_geo(dx,dy,len2,outer,outer2);
 for(int y=top;y<=bottom;++y){
  float lo,hi;if(!line_row_span(geo,y+.5f-y0,lo,hi))continue;
  uint16_t *row=halo_row(y);
  for(int x=max(left,(int)floorf(x0+lo-.5f)-1);x<=min(right,(int)ceilf(x0+hi-.5f)+1);++x){
  HALO_COUNT(0);float px=x+.5f-x0,py=y+.5f-y0,t=clamp_float((px*dx+py*dy)*inv,0,1);
  float d2=(px-t*dx)*(px-t*dx)+(py-t*dy)*(py-t*dy);
  if(d2>=outer2)continue;
  uint8_t coverage=255;if(radius<.5f || d2>inner2)coverage=fast_aa_coverage(d2,outer);
  halo_put(row,x,color,halo_mul(alpha8(a0+(a1-a0)*t),coverage));
  }
 }
}

// ---- polygon fill, 4 samples per pixel (thin airfoil of the wing) ----
// Scanline form of the same even-odd test: per sample row the edge crossings are computed once and sorted,
// then each sample is inside when an odd number of crossings lie to its right (identical to ray casting per
// sample, without testing every edge for every pixel — the zoomed wing used to cost ~1400 cycles per pixel).
static void HALO_HOT fill_polygon_aa(uint16_t *dst,const ArtPoint *p,int n,uint16_t color,uint8_t alpha){
 if(!alpha || n<3 || n>40)return;
 HaloBox box;for(int i=0;i<n;++i)box.add(p[i].x,p[i].y);
 int x0=(int)floorf(box.x0),y0=(int)floorf(box.y0),x1=(int)ceilf(box.x1),y1=(int)ceilf(box.y1);
 if(!clip_box(x0,y0,x1,y1))return;
 float xs[2][40];int count[2];
 for(int y=y0;y<=y1;++y){
  for(int s=0;s<2;++s){
   const float py=y+(s?.75f:.25f);int c=0;
   for(int i=0,j=n-1;i<n;j=i++)
    if((p[i].y>py)!=(p[j].y>py)){const float xc=(p[j].x-p[i].x)*(py-p[i].y)/(p[j].y-p[i].y)+p[i].x;
     int at=c++;while(at>0 && xs[s][at-1]>xc){xs[s][at]=xs[s][at-1];--at;}xs[s][at]=xc;}
   count[s]=c;
  }
  if(!count[0] && !count[1])continue;
  int seen[2][2]={{0,0},{0,0}};uint16_t *row=halo_row(y);  // per sample row, per sample column: crossings at or left of the sample
  for(int x=x0;x<=x1;++x){
   HALO_COUNT(2);int inside=0;
   for(int s=0;s<2;++s)for(int k=0;k<2;++k){
    const float px=x+(k?.75f:.25f);int &at=seen[s][k];
    while(at<count[s] && !(px<xs[s][at]))++at;
    inside+=(count[s]-at)&1;
   }
   if(inside)halo_put(row,x,color,halo_mul(alpha,(uint8_t)(inside*255/4)));
  }
 }
}

// ---- text, clipped like every other primitive; letter spacing and x offset for the glides ----
static void HALO_HOT halo_glyph(uint16_t *dst,const SDFont &font,const SDGlyph &g,int16_t cx,int16_t base,uint16_t color,uint8_t opacity){
 if(!g.width || !g.height || !g.data_length)return;
 const int gx=cx+g.x_offset,gy=base+g.y_offset;
 if(gx>=art_clip.x+art_clip.w || gy>=art_clip.y+art_clip.h || gx+g.width<=art_clip.x || gy+g.height<=art_clip.y)return;
 // Runs are walked with a column/row cursor (no division per pixel); rows outside the clip are skipped whole.
 uint32_t index=0;const uint32_t count=(uint32_t)g.width*g.height;int col=0,row=0;
 for(uint16_t i=0;i<g.data_length && index<count;++i){
  const uint8_t packed=pgm_read_byte(font.data+g.data_offset+i),a=halo_mul((packed&15)*17,opacity);
  uint8_t run=(packed>>4)+1;
  while(run && index<count){
   const uint8_t n=(uint8_t)min((int)run,g.width-col);
   const int y=gy+row;
   if(a && y>=art_clip.y && y<art_clip.y+art_clip.h){
    const int xs=max(gx+col,(int)art_clip.x),xe=min(gx+col+n,(int)(art_clip.x+art_clip.w));
    uint16_t *row=halo_row(y);for(int x=xs;x<xe;++x){HALO_COUNT(3);halo_put(row,x,color,a);}
   }
   run-=n;index+=n;col+=n;if(col==g.width){col=0;++row;}
  }
 }
}
static void halo_text(uint16_t *dst,const SDFont &font,const char *text,float center_x,int16_t base,uint16_t color,float alpha,int8_t spacing=0){
 const uint8_t opacity=alpha8(alpha);if(!opacity || !text || !*text)return;
 int16_t x=(int16_t)lroundf(center_x-measure_text(font,text,spacing)/2.f);
 for(const char *c=text;*c;++c){SDGlyph g;if(find_glyph(font,(uint8_t)*c,g)){halo_glyph(dst,font,g,x,base,color,opacity);x+=g.advance+spacing;}}
}
