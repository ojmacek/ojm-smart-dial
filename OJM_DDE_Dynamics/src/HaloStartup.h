#pragma once
#include "SignatureAssets.h"
// Approved startup (V6.0.6, prototype variant 10 "Nekonečno → laser"):
//  - two balls with the white ribbon arrive from both edges and draw one half of the infinity each at the same
//    constant speed; they cross in the centre and end on each other's start;
//  - ball B accelerates through a short link into the O and becomes the laser: it traces every outline of the
//    logo (with a few sparks); the whole logo then brightens to white at once;
//  - hold, close into the white dot, Titan — total 6.49 s.
// Geometry and timing are computed once (no allocation, fixed static tables); every phase is a pure function
// of the elapsed time, so any frame can be repainted rectangle by rectangle.
static constexpr uint16_t STARTUP_WHITE=0xF7BE,LOGO_WHITE=0xFFFF,LASER_LINE=0xDF3F,LASER_SPARK=0xCF1F,TITAN_WING=0x2ADF;
// The Titan ends exactly in the first UI frame: body alpha = UI dim 0.9, the UI white.
static constexpr uint16_t STARTUP_WHITE_UI=0xF79D;static constexpr uint8_t STARTUP_BODY_ALPHA=230;
static constexpr uint32_t STARTUP_HOLD_MS=900,STARTUP_CLOSE_MS=800,STARTUP_TITAN_MS=1100,STARTUP_FILL_MS=850;
static constexpr float INF_CX=233,INF_CY=224,INF_RX=150,INF_RY=52,INF_LX=INF_CX-INF_RX;
static float halo_soft(float p){p=clamp_float(p,0,1);return p*p*p*(10+p*(-15+6*p));}
static float halo_phase(float p,float a,float b){return clamp_float((p-a)/(b-a),0,1);}

// chunk: bounding boxes of 16 segments each, so a rectangle skips whole parts of a long polyline.
struct HaloTrack {ArtPoint *p;float *cum;uint16_t n;float total;HaloBox *chunk=nullptr;};
static constexpr uint16_t TRACK_CHUNK=16;
static ArtPoint halo_track_at(const HaloTrack &t,float d){
 if(d<=0)return t.p[0];
 if(d>=t.total)return t.p[t.n-1];
 uint16_t lo=1,hi=t.n-1;
 while(lo<hi){uint16_t mid=(lo+hi)/2;if(t.cum[mid]<d)lo=mid+1;else hi=mid;}
 const float seg=t.cum[lo]-t.cum[lo-1],f=seg>0?(d-t.cum[lo-1])/seg:0;
 return {t.p[lo-1].x+(t.p[lo].x-t.p[lo-1].x)*f,t.p[lo-1].y+(t.p[lo].y-t.p[lo-1].y)*f};
}
static void halo_chunks(HaloTrack &t,HaloBox *boxes){
 t.chunk=boxes;
 for(uint16_t c=0;c*TRACK_CHUNK+1<t.n;++c){HaloBox b;
  for(uint16_t i=c*TRACK_CHUNK;i<=min((uint16_t)(t.n-1),(uint16_t)(c*TRACK_CHUNK+TRACK_CHUNK));++i)b.add(t.p[i].x,t.p[i].y);
  boxes[c]=b;}
}
static void halo_measure(HaloTrack &t){t.cum[0]=0;for(uint16_t i=1;i<t.n;++i)t.cum[i]=t.cum[i-1]+hypotf(t.p[i].x-t.p[i-1].x,t.p[i].y-t.p[i-1].y);t.total=t.cum[t.n-1];}

