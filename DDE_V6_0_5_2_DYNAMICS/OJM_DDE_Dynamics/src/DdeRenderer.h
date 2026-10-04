#pragma once
#include "TitanAssets.h"
#include "DdeFonts.h"
#include "OjmLogo.h"
#include "TitanStrokes.h"
#include "ArtFont.h"
#include "ArtTop.h"
#include "ArtLogo.h"
static constexpr uint16_t CYAN=0xF908,WHITE=0xFFDF,DIM=0x632C,ORANGE=0xFC48;
static constexpr uint32_t FRAME_US=16667,STARTUP_MS=3710,MODE_FADE_MS=220;
struct DirtyRect {int16_t x,y,w,h;};
static DirtyRect dirty_rects[16];
static uint8_t dirty_count=0,shown_page=255,last_mode=255;
static bool base_invalid=true,startup_active=true,shown_detail=false,mode_animating=false;
static uint32_t startup_started=0,last_render_us=0,mode_at=0;
struct RenderStats {
 uint32_t frames=0,draw_us=0,te_us=0,transfer_us=0,total_us=0,pixels=0;
 uint32_t max_draw_us=0,max_te_us=0,max_transfer_us=0,max_total_us=0,max_pixels=0;
 uint32_t over_budget=0;
};
static volatile RenderStats render_stats;
static const char *const MENU_NAMES[]={"REAR WHEEL STEERING","TORQUE VECTORING","ACTIVE AERO"};
static const int16_t WHEEL_X[]={163,303,163,303},WHEEL_Y[]={166,166,274,274};
static int16_t last_value[4]={-32768,-32768,-32768,-32768},last_tint[4]={-1,-1,-1,-1};
static int last_phase=-1,last_strength=-1;
static char last_status[40]={};
static uint8_t last_actual_mode=255;
static float smooth(float t){t=clamp_float(t,0,1);return t*t*(3-2*t);}
static DirtyRect art_clip={0,0,LCD_W,LCD_H};
static float edit_mix=0,rail_x=211,rail_velocity=0,top_mix=0;
static uint32_t ui_last=0,seen_nav=0,gesture_at=0,flow_at=0,page_at=0;
static int8_t gesture_dir=0;
static bool gesture_edge=false,gesture_confirm=false;
static int previous_page=2;
static float flow_phase=-1,last_flow=-1,body_level=1,last_body_level=-1;
static float page_mix=1,gesture=0;
static bool ui_moving=false;
static bool clipped(int x,int y){return x<art_clip.x || y<art_clip.y || x>=art_clip.x+art_clip.w || y>=art_clip.y+art_clip.h;}
static void dirty(int16_t x,int16_t y,int16_t w,int16_t h){
  if(w<=0 || h<=0)return;
  int16_t right=clamp_i32(x+w,0,LCD_W),bottom=clamp_i32(y+h,0,LCD_H);
  x=clamp_i32(x,0,LCD_W);y=clamp_i32(y,0,LCD_H);
  if(right<=x || bottom<=y)return;
  // CO5300 updates use even origins and even dimensions. Expand BEFORE
  // restore/draw/pack so the pixel stride and the panel window stay identical.
  // Aligning only the address window would send rows with the wrong stride.
  static_assert(LCD_W%2==0 && LCD_H%2==0,"CO5300 panel dimensions must be even");
  x&=~1;y&=~1;right=(right+1)&~1;bottom=(bottom+1)&~1;
  for(uint8_t i=0;i<dirty_count;){auto r=dirty_rects[i];
    if(x<=r.x+r.w && right>=r.x && y<=r.y+r.h && bottom>=r.y){
      right=max(right,(int16_t)(r.x+r.w));bottom=max(bottom,(int16_t)(r.y+r.h));x=min(x,r.x);y=min(y,r.y);
      dirty_rects[i]=dirty_rects[--dirty_count];i=0;
    }else ++i;
  }
  if(dirty_count==16){
    for(uint8_t i=0;i<dirty_count;++i){auto r=dirty_rects[i];right=max(right,(int16_t)(r.x+r.w));bottom=max(bottom,(int16_t)(r.y+r.h));x=min(x,r.x);y=min(y,r.y);}
    dirty_count=0;
  }
  dirty_rects[dirty_count++]={x,y,(int16_t)(right-x),(int16_t)(bottom-y)};
}
static void restore_dirty(){for(uint8_t i=0;i<dirty_count;++i){auto r=dirty_rects[i];copy_frame_rect(g_frame,g_base_frame,r.x,r.y,r.w,r.h);}}
static void push_dirty(uint32_t started){
  if(!dirty_count)return;
  uint32_t ready=micros();render_stats.draw_us=ready-started;
  wait_for_te_edge();uint32_t synced=micros();render_stats.te_us=synced-ready;
  render_stats.pixels=0;
  for(uint8_t i=0;i<dirty_count;++i){auto r=dirty_rects[i];push_frame_rect(r.x,r.y,r.w,r.h,false);render_stats.pixels+=(uint32_t)r.w*r.h;}
  uint32_t ended=micros();render_stats.transfer_us=ended-synced;render_stats.total_us=ended-started;++render_stats.frames;
  render_stats.max_draw_us=max(render_stats.max_draw_us,render_stats.draw_us);
  render_stats.max_te_us=max(render_stats.max_te_us,render_stats.te_us);
  render_stats.max_transfer_us=max(render_stats.max_transfer_us,render_stats.transfer_us);
  render_stats.max_total_us=max(render_stats.max_total_us,render_stats.total_us);
  render_stats.max_pixels=max(render_stats.max_pixels,render_stats.pixels);
}
static void outline(uint16_t *target,const OutlineAsset &asset,int16_t cx,int16_t cy,uint16_t color=WHITE,uint8_t opacity=230){
  int16_t ox=cx-asset.width/2,oy=cy-asset.height/2;
  for(uint16_t n=0;n<asset.count;++n){AssetSpan span;memcpy_P(&span,asset.spans+n,sizeof(span));
    int16_t x=ox+span.offset%asset.width,y=oy+span.offset/asset.width;
    uint8_t alpha=mul_alpha(span.alpha,opacity);
    for(uint8_t j=0;j<span.length;++j)if(!clipped(x+j,y))blend_pixel(target,x+j,y,color,alpha);
  }
}
static void text_at(const char *text,int y,const SDFont &font,uint16_t color=WHITE,uint8_t opacity=255){draw_text_centered(g_frame,font,text,233,y,color,opacity);}