static constexpr int CUBIC_N=72,INF_N=420,HALF_A_N=210,HALF_B_N=211;
static constexpr int TRACK_A_N=CUBIC_N+HALF_A_N-1,TRACK_B_N=CUBIC_N+HALF_B_N-1+CUBIC_N-1;
static ArtPoint inf_pts[INF_N],track_a_pts[TRACK_A_N],track_b_pts[TRACK_B_N],tour_pts[HALO_CONTOUR_POINTS];
static float half_a_cum[HALF_A_N],half_b_cum[HALF_B_N],track_a_cum[TRACK_A_N],track_b_cum[TRACK_B_N],tour_cum[HALO_CONTOUR_POINTS];
static HaloTrack half_a{inf_pts,half_a_cum,HALF_A_N,0},half_b{inf_pts+HALF_A_N-1,half_b_cum,HALF_B_N,0};
static HaloTrack track_a{track_a_pts,track_a_cum,TRACK_A_N,0},track_b{track_b_pts,track_b_cum,TRACK_B_N,0};
static HaloTrack tour[HALO_CONTOUR_COUNT];
static HaloBox half_a_boxes[(HALF_A_N+TRACK_CHUNK-2)/TRACK_CHUNK],half_b_boxes[(HALF_B_N+TRACK_CHUNK-2)/TRACK_CHUNK];
static HaloBox tour_boxes[HALO_CONTOUR_POINTS/TRACK_CHUNK+HALO_CONTOUR_COUNT+1];static uint16_t tour_box_used=0;
static float tour_len=0,start_e=0,start_h1=0,start_lk=0;
static constexpr int DIST_MAX=3400;
static float dist_at[DIST_MAX];
static uint16_t trace_end=0,close_at=0;
static uint32_t startup_draw_ms=0,startup_hold_end=0,startup_titan_at=0,startup_total_ms=0,startup_fill_start=0;
static float ribbon_taper[56];
static bool startup_ready=false;

static void halo_cubic(ArtPoint *out,ArtPoint a,ArtPoint b,ArtPoint c,ArtPoint d){
 for(int i=0;i<CUBIC_N;++i){float p=i/(float)(CUBIC_N-1),q=1-p;
  out[i]={q*q*q*a.x+3*q*q*p*b.x+3*q*p*p*c.x+p*p*p*d.x,q*q*q*a.y+3*q*q*p*b.y+3*q*p*p*c.y+p*p*p*d.y};}
}
static ArtPoint tour_point(uint8_t i,float f){return halo_track_at(tour[i],clamp_float(f,0,1)*tour[i].total);}
struct TourHead {uint8_t i;float p;};
static TourHead tour_head(float q){
 float l=clamp_float(q,0,1)*tour_len;
 for(uint8_t i=0;i<HALO_CONTOUR_COUNT;++i){if(l<=tour[i].total || i==HALO_CONTOUR_COUNT-1)return {i,min(1.f,tour[i].total>0?l/tour[i].total:1.f)};l-=tour[i].total;}
 return {0,0};
}
static void startup_prepare(){
 if(startup_ready)return;
 for(int i=0;i<INF_N;++i){float a=-PI/2+2*PI*i/(INF_N-1);inf_pts[i]={INF_CX+INF_RX*sinf(a),INF_CY+INF_RY*sinf(2*a)};}
 halo_measure(half_a);halo_measure(half_b);halo_chunks(half_a,half_a_boxes);halo_chunks(half_b,half_b_boxes);
 uint16_t o=0;
 for(uint8_t i=0;i<HALO_CONTOUR_COUNT;++i){
  const auto &c=HALO_CONTOURS[i];
  for(uint16_t k=0;k<c.count;++k)tour_pts[o+k]={c.points[k].x,c.points[k].y};
  tour[i]={tour_pts+o,tour_cum+o,c.count,0};halo_measure(tour[i]);tour_len+=tour[i].total;o+=c.count;
  halo_chunks(tour[i],tour_boxes+tour_box_used);tour_box_used+=(c.count+TRACK_CHUNK-2)/TRACK_CHUNK;
 }
 const ArtPoint t0=tour_point(0,0),t1=tour_point(0,.02f);
 float tx=t1.x-t0.x,ty=t1.y-t0.y,tl=hypotf(tx,ty);if(tl<1e-6f)tl=1;tx/=tl;ty/=tl;
 ArtPoint entry[CUBIC_N],link[CUBIC_N];
 halo_cubic(entry,{INF_CX-252,INF_CY+46},{INF_CX-205,INF_CY+42},{INF_LX,INF_CY+46},{INF_LX,INF_CY});
 halo_cubic(link,{INF_LX,INF_CY},{INF_LX,INF_CY-34},{t0.x-tx*34,t0.y-ty*34},t0);
 // Ball A: entry from the left + first half. Ball B: mirrored entry + second half + link into the O.
 int n=0;for(int i=0;i<CUBIC_N;++i)track_a_pts[n++]=entry[i];for(int i=1;i<HALF_A_N;++i)track_a_pts[n++]=inf_pts[i];
 n=0;for(int i=0;i<CUBIC_N;++i)track_b_pts[n++]={2*INF_CX-entry[i].x,entry[i].y};
 for(int i=1;i<HALF_B_N;++i)track_b_pts[n++]=inf_pts[HALF_A_N-1+i];for(int i=1;i<CUBIC_N;++i)track_b_pts[n++]=link[i];
 halo_measure(track_a);halo_measure(track_b);
 float le=0,lk=0;for(int i=1;i<CUBIC_N;++i){le+=hypotf(entry[i].x-entry[i-1].x,entry[i].y-entry[i-1].y);lk+=hypotf(link[i].x-link[i-1].x,link[i].y-link[i-1].y);}
 start_e=le;start_h1=half_a.total;start_lk=lk;
 // One constant speed for both halves, a smooth ramp through the link, the laser at constant speed, a short ease-out.
 const float V1=.5f,VL=1.72f,lend=start_e+start_h1+start_lk+tour_len;
 float d=0,tm=0;int next=1;dist_at[0]=0;int count=1;
 while(d<lend && count<DIST_MAX){
  float v=d<start_e+start_h1?V1:d<start_e+start_h1+start_lk?V1+(VL-V1)*halo_soft((d-start_e-start_h1)/start_lk):VL*(.35f+.65f*halo_soft((lend-d)/120));
  d+=v*.25f;tm+=.25f;if(tm>=next){dist_at[count++]=min(d,lend);++next;}
 }
 if(count<DIST_MAX && dist_at[count-1]<lend)dist_at[count++]=lend; // the tour always completes
 trace_end=count-1;close_at=0;while(close_at<trace_end && dist_at[close_at]<start_e+start_h1)++close_at;
 startup_fill_start=trace_end-60;
 startup_draw_ms=(uint32_t)ceilf(startup_fill_start+STARTUP_FILL_MS+150);
 startup_hold_end=startup_draw_ms+STARTUP_HOLD_MS;startup_titan_at=startup_hold_end+STARTUP_CLOSE_MS;
 startup_total_ms=startup_titan_at+STARTUP_TITAN_MS;
 for(int i=0;i<56;++i)ribbon_taper[i]=powf(i/55.f,.72f);
 startup_ready=true;
}
static uint8_t startup_stage(uint32_t e){return e<startup_draw_ms?0:e<startup_hold_end?1:e<startup_titan_at?2:3;}

// ---- shared pieces ----
static void halo_star(float x,float y,float alpha,float size){
 if(alpha<=0)return;
 art_dot(g_frame,x,y,size*2.5f,STARTUP_WHITE,alpha8(.038f*alpha*.72f));
 art_dot(g_frame,x,y,size*1.65f,STARTUP_WHITE,alpha8(.075f*alpha*.72f));
 art_dot(g_frame,x,y,size,STARTUP_WHITE,alpha8(alpha));
 art_dot(g_frame,x-.8f,y-.9f,size*.31f,STARTUP_WHITE,alpha8(alpha));
}
static void HALO_HOT halo_polyline(const HaloTrack &t,float length,float radius,uint16_t color,uint8_t alpha){
 if(!alpha || length<=0)return;
 const float pad=radius+2;
 for(uint16_t c=0;c*TRACK_CHUNK+1<t.n;++c){
  const uint16_t first=c*TRACK_CHUNK+1,last=min((uint16_t)(t.n-1),(uint16_t)(c*TRACK_CHUNK+TRACK_CHUNK));
  if(t.cum[first-1]>=length)break;
  if(t.chunk){const auto &b=t.chunk[c];if(!clip_hits(b.x0-pad,b.y0-pad,b.x1+pad,b.y1+pad))continue;}
  for(uint16_t i=first;i<=last;++i){
   if(t.cum[i-1]>=length)break;
   ArtPoint a=t.p[i-1],b=t.p[i];
   if(t.cum[i]>length){float f=(length-t.cum[i-1])/(t.cum[i]-t.cum[i-1]);b={a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f};}
   art_line(g_frame,a.x,a.y,b.x,b.y,radius,color,alpha);
  }
 }
}
// The white ribbon of the approved signature, along any track (travel and span as fractions of the track).
// The same frame paints many rectangles: the geometry is built once per frame and then only drawn (clipped).
struct RibbonPiece {ArtPoint a,b;float nx,ny,w,z;};
struct RibbonCache {const HaloTrack *t=nullptr;float travel=-1,span=0,band=0,twist=0;HaloBox box;ArtPoint head{0,0};int used=0;RibbonPiece pieces[55];};
static RibbonCache ribbon_cache[2];
static HaloBox halo_ribbon(const HaloTrack &t,float travel,float span,float alpha,float band,float twist,bool draw){
 HaloBox box;if(alpha<=0)return box;
 constexpr int count=56;
 RibbonCache &c=ribbon_cache[&t==&track_a?0:1];
 if(c.t!=&t || c.travel!=travel || c.span!=span || c.band!=band || c.twist!=twist){
  c.t=&t;c.travel=travel;c.span=span;c.band=band;c.twist=twist;c.box=HaloBox{};c.used=0;
  ArtPoint pts[count];float depth[count];
  const float begin=max(0.f,travel-span);
  for(int i=0;i<count;++i){float f=begin+(travel-begin)*i/(count-1);pts[i]=halo_track_at(t,f*t.total);depth[i]=.5f+.5f*sinf(f*2*PI*4+twist);c.box.add(pts[i].x,pts[i].y,10);}
  for(int i=1;i<count;++i){
   const float dx=pts[i].x-pts[i-1].x,dy=pts[i].y-pts[i-1].y,l=sqrtf(dx*dx+dy*dy);if(l<.001f)continue;
   const float z=clamp_float(depth[i],0,1);RibbonPiece piece={pts[i-1],pts[i],-dy/l,dx/l,band*(.23f+.77f*z)*ribbon_taper[i],z};
   int at=c.used;while(at>0 && c.pieces[at-1].z>z){c.pieces[at]=c.pieces[at-1];--at;}c.pieces[at]=piece;++c.used;
  }
  c.head=pts[count-1];
 }
 box=c.box;
 if(!draw || !clip_hits(box.x0,box.y0,box.x1,box.y1))return box;
 for(int i=0;i<c.used;++i){const auto &p=c.pieces[i];const float off=p.w*.42f;
  art_line(g_frame,p.a.x,p.a.y,p.b.x,p.b.y,max(.2f,p.w*.5f),STARTUP_WHITE,alpha8(alpha*(.18f+.52f*p.z)));
  art_line(g_frame,p.a.x+p.nx*off,p.a.y+p.ny*off,p.b.x+p.nx*off,p.b.y+p.ny*off,.35f,STARTUP_WHITE,alpha8(alpha*(.35f+.6f*p.z)));
  if(p.w>3)art_line(g_frame,p.a.x-p.nx*off,p.a.y-p.ny*off,p.b.x-p.nx*off,p.b.y-p.ny*off,.25f,STARTUP_WHITE,alpha8(alpha*.12f));
 }
 halo_star(c.head.x,c.head.y,alpha,3.05f);
 return box;
}
// Ink is stored row by row, sorted by x: per row only the run inside the clip is visited (binary search).
static size_t logo_ink_at(size_t lo,size_t hi,uint16_t offset){
 while(lo<hi){size_t mid=(lo+hi)/2;SignatureInkPixel px;memcpy_P(&px,SIGNATURE_INK+mid,sizeof(px));if(px.offset<offset)lo=mid+1;else hi=mid;}
 return lo;
}
static void HALO_HOT logo_draw(uint8_t alpha){
 if(!alpha)return;
 const int row0=clamp_i32(art_clip.y-SIGNATURE_Y,0,SIGNATURE_H),row1=clamp_i32(art_clip.y+art_clip.h-SIGNATURE_Y,0,SIGNATURE_H);
 const int x0=clamp_i32(art_clip.x-SIGNATURE_X,0,SIGNATURE_W),x1=clamp_i32(art_clip.x+art_clip.w-SIGNATURE_X,0,SIGNATURE_W);
 if(x1<=x0)return;
 for(int row=row0;row<row1;++row){
  const size_t first=pgm_read_word(SIGNATURE_ROW_START+row),last=pgm_read_word(SIGNATURE_ROW_START+row+1);
  const uint16_t base=row*SIGNATURE_W;uint16_t *line=halo_row(SIGNATURE_Y+row);
  for(size_t i=logo_ink_at(first,last,base+x0);i<last;++i){SignatureInkPixel px;memcpy_P(&px,SIGNATURE_INK+i,sizeof(px));
   HALO_COUNT(5);const int x=px.offset-base;if(x>=x1)break;
   halo_put(line,SIGNATURE_X+x,LOGO_WHITE,halo_mul(px.alpha,alpha));}
 }
}
static float spark_hash(float i){float x=sinf(i*12.9898f+78.233f)*43758.5453f;return x-floorf(x);}