static void art_line(uint16_t *dst,float x0,float y0,float x1,float y1,float radius,uint16_t color,uint8_t alpha=255){
 float dx=x1-x0,dy=y1-y0,len2=dx*dx+dy*dy;
 int left=floorf(min(x0,x1)-radius-1),right=ceilf(max(x0,x1)+radius+1);
 int top=floorf(min(y0,y1)-radius-1),bottom=ceilf(max(y0,y1)+radius+1);
 for(int y=max((int)art_clip.y,top);y<=min((int)(art_clip.y+art_clip.h-1),bottom);++y)for(int x=max((int)art_clip.x,left);x<=min((int)(art_clip.x+art_clip.w-1),right);++x){
  float px=x+.5f-x0,py=y+.5f-y0,t=len2>.001f?clamp_float((px*dx+py*dy)/len2,0,1):0;
  float a=clamp_float(radius+.5f-sqrtf((px-t*dx)*(px-t*dx)+(py-t*dy)*(py-t*dy)),0,1);
  if(a>0)blend_pixel(dst,x,y,color,mul_alpha(alpha,lroundf(a*255)));
 }
}
static void logo_unfold(float progress){
 // Climate-style centre-out strip reveal. Glyphs stay at their final size
 // and coordinates; only the visible window opens/closes. No scaling.
 int half=lroundf(ART_LOGO.width*.5f*smooth(progress));
 if(half<=0)return;
 auto previous=art_clip;
 art_clip={(int16_t)(233-half),(int16_t)(233-ART_LOGO.height/2),(int16_t)(half*2),(int16_t)ART_LOGO.height};
 outline(g_frame,ART_LOGO,233,233,WHITE,255);
 art_clip=previous;
}
static void art_dot(uint16_t *dst,float x,float y,float radius,uint16_t color,uint8_t alpha=255){art_line(dst,x,y,x,y,radius,color,alpha);}
static void path_draw(uint16_t *dst,int index,float progress,uint8_t alpha=255,bool dot=false){
 const auto &p=ART_PATHS[index];float amount=clamp_float(progress,0,1)*(p.count-1);
 for(int i=0;i<(int)ceilf(amount);++i){
  auto a=p.points[i],b=p.points[i+1];float t=min(1.f,amount-i);
  art_line(dst,a.x,a.y,a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,.85f,WHITE,alpha);
 }
 if(dot && amount>0 && progress<1){int i=min((int)amount,(int)p.count-2);auto a=p.points[i],b=p.points[i+1];float t=amount-i;
  fill_circle_aa(dst,a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,2,WHITE,alpha);
 }
}
static void artistic_body(uint16_t *dst){for(int i=0;i<6;++i)path_draw(dst,i,1,roundf(218*body_level));}
static void top_body(uint16_t *dst){outline(dst,ART_TOP,233,213,WHITE,roundf(218*body_level));}
static float aero_angle(float position){
 // Virtual ECU: Low Drag=5, Downforce=75, Air Brake=100.
 return position<=75?clamp_float((position-5)/70,0,1)*25:25+(position-75)/25*33;
}
static void wing_draw(uint16_t *dst,float position,uint8_t alpha=255){
 float a=aero_angle(position)*DEG_TO_RAD;
 art_line(dst,392,232,392+19*cosf(a),232-19*sinf(a),1.25f,CYAN,alpha);
 art_dot(dst,392,232,1.45f,CYAN,alpha);
}
static void build_base(){
 fill_rect(g_base_frame,0,0,LCD_W,LCD_H,0);
 // The base stays black; each damaged region composites the immutable outline
 // and its moving layer through a clip. No full-screen per-frame redraw.
}
static const char *status_text(uint8_t){return "";}
static const char *display_mode(){return mode_name(requested.page);}
static void labels(uint32_t now){
 uint8_t a=mode_animating?130+125*smooth((uint32_t)(now-mode_at)/(float)MODE_FADE_MS):255;
 text_at(MENU_NAMES[requested.page],96,SD_FONT_LABEL,WHITE,a);
 text_at(display_mode(),369-lroundf(edit_mix*2),ART_FONT,WHITE,a);
 // Visual page order: Aero, RWS, Vectoring. CAN page IDs stay unchanged.
 float y=399-5*edit_mix;
 for(int i=0;i<4;++i){float x=211+22*i-11*edit_mix;float alpha=i==3?edit_mix:1;
  art_line(g_frame,x-3,y,x+3,y,1.25f,DIM,roundf(180*alpha));}
 float stretch=4*gesture*(gesture_edge?-.4f:1.f);
 float x=rail_x+gesture_dir*(gesture_edge?2:0)*gesture;
 float press=gesture_confirm?sin(gesture*PI)*1.1f:0;
 art_line(g_frame,x-4-(gesture_dir<0?stretch:0)+press,y,x+4+(gesture_dir>0?stretch:0)-press,y,1.6f,CYAN);
 art_dot(g_frame,x+gesture_dir*stretch,y,2.f,CYAN);
}
static void sample_visual(uint8_t page,int16_t *value,int16_t *tint){
 for(int i=0;i<4;++i){
  value[i]=page==0?lroundf((i<2?visual.front:visual.rear)*4):page==1?lroundf(clamp_float(visual.torque[i],0,100)*2):lroundf(clamp_float(visual.aero,0,100)*2);
  tint[i]=page==1?lroundf(clamp_float(visual.tc[i],0,1)*15):page==0 && i>=2 && (effective_faults&1)?15:0;
 }
}
static void draw_visual(uint8_t page,uint8_t i,int16_t value,int16_t tint){
 float x=WHEEL_X[i],y=WHEEL_Y[i];
 if(page==2){wing_draw(g_frame,value*.5f,roundf(255*body_level));return;}
 float angle=(i<2?visual.front:visual.rear)*(1-top_mix);
 float a=roundf(angle*4)*.25f*DEG_TO_RAD,sx=sinf(a)*11,sy=cosf(a)*11;
 float strength=roundf(visual.vector_strength*100)/100.f;
 float load=clamp_float(visual.torque[i]/100.f,0,1);
 float phase=fmodf(floorf(visual.pulse_phase*256)/256.f+(i>=2?.18f:0.f),1.f);
 float arrival=smooth((phase-.68f)/.12f)*(1-smooth((phase-.80f)/.20f));
 uint16_t rws_color=i<2?WHITE:blend_rgb565(WHITE,CYAN,lroundf(220*clamp_float(fabsf(roundf(visual.rear*4)*.25f)/3.f,0,1)));
 uint16_t tv_color=blend_rgb565(DIM,CYAN,roundf(255*strength*(.35f+.65f*arrival)*(.55f+.45f*load)));
 uint16_t color=blend_rgb565(rws_color,tv_color,roundf(top_mix*255));
 art_line(g_frame,x-sx,y+sy,x+sx,y-sy,2.7f,color,roundf(255*body_level));
 // Brief turn cue entirely outside the body; no axle or centre line.
 float cue=(1-top_mix)*gesture*(i>=2?1.f:.35f);
 if(cue>.01f && fabsf(angle)>1){
  float side=i%2?1.f:-1.f;
  for(int n=0;n<7;++n){float t=n/7.f,u=(n+1)/7.f;
   float ax=x+side*(13+3*sinf(t*PI)),ay=y-12+24*t;
   float bx=x+side*(13+3*sinf(u*PI)),by=y-12+24*u;
   art_line(g_frame,ax,ay,bx,by,.65f,CYAN,roundf(140*cue));}
 }
 if(top_mix<.005f || strength<.003f)return;
 float energy=strength*top_mix*body_level;
 if(arrival>.01f){
  // One restrained outer echo at arrival; it stays clear of the silhouette.
  art_line(g_frame,x+(i%2?6:-6),y-12,x+(i%2?6:-6),y+12,.65f,CYAN,roundf(150*arrival*energy*load));
 }
 float side=i%2?1.f:-1.f;
 if(i%2==0){float emit=(1-smooth(phase/.20f))*energy;
  art_dot(g_frame,233,y,.8f+1.4f*emit,CYAN,roundf(180*emit));}
 // The pulse ends well INSIDE the outline. Its energy reappears at the wheel;
 // neither the pulse nor a permanent path crosses any white body pixel.
 for(int echo=0;echo<(strength>.68f?2:1);++echo){
  float p=(phase-echo*.115f)/.78f;
  if(p<=0 || p>=1)continue;
  float head=smooth(p),tail=max(0.f,head-(.14f+.16f*strength)*(.6f+.4f*load));
  float envelope=smooth(p/.10f)*(1-smooth((p-.80f)/.20f));
  float alpha=255*energy*(.4f+.6f*load)*envelope*(echo?.38f*smooth((strength-.68f)/.32f):1.f);
  art_line(g_frame,233+side*(4+24*tail),y,233+side*(4+24*head),y,.8f+.5f*strength,CYAN,roundf(alpha));
 }
}
static void airflow(){
 if(flow_phase<0)return;
 float bend=clamp_float((visual.aero-5)/95,0,1);
 ArtPoint points[]={{66,240},{110,218},{154,201},{195,189},{233,185},{278,188},{325,199},{369,214},{407,219},{435,219+22*bend}};
 float head=flow_phase*9,tail=max(0.f,head-1.15f);
 float envelope=smooth(flow_phase/.15f)*(1-smooth((flow_phase-.78f)/.22f));
 for(int i=0;i<9;++i){float lo=max(tail,(float)i),hi=min(head,(float)i+1);if(hi<=lo)continue;
  auto a=points[i],b=points[i+1];float l=lo-i,h=hi-i;
  art_line(g_frame,a.x+(b.x-a.x)*l,a.y+(b.y-a.y)*l,a.x+(b.x-a.x)*h,a.y+(b.y-a.y)*h,.65f,CYAN,roundf(155*envelope*body_level));
  if(i==8 && bend>.85f)art_line(g_frame,a.x+(b.x-a.x)*l,a.y-16*l,a.x+(b.x-a.x)*h,a.y-16*h,.55f,CYAN,roundf(95*envelope*body_level));
 }
}
static void paint_car(){
 if(requested.page==2)artistic_body(g_frame);else top_body(g_frame);
 int16_t v[4],t[4];sample_visual(requested.page,v,t);
 for(int i=0;i<(requested.page==2?1:4);++i)draw_visual(requested.page,i,v[i],t[i]);
 if(requested.page==2)airflow();
}
static void ui_service(uint32_t now,bool first){
 float dt=ui_last?min(.05f,(uint32_t)(now-ui_last)*.001f):.016667f;ui_last=now;
 if(seen_nav!=nav_revision){seen_nav=nav_revision;gesture_at=now;gesture_dir=nav_direction;gesture_edge=nav_boundary;gesture_confirm=nav_confirm;}
 bool change=first || last_mode!=page_mode(requested.page) || shown_detail!=detail_open;
 if(change){mode_at=now;mode_animating=true;gesture_at=now;flow_at=now+700;}
 if(first){previous_page=shown_page==255?requested.page:shown_page;page_at=now;}
 page_mix=smooth((uint32_t)(now-page_at)/220.f);
 float old_edit=edit_mix;
 edit_mix+=(float(detail_open)-edit_mix)*(1-expf(-dt/.055f));
 if(fabsf(edit_mix-float(detail_open))<.002f)edit_mix=float(detail_open);
 float target=211+22*((requested.page+1)%3);
 target+=(200+22*page_mode(requested.page)-target)*edit_mix;
 float old_x=rail_x;settle_axis(rail_x,rail_velocity,target,dt,26.f);
 if(fabsf(rail_x-target)<.025f && fabsf(rail_velocity)<.5f){rail_x=target;rail_velocity=0;}
 top_mix=requested.page==1?1.f:0.f;
 if(previous_page<2 && requested.page<2 && previous_page!=requested.page)top_mix=previous_page==1?1-page_mix:page_mix;
 gesture=1-smooth((uint32_t)(now-gesture_at)/220.f);
 body_level=1-.15f*edit_mix;
 if((previous_page==2)!=(requested.page==2))body_level*=.65f+.35f*page_mix;
 flow_phase=-1;
 if(requested.page==2 && (int32_t)(now-flow_at)>=0){uint32_t phase=(now-flow_at)%3200;if(phase<900)flow_phase=phase/900.f;}
 ui_moving=change || edit_mix!=old_edit || old_x!=rail_x || gesture>0 || page_mix<1;
 shown_detail=detail_open;last_mode=page_mode(requested.page);
}
static void render_detail(bool first,uint32_t now){
 uint8_t page=requested.page;ui_service(now,first);
 bool label_change=first || mode_animating || ui_moving;
 if(label_change){dirty(40,63,386,40);dirty(45,316,376,61);dirty(185,383,96,24);}
 int16_t v[4],t[4];sample_visual(page,v,t);bool moving=first;
 for(int i=0;i<(page==2?1:4);++i)if(v[i]!=last_value[i] || t[i]!=last_tint[i])moving=true;
 int phase=page==1 && visual.vector_strength>.005f && (v[0]+v[1]+v[2]+v[3])>0?(int)(visual.pulse_phase*256):-1;
 int strength=lroundf(visual.vector_strength*100);
 if(page==1 && strength!=last_strength)moving=true;last_strength=strength;
 if(page==1 && phase!=last_phase)moving=true;last_phase=phase;
 bool body_change=first || fabsf(body_level-last_body_level)>.0001f;
 if(body_change){if(page==2)dirty(45,107,398,207);else dirty(143,107,181,207);}
 else if(moving || ui_moving){
  if(page==2)dirty(387,209,31,33);
  else {dirty(143,143,181,47);dirty(143,251,181,47);}
 }
 if(flow_phase>=0 || last_flow>=0)dirty(61,179,380,66);
 last_flow=flow_phase;last_body_level=body_level;
 restore_dirty();
 // All visual primitives are clipped to each damaged rectangle. This also
 // restores the body under disappearing effects without alpha accumulation.
 for(int n=0;n<dirty_count;++n){auto r=dirty_rects[n];if(r.y>=314 || r.y+r.h<=107)continue;
  art_clip=r;paint_car();}
 art_clip={0,0,LCD_W,LCD_H};
 if(label_change)labels(now);
 for(int i=0;i<4;++i){last_value[i]=v[i];last_tint[i]=t[i];}
 if((uint32_t)(now-mode_at)>=MODE_FADE_MS)mode_animating=false;
}
static void render_startup(uint32_t now){
 uint32_t elapsed=now-startup_started;
 dirty(50,130,368,155);fill_rect(g_frame,50,130,368,155,0);
 if(elapsed<470){float t=smooth(elapsed/470.f);
  art_dot(g_frame,110+123*t,233,3,WHITE);
  art_dot(g_frame,356-123*t,233,3,CYAN);
 }else if(elapsed<2050){
  // 300ms opening, 1000ms full hold, 280ms closing: fixed-size strips.
  float reveal=elapsed<770?(elapsed-470)/300.f:elapsed<1770?1.f:1-(elapsed-1770)/280.f;
  logo_unfold(reveal);
  art_dot(g_frame,233,233,3,CYAN,roundf(255*(1-smooth(reveal/.12f))));
 }else if(elapsed<2378){float t=smooth((elapsed-2050)/328.f);
  // Same travel speed as the former 45px / 220ms trajectory.
  art_dot(g_frame,233,233-67*t,3,CYAN);
 }else{
  float fall=smooth((elapsed-2378)/130.f);
  if(elapsed<2508)art_dot(g_frame,233,166+33*fall,3,CYAN);
  for(int i=0;i<6;++i){float delay=i<2?0:i==2?130:260;
   float t=smooth(((float)elapsed-2478-delay)/720.f);path_draw(g_frame,i,t,218,true);
  }
  if(elapsed>3200){float u=clamp_float((elapsed-3200)/460.f,0,1),q=u-1,t=1+1.6f*q*q*q+.6f*q*q;
   // Finish at the local selection, seamlessly handed to the live model.
   wing_draw(g_frame,5+(visual.aero-5)*t,255*smooth((elapsed-3200)/120.f));
  }
 }
 if(elapsed>=STARTUP_MS){startup_active=false;base_invalid=true;}
}
static void render_tick(){
 uint32_t us=micros();if((uint32_t)(us-last_render_us)<FRAME_US)return;last_render_us=us;
 uint32_t now=millis();dirty_count=0;
 if(startup_active){render_startup(now);push_dirty(us);return;}
 bool first=base_invalid || shown_page!=requested.page;
 if(first){build_base();base_invalid=false;
  dirty(40,63,405,370);
 }
 render_detail(first,now);shown_page=requested.page;push_dirty(us);
 if(dirty_count && render_stats.total_us>33333)++render_stats.over_budget;
}