// ---- the drawing phase as a pure function of time ----
struct StartupFrame {
 float tm,dB,dA,tourD,k,infA,aOut,q,outline,bright;TourHead head;int frame;
};
static StartupFrame startup_frame(uint32_t tm_ms){
 StartupFrame f;f.tm=tm_ms;
 f.dB=dist_at[min((uint32_t)trace_end,tm_ms)];f.dA=min(f.dB,start_e+start_h1);
 f.tourD=f.dB-start_e-start_h1-start_lk;
 f.k=clamp_float((f.dA-start_e)/start_h1,0,1);
 f.infA=.5f*(1-halo_soft((f.tm-close_at)/380.f));
 f.aOut=clamp_float((f.tm-close_at)/220.f,0,1);
 // From the end of the track on the tour is complete (on the board the sums round a hair below tour_len with FMA,
 // so q stayed at 0.99999 and the laser head kept shining on the last letter through the fill).
 f.q=tm_ms>=trace_end?1.f:clamp_float(f.tourD/tour_len,0,1);f.head=tour_head(f.q);
 f.outline=1-halo_soft(halo_phase(f.tm,startup_fill_start+STARTUP_FILL_MS*.4f,startup_fill_start+STARTUP_FILL_MS));
 f.bright=f.tm>startup_fill_start?halo_soft((f.tm-startup_fill_start)/(float)STARTUP_FILL_MS):0;
 f.frame=(int)floorf(f.tm/4400.f*900);
 return f;
}
static float ball_a_alpha(const StartupFrame &f){return .88f*halo_soft(f.tm/80)*(1-halo_soft(f.aOut));}
static float ball_b_alpha(const StartupFrame &f){return f.tourD<30?.88f*halo_soft(f.tm/80)*(1-halo_soft((f.tourD+10)/40)):0;}
static ArtPoint spark_pos(const StartupFrame &f,int k,float &alpha){
 const float age=(k+.5f)*.004f,te=f.q-age*1.2f;alpha=0;if(te<=0)return {0,0};
 const auto e=tour_head(te);const ArtPoint o=tour_point(e.i,e.p);const float seed=f.frame-k;
 alpha=1-age/.04f;
 return {o.x+(spark_hash(seed)-.5f)*120*age,o.y-(40+spark_hash(seed+3)*60)*age+900*age*age};
}
static void startup_draw_phase(uint32_t tm_ms){
 const auto f=startup_frame(tm_ms);
 // The infinity: both halves grow together; the line fades once it is closed.
 if(f.infA>0){halo_polyline(half_a,f.k*half_a.total,.45f,STARTUP_WHITE,alpha8(f.infA));halo_polyline(half_b,f.k*half_b.total,.45f,STARTUP_WHITE,alpha8(f.infA));}
 if(f.aOut<1)halo_ribbon(track_a,clamp_float(f.dA/track_a.total,0,1),.11f,ball_a_alpha(f),5,-f.tm/900,true);
 if(f.aOut>0 && f.aOut<1)art_dot(g_frame,INF_CX+INF_RX,INF_CY,3+8*f.aOut,STARTUP_WHITE,alpha8(.4f*(1-f.aOut)));
 if(ball_b_alpha(f)>0)halo_ribbon(track_b,clamp_float(f.dB/track_b.total,0,1),.11f,ball_b_alpha(f),5,f.tm/900,true);
 // Laser: outline tour and sparks; then the whole logo brightens at once (white only).
 if(f.tourD>0 && f.outline>0){const uint8_t a=alpha8(.85f*f.outline);
  for(uint8_t i=0;i<=f.head.i;++i)halo_polyline(tour[i],(i<f.head.i?1:f.head.p)*tour[i].total,.45f,LASER_LINE,a);}
 if(f.q>0 && f.q<1){
  const ArtPoint h=tour_point(f.head.i,f.head.p);halo_star(h.x,h.y,1,2.6f);
  for(int k=0;k<10;++k){float a;ArtPoint s=spark_pos(f,k,a);if(a>0)art_dot(g_frame,s.x,s.y,.9f,LASER_SPARK,alpha8(a));}
 }
 if(f.bright>0)logo_draw(alpha8(f.bright));
}
static void startup_close(uint32_t e){
 const float u=.54f+.16f*(e-startup_hold_end)/(float)STARTUP_CLOSE_MS,p=smooth((u-.54f)/.11f);
 if(p<1){auto previous=art_clip;int half=(int)ceilf(SIGNATURE_W*.5f*(1-p));
  int x=max((int)previous.x,233-half),y=max((int)previous.y,(int)SIGNATURE_Y);
  int right=min((int)(previous.x+previous.w),233+half),bottom=min((int)(previous.y+previous.h),(int)(SIGNATURE_Y+SIGNATURE_H));
  art_clip={(int16_t)x,(int16_t)y,(int16_t)max(0,right-x),(int16_t)max(0,bottom-y)};logo_draw(255);art_clip=previous;
  art_dot(g_frame,233,233,2.5f,LOGO_WHITE,alpha8(smooth(p)));
 }
 if(u>.635f){float q=smooth((u-.635f)/.065f);art_dot(g_frame,233,233-34*q,2.5f,LOGO_WHITE,alpha8(1-halo_phase(u,.69f,.70f)));}
}
// The Active Aero wing: a flush airfoil that is part of the deck line (white edges, blue fill), lifting around
// its leading edge at 392,232 (model space, mapped by scale s and offset ox,oy). Shared by the Titan and the UI.
static void halo_wing(float s,float ox,float oy,float deg,float alpha,float zoom){
 if(alpha<=0.002f)return;
 const float A=deg*HALO_DEG+atan2f(1,18.6f),ch=hypotf(18.6f,1),dx=cosf(A),dy=-sinf(A),nx=sinf(A),ny=cosf(A);
 ArtPoint upper[17],lower[17],poly[34];
 for(int i=0;i<=16;++i){const float u=i/16.f,th=2.3f*powf(max(0.f,sinf(PI*min(1.f,u*1.08f))),.75f)*(1-.55f*u);
  const float x=392+u*ch*dx,y=232+u*ch*dy;upper[i]={x*s+ox,y*s+oy};lower[i]={(x+th*nx)*s+ox,(y+th*ny)*s+oy};}
 for(int i=0;i<17;++i){poly[i]=upper[i];poly[33-i]=lower[i];}
 fill_polygon_aa(g_frame,poly,34,TITAN_WING,alpha8(.9f*alpha));
 for(int i=1;i<17;++i)art_line(g_frame,upper[i-1].x,upper[i-1].y,upper[i].x,upper[i].y,.85f,STARTUP_WHITE_UI,alpha8(218/255.f*alpha));
 const float la=.55f*alpha*min(1.f,deg/4+zoom);
 if(la>0)for(int i=1;i<17;++i)art_line(g_frame,lower[i-1].x,lower[i-1].y,lower[i].x,lower[i].y,.55f,STARTUP_WHITE_UI,alpha8(la));
}
static float startup_wing_target();  // the selected Aero wing angle (from the UI), so the Titan hands over seamlessly
static float startup_titan_time(uint32_t e){return (e-startup_titan_at)*1280.f/STARTUP_TITAN_MS;}
static float startup_wing_alpha(float time){return time>770?smooth((time-770)/120.f):0;}
static float startup_wing_angle(float time){const float u=clamp_float((time-770)/460.f,0,1),q=u-1;return startup_wing_target()*(1+1.6f*q*q*q+.6f*q*q);}
static void startup_titan(uint32_t e){
 const float time=startup_titan_time(e);
 for(int i=0;i<6;++i){float delay=i<2?0:i==2?130:260;path_draw(g_frame,i,smooth((time-delay)/900.f),STARTUP_BODY_ALPHA,true);}
 halo_wing(1,0,0,startup_wing_angle(time),startup_wing_alpha(time),0);
}
static void render_startup_scene(uint32_t e){
 startup_prepare();
 if(e<startup_draw_ms)startup_draw_phase(e);
 else if(e<startup_hold_end)logo_draw(255);
 else if(e<startup_titan_at)startup_close(e);
 else startup_titan(e);
}

// ---- damage: only what changed since the previous startup frame ----
static uint32_t startup_previous=UINT32_MAX;
static DamageSlot startup_slots[6];
static void startup_mark_damage(uint32_t e){
 startup_prepare();
 const uint8_t stage=startup_stage(e);
 if(startup_previous==UINT32_MAX || e<startup_previous){
  // First frame: the panel is black, the tracked items mark themselves. A restarted startup (tests) clears all.
  if(startup_previous!=UINT32_MAX)dirty(0,0,LCD_W,LCD_H);
  for(auto &s:startup_slots)s=DamageSlot{};startup_previous=e;
 }else if(stage!=startup_stage(startup_previous)){
  // Phase change: only what the old phase still showed (its tracked items, the rising dot); the phases join
  // on identical pictures, so there is no full-area repaint (that was a 60 ms frame in 6.0.6).
  // A long stall can jump over a whole phase, so the old phase's picture is cleared by what it can show.
  const uint8_t old=startup_stage(startup_previous);
  for(auto &s:startup_slots){if(s.on)dirty(s.x,s.y,s.w,s.h);s=DamageSlot{};}
  if(old==1)dirty(SIGNATURE_X,SIGNATURE_Y,SIGNATURE_W,SIGNATURE_H);
  if(old==2){const float u=.54f+.16f*(startup_previous-startup_hold_end)/(float)STARTUP_CLOSE_MS;
   const int half=(int)ceilf(SIGNATURE_W*.5f*(1-smooth((u-.54f)/.11f)));
   if(half>0)dirty(233-half,SIGNATURE_Y,2*half,SIGNATURE_H);dirty(229,195,8,42);}
 }
 if(stage==0){
  const auto f=startup_frame(e),p=startup_frame(startup_previous);
  HaloBox inf;inf.add(INF_CX-INF_RX,INF_CY-INF_RY,3);inf.add(INF_CX+INF_RX,INF_CY+INF_RY,3);
  // The infinity: while it only grows, just the new piece of each half; any alpha change repaints all of it.
  const bool inf_on=f.infA>0 && f.k>0;
  if(inf_on && startup_slots[0].on && f.infA==p.infA){
   const HaloTrack *const halves[2]={&half_a,&half_b};
   if(f.k!=p.k)for(const HaloTrack *h:halves){
    const float la=p.k*h->total,lb=f.k*h->total;HaloBox piece;
    auto pa=halo_track_at(*h,la),pb=halo_track_at(*h,lb);piece.add(pa.x,pa.y,2);piece.add(pb.x,pb.y,2);
    for(uint16_t i=0;i<h->n;++i)if(h->cum[i]>la && h->cum[i]<lb)piece.add(h->p[i].x,h->p[i].y,2);
    dirty((int16_t)floorf(piece.x0),(int16_t)floorf(piece.y0),(int16_t)(ceilf(piece.x1)-floorf(piece.x0))+1,(int16_t)(ceilf(piece.y1)-floorf(piece.y0))+1);
   }
  }else{HaloSig si;si.addf(f.k);si.addf(f.infA);damage_item(startup_slots[0],inf_on,inf,si.h);}
  HaloSig sa;sa.addf(f.dA);sa.addf(ball_a_alpha(f));sa.addf(f.tm/900);
  damage_item(startup_slots[1],f.aOut<1 && ball_a_alpha(f)>0,halo_ribbon(track_a,clamp_float(f.dA/track_a.total,0,1),.11f,ball_a_alpha(f),5,-f.tm/900,false),sa.h);
  HaloBox fl;fl.add(INF_CX+INF_RX,INF_CY,13);HaloSig sf;sf.addf(f.aOut);damage_item(startup_slots[2],f.aOut>0 && f.aOut<1,fl,sf.h);
  HaloSig sb;sb.addf(f.dB);sb.addf(ball_b_alpha(f));sb.addf(f.tm/900);
  damage_item(startup_slots[3],ball_b_alpha(f)>0,halo_ribbon(track_b,clamp_float(f.dB/track_b.total,0,1),.11f,ball_b_alpha(f),5,f.tm/900,false),sb.h);
  // New outline between the previous and the current laser position.
  if(f.q!=p.q){
   HaloBox seg;const float qa=min(p.q,f.q),qb=max(p.q,f.q);
   for(int s=0;s<=24;++s){auto h=tour_head(qa+(qb-qa)*s/24);auto pt=tour_point(h.i,h.p);seg.add(pt.x,pt.y,2);}
   const auto ha=tour_head(qa),hb=tour_head(qb);
   for(uint8_t i=ha.i;i<=hb.i;++i)for(uint16_t k=0;k<tour[i].n;++k){float d=tour[i].cum[k]/tour[i].total;
    if((i>ha.i || d>=ha.p) && (i<hb.i || d<=hb.p))seg.add(tour[i].p[k].x,tour[i].p[k].y,2);}
   dirty((int16_t)floorf(seg.x0),(int16_t)floorf(seg.y0),(int16_t)(ceilf(seg.x1)-floorf(seg.x0))+1,(int16_t)(ceilf(seg.y1)-floorf(seg.y0))+1);
  }
  // Head and sparks.
  HaloBox hs;HaloSig sh;
  if(f.q>0 && f.q<1){auto h=tour_point(f.head.i,f.head.p);hs.add(h.x,h.y,8);sh.addf(f.q);sh.add(f.frame);
   for(int k=0;k<10;++k){float a;auto sp=spark_pos(f,k,a);if(a>0)hs.add(sp.x,sp.y,2);}}
  damage_item(startup_slots[4],f.q>0 && f.q<1,hs,sh.h);
  // Brightening logo and the fading outline share the logo box.
  HaloBox lb;lb.add(SIGNATURE_X,SIGNATURE_Y,3);lb.add(SIGNATURE_X+SIGNATURE_W,SIGNATURE_Y+SIGNATURE_H,3);
  HaloSig sl;sl.addf(f.bright);sl.addf(f.outline);damage_item(startup_slots[5],f.bright>0 || f.outline<1,lb,sl.h);
 }else if(stage==2){
  auto half=[](uint32_t ms){float u=.54f+.16f*(ms-startup_hold_end)/(float)STARTUP_CLOSE_MS;return (int)ceilf(SIGNATURE_W*.5f*(1-smooth((u-.54f)/.11f)));};
  int a=half(startup_previous),b=half(e),outer=max(a,b),inner=min(a,b);
  if(outer!=inner){dirty(233-outer,SIGNATURE_Y,outer-inner,SIGNATURE_H);dirty(233+inner,SIGNATURE_Y,outer-inner,SIGNATURE_H);}
  dirty(229,195,8,42);
 }else if(stage==3){
  const float old_time=(startup_previous-startup_titan_at)*1280.f/STARTUP_TITAN_MS,time=(e-startup_titan_at)*1280.f/STARTUP_TITAN_MS;
  for(int path=0;path<6;++path){
   float delay=path<2?0:path==2?130:260;const auto &pp=ART_PATHS[path];
   float a=smooth((old_time-delay)/900.f)*(pp.count-1),b=smooth((time-delay)/900.f)*(pp.count-1);
   if(a==b)continue;
   HaloBox box;auto point=[&](float at){int i=min((int)at,(int)pp.count-2);float fr=at-i;return ArtPoint{pp.points[i].x+(pp.points[i+1].x-pp.points[i].x)*fr,pp.points[i].y+(pp.points[i+1].y-pp.points[i].y)*fr};};
   auto pa=point(a),pb=point(b);box.add(pa.x,pa.y,4);box.add(pb.x,pb.y,4);
   for(int i=(int)ceilf(a);i<=(int)floorf(b);++i)box.add(pp.points[i].x,pp.points[i].y,4);
   dirty((int16_t)floorf(box.x0),(int16_t)floorf(box.y0),(int16_t)(ceilf(box.x1)-floorf(box.x0))+1,(int16_t)(ceilf(box.y1)-floorf(box.y0))+1);
  }
  if(time>770)dirty(387,203,32,36);
 }
 startup_previous=e;
}
