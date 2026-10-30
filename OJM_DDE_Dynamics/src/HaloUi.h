#pragma once
// Halo 2 UI (V6.0.6) — a 1:1 port of the approved interactive model (design/halo2-ui-2026-10-20).
// Visual page order: 0 Active Aero, 1 Rear Wheel Steering, 2 Torque Vectoring (CAN page IDs are unchanged).
// ui_update() advances the springs once per frame; ui_paint() is a pure function of that state and time,
// drawn rectangle by rectangle; ui_mark_damage() reports exactly the items whose look changed.
// V6.0.6.7: RWS shown as Off, Auto, Stable, Agile and TV as Off, Auto, Sport, Drift (CAN values unchanged). Auto
// shows what it picks from the car state (TorusContext.h); locked options are dimmed in the selector; a long press
// in the menu fills the halo (the quick action), Launch keeps it lit, Boost drains it over its 10 s.
// V6.0.6.8 action view: Launch and Boost look the same on every page - the side view (cross-faded in like a page
// change), three start lights for Launch (lit one by one, out at once on Go) and airflow along the body (one on Go,
// a stream during Boost that slows down as it runs out). A turn or the selector during Boost ends the view.
// V6.0.6.9: a locked option is a grey segment of full length; bouncing on it shows its name in grey with the
// reason below ("Track only", "Above 120 km/h") for 0.8 s.
static constexpr uint16_t UI_BLUE=0x2ADF,UI_GLOW=0x01DF,UI_GREY=0x7C11,UI_RING=0x18E4,UI_RING_EDIT=0x11D7,UI_PULSE=0x7CDF;
static constexpr uint16_t UI_MARK=0x2166,UI_MARK_EDIT=0x4A8B,UI_SEG=0x112B,UI_SEG_OFF=0x2146,UI_TITLE_EDIT=0x5B2E;
static constexpr uint16_t UI_WHEEL=0x31C8,UI_WHEEL_OFF=0x2966,UI_HEAD_GLOW=0x3B5F,UI_LOCKED=0x4208;  // locked: neutral grey
static const char *const UI_TITLES[3]={"Active Aero","Rear Wheel Steering","Torque Vectoring"};
static const char *const UI_MODES[3][4]={{"Auto","Low Drag","Downforce","Air Brake"},{"Off","Auto","Stable","Agile"},{"Off","Auto","Sport","Drift"}};
static constexpr bool UI_OFF[3][4]={{false,false,false,false},{true,false,false,false},{true,false,false,false}};
// Per position: Aero wing angle (Auto at rest 12 deg), RWS rear angle (Auto neutral 0), TV torque per wheel FL FR RL RR.
static constexpr float UI_WING[4]={12,0,25,58},UI_REAR[4]={0,0,9,-11};
static constexpr float UI_TQ[4][4]={{0,0,0,0},{0,0,.65f,.35f},{0,0,.85f,.2f},{.15f,.15f,1,.95f}};
static const char *const UI_LAUNCH="Launch Control",*const UI_READY="Ready",*const UI_GO="Go",*const UI_BOOST="Boost",*const UI_ACTIVE="Active";
static constexpr int32_t UI_NEVER=-1000000;
static constexpr float UI_AXLE_X[2]={163,303};
static uint8_t vis_of(uint8_t fw){return (fw+1)%3;}
static uint8_t fw_of(uint8_t vis){return (vis+2)%3;}

struct UiTransition {bool active=false;uint8_t from=0,to=0;int32_t at=UI_NEVER;int8_t dir=1;bool fast=false,same=false;uint8_t prev_pos=0;};
struct UiTvAnim {bool on=false;int32_t at=UI_NEVER;float from[4]={0,0,0,0},to[4]={0,0,0,0};int8_t kick=-1;};
struct UiState {
 bool init=false;int32_t last=0;uint8_t page=0,cand=0,applied[3]={0,0,0};bool edit=false;
 // V6.0.6.7 quick action: hold ring (UI clock), the last context event and when Boost started; Auto picks seen.
 int32_t holdAt=UI_NEVER,holdEndAt=UI_NEVER,actAt=UI_NEVER,boostAt=UI_NEVER;float holdEndA=0;bool holdOk=false;
 uint32_t actSeen=0;uint8_t actEvent=EVENT_NONE,seenAeroAuto=0,seenRwsAuto=1;
 // Action view: on/off with the time and value it started from (its blend is a pure function of time).
 bool axOn=false,axDismissed=false,axBoost=false;int32_t axAt=UI_NEVER;float axFrom=0;
 uint32_t lockSeen=0;int32_t lockAt=UI_NEVER;uint8_t lockPos=0;const char *lockReason=nullptr;
 float pagePos=0,pageVel=0,lead=0,leadVel=0,trail=0,trailVel=0,wing=25,wingVel=0,az=0,azVel=0;
 float rear=0,rearVel=0,prevRear=0,fb=0,fbVel=0,tq[4]={0,0,0,0},tqVel[4]={0,0,0,0},dim=.9f,exitK=1;
 int32_t introAt=UI_NEVER,editAt=UI_NEVER,exitAt=UI_NEVER,pulseAt=UI_NEVER,commitAt=UI_NEVER,edgeAt=UI_NEVER;
 int32_t zeroAt=UI_NEVER,flowAt=UI_NEVER,trailAt=UI_NEVER,mtAt=UI_NEVER,lastRotAt=UI_NEVER;
 int8_t edgeDir=1;uint8_t mtFrom=0;bool mtValid=false;
 uint32_t seenCommit=0,seenEdge=0;UiTransition tr;UiTvAnim tv;
};
static UiState ui;
// What the car shows: Auto shows the position it picks from the situation (no car data: the Auto rest look).
static uint8_t ui_wing_pos(uint8_t shown){return shown==0?torus.aero_auto:shown;}
static uint8_t ui_rws_pos(uint8_t shown){return shown==1?torus.rws_auto:shown;}
static float startup_wing_target(){return UI_WING[ui_wing_pos(mode_position(2,requested.aero))];}
// Pieces the Titan does not draw (reflection, Auto ghost) fade in with the UI entry instead of popping.
static constexpr float UI_TITAN_FADE_R0=192,UI_TITAN_FADE_R1=204;
static float ui_settle(int32_t now){return smooth((now-ui.introAt)/320.f);}
// V6.0.6.5: while the Aero zoom moves, the edit pulse (hairline swell and glow around the whole ring, ~48 k px of
// damage) steps at 30 Hz: a frame with zoom and ring takes ~18 ms on the board, one with the zoom alone ~10 ms, so
// with painting overlapped with the transfer every TE edge gets its frame and the zoom keeps a full 60 fps.
// Everywhere else the pulse runs at 60 Hz.
static bool ui_zooming(){return fabsf(ui.azVel)>.02f || (ui.az>.001f && ui.az<.999f);}
static float ui_pulse_phase(int32_t now){const int32_t e=now-ui.editAt;return (e>=0 && ui_zooming()?e/33*33:e)/340.f;}
static uint8_t ui_shown(uint8_t k){return ui.edit && k==ui.page?ui.cand:ui.applied[k];}
static float ui_ax(int32_t now){const float k=smooth((now-ui.axAt)/240.f);return ui.axOn?ui.axFrom+(1-ui.axFrom)*k:ui.axFrom*(1-k);}
// Hold ring: shows after 120 ms (a plain click never flashes it), full when the long press fires.
static float ui_hold_angle(int32_t now){return ui.holdAt==UI_NEVER?0:180*clamp_float((now-ui.holdAt-120)/(float)(BUTTON_LONG_MS-120),0,1);}

static void ui_spring(float &v,float &vel,float target,float w,float zeta,float dt){
 for(int i=0;i<4;++i){const float h=dt/4,a=w*w*(target-v)-2*zeta*w*vel;vel+=a*h;v+=vel*h;}
 // Settled: snap exactly, so a resting item stops repainting (no endless sub-pixel tail).
 if(fabsf(target-v)<.0005f && fabsf(vel)<.005f){v=target;vel=0;}
}
static void ui_begin_scene(int32_t now){
 ui.introAt=now;
 if(!ui.init){
  // The Titan ends on the Aero side view; another page (set over CAN meanwhile) follows as a normal transition.
  ui.init=true;ui.page=0;ui.pagePos=ui.page;
  for(uint8_t k=0;k<3;++k)ui.applied[k]=mode_position(fw_of(k),page_mode(fw_of(k)));
  ui.lead=ui.trail=ui.applied[ui.page];ui.wing=UI_WING[ui_wing_pos(ui.applied[0])];ui.rear=ui.prevRear=UI_REAR[ui_rws_pos(ui.applied[1])];
  for(int i=0;i<4;++i)ui.tq[i]=UI_TQ[ui.applied[2]][i];ui.fb=ui_rws_pos(ui.applied[1])==2;
  ui.seenAeroAuto=torus.aero_auto;ui.seenRwsAuto=torus.rws_auto;ui.actSeen=torus.event_seq;ui.lockSeen=nav_lock_seq;
  ui.seenCommit=nav_commit_seq;ui.seenEdge=nav_edge_seq;ui.last=now;
 }
}
static void ui_pour(int32_t now,uint8_t to_pos){
 ui.tv.on=true;ui.tv.at=now;float best=-1;ui.tv.kick=-1;
 for(int i=0;i<4;++i){ui.tv.from[i]=ui.tq[i];ui.tv.to[i]=UI_TQ[to_pos][i];if(ui.tv.to[i]>best){best=ui.tv.to[i];ui.tv.kick=i;}}
}
static void ui_update(int32_t now){
 const float dt=clamp_float((now-ui.last)*.001f,0,.033f);ui.last=now;
 const uint8_t vis=vis_of(requested.page);
 uint8_t applied[3];for(uint8_t k=0;k<3;++k)applied[k]=mode_position(fw_of(k),page_mode(fw_of(k)));
 if(vis!=ui.page){
  auto &tr=ui.tr;tr.active=true;tr.from=ui.page;tr.to=vis;tr.at=now;tr.dir=nav_page_dir;
  tr.fast=now-ui.lastRotAt<300;tr.same=tr.from!=0 && tr.to!=0;tr.prev_pos=ui.applied[tr.from];
  if(torus.action==ACTION_BOOST)ui.axDismissed=true;  // browsing during Boost leaves the action view
  ui.lastRotAt=now;ui.page=vis;ui.mtValid=false;ui.lead=ui.trail=applied[vis];ui.leadVel=ui.trailVel=0;ui.tv.on=false;
  if(vis==0)ui.flowAt=now+220;
 }
 // A mode changed outside the selector (remote): same name cross-fade and system cues as a local change.
 if(!ui.edit && applied[ui.page]!=ui.applied[ui.page]){
  ui.mtAt=now;ui.mtFrom=ui.applied[ui.page];ui.mtValid=true;
  if(ui.page==1)ui.trailAt=now;if(ui.page==2)ui_pour(now,applied[2]);if(ui.page==0)ui.flowAt=now+80;
 }
 for(uint8_t k=0;k<3;++k)ui.applied[k]=applied[k];
 if(detail_open && !ui.edit){ui.edit=true;ui.editAt=now;ui.cand=edit_candidate;ui.mtValid=false;}
 if(!detail_open && ui.edit){ui.exitK=clamp_float((8000-(int32_t)(now-nav_last_input))/1000.f,0,1);ui.edit=false;ui.exitAt=now;}
 if(ui.edit && edit_candidate!=ui.cand){
  ui.mtAt=now;ui.mtFrom=ui.cand;ui.mtValid=true;ui.cand=edit_candidate;ui.pulseAt=now;ui.lockAt=UI_NEVER;
  if(ui.page==1)ui.trailAt=now;
  if(ui.page==2)ui_pour(now,ui.cand);
 }
 if(nav_edge_seq!=ui.seenEdge){ui.seenEdge=nav_edge_seq;ui.edgeAt=now;ui.edgeDir=nav_edge_dir;}
 if(nav_lock_seq!=ui.lockSeen){ui.lockSeen=nav_lock_seq;
  if(vis_of(nav_lock_page)==ui.page){ui.lockAt=now;ui.lockPos=nav_lock_pos;ui.lockReason=torus_lock_reason(nav_lock_page,nav_lock_pos);}}
 if(!ui.edit || ui.lockAt>now)ui.lockAt=UI_NEVER;
 if(nav_commit_seq!=ui.seenCommit){ui.seenCommit=nav_commit_seq;ui.commitAt=now;if(ui.page==0)ui.flowAt=now+80;}
 // Auto picked another position (the car got faster, braked...): the same cues as a change by hand.
 if(torus.aero_auto!=ui.seenAeroAuto){ui.seenAeroAuto=torus.aero_auto;if(ui.page==0 && ui_shown(0)==0)ui.flowAt=now+80;}
 if(torus.rws_auto!=ui.seenRwsAuto){ui.seenRwsAuto=torus.rws_auto;if(ui.page==1 && ui_shown(1)==1)ui.trailAt=now;}
 // Quick action: the hold ring follows the debounced button in the menu (its start converted to the UI clock);
 // a context event (armed, launched, Boost...) takes over from it.
 const uint32_t ms=millis();
 const bool holding=g_button_down && !detail_open && torus.action!=ACTION_LAUNCH;
 if(holding && ui.holdAt==UI_NEVER){ui.holdAt=now-(int32_t)(ms-g_button_down_at);ui.holdOk=torus_available_action(ms)!=ACTION_NONE;}
 if(!holding && ui.holdAt!=UI_NEVER){ui.holdEndA=ui_hold_angle(now);ui.holdEndAt=now;ui.holdAt=UI_NEVER;}
 if(torus.event_seq!=ui.actSeen){ui.actSeen=torus.event_seq;ui.actEvent=torus.event;ui.actAt=now;ui.holdEndAt=UI_NEVER;
  if(torus.event==EVENT_BOOST){ui.boostAt=now;ui.axDismissed=false;}}
 // Action view: while Launch is armed, 900 ms after Go, and during Boost until a turn or the selector.
 if(ui.edit && torus.action==ACTION_BOOST)ui.axDismissed=true;
 const bool want=!ui.edit && (torus.action==ACTION_LAUNCH || (torus.action==ACTION_BOOST && !ui.axDismissed) || (ui.actEvent==EVENT_LAUNCH_GO && now-ui.actAt<900));
 if(want!=ui.axOn){ui.axFrom=ui_ax(now);ui.axOn=want;ui.axAt=now;}
 if(want)ui.axBoost=torus.action==ACTION_BOOST;  // (a Boost right after Go switches the texts)
 // Springs (4 substeps, as in the model).
 ui_spring(ui.pagePos,ui.pageVel,ui.page,20,1,dt);
 const float chosen=ui.edit?ui.cand:ui.applied[ui.page];
 const float edge=now-ui.edgeAt<180?ui.edgeDir*.32f*sinf(PI*(now-ui.edgeAt)/180.f):0;
 ui_spring(ui.lead,ui.leadVel,chosen+edge,40,1,dt);ui_spring(ui.trail,ui.trailVel,chosen,24,1,dt);
 ui_spring(ui.wing,ui.wingVel,UI_WING[ui_wing_pos(ui_shown(0))],20,.6f,dt);
 ui_spring(ui.az,ui.azVel,ui.edit && ui.page==0?1:0,13,1,dt);
 const uint8_t rws=ui_rws_pos(ui_shown(1));const float rear_target=UI_REAR[rws];
 ui_spring(ui.rear,ui.rearVel,rear_target,30,.62f,dt);
 if(ui.prevRear*ui.rear<0 && rear_target!=0)ui.zeroAt=now;
 ui.prevRear=ui.rear;
 ui_spring(ui.fb,ui.fbVel,ui.page==1 && rws==2?1:0,24,1,dt);
 for(int i=0;i<4;++i)ui_spring(ui.tq[i],ui.tqVel[i],UI_TQ[ui_shown(2)][i],30,.7f,dt);
 // Safety: the car is never dimmed while choosing; it gets a touch brighter.
 const float target=ui.edit?1.f:.9f,rate=dt/.16f*.56f;
 ui.dim=ui.dim<target?min(target,ui.dim+rate):max(target,ui.dim-rate);
}

// ---- what is visible this frame ----
enum UiPhase : uint8_t {UI_IDLE,UI_FADE,UI_SAME,UI_CROSS};
struct UiVis {UiPhase phase=UI_IDLE;uint8_t page=0,from=0;float mix=1,fade=1;};
static UiVis ui_visible(int32_t now){
 UiVis v;v.page=ui.page;const auto &tr=ui.tr;if(!tr.active)return v;
 const int32_t e=now-tr.at;v.page=tr.to;v.from=tr.from;
 if(tr.fast){if(e<110){v.phase=UI_FADE;v.fade=smooth(e/110.f);}return v;}
 if(tr.same){if(e<200){v.phase=UI_SAME;v.mix=smooth(e/200.f);}return v;}
 if(e<240){v.phase=UI_CROSS;v.mix=smooth(e/240.f);}
 return v;
}
struct UiXf {float s=1,ox=0,oy=0;ArtPoint map(float x,float y)const{return {x*s+ox,y*s+oy};}};
struct UiFrame {
 UiVis v;float body,sideA,topA,la[3],AZ,ZK;UiXf side;float topDx,ax;
};
static float ui_live(const UiVis &v,uint8_t k){
 if(v.phase==UI_SAME)return k==v.page?v.mix:k==v.from?1-v.mix:0;
 if(v.phase==UI_CROSS)return k==v.page?smooth((v.mix-.25f)/.75f):k==v.from?1-smooth(v.mix/.6f):0;
 if(v.page!=k)return 0;
 return v.phase==UI_FADE?v.fade:1;
}
static UiFrame ui_frame(int32_t now){
 UiFrame f;f.v=ui_visible(now);const auto &v=f.v;const bool cross=v.phase==UI_CROSS;
 f.body=ui.dim*(v.phase==UI_FADE?v.fade:1);
 f.sideA=cross?(v.page==0?v.mix:1-v.mix):v.page==0?1:0;
 f.topA=cross?1-f.sideA:(v.page==1 || v.page==2 || v.phase==UI_SAME)?1:0;
 for(uint8_t k=0;k<3;++k)f.la[k]=ui_live(v,k);
 // Cross-fade depth (incoming from 103 %, outgoing to 97 %) and the glide with the knob direction.
 const int8_t dir=ui.tr.active?ui.tr.dir:0;
 auto zoom=[&](bool incoming,float &k,float &dx){k=1;dx=0;
  if(cross){k=incoming?1.03f-.03f*v.mix:1-.03f*v.mix;dx=incoming?dir*16*(1-v.mix):-dir*16*v.mix;}
  else if(v.phase==UI_FADE)dx=dir*10*(1-v.fade);};
 // Aero focus: zoom 1 -> 2.75x around the rear third while choosing on Active Aero.
 f.AZ=clamp_float(ui.az,0,1);f.ZK=1+1.75f*f.AZ;
 const float FX=233+137*f.AZ,FY=233-3*f.AZ,PY=233-10*f.AZ;
 float k,dx;zoom(v.page==0,k,dx);
 const float fs=f.ZK,fox=233-FX*f.ZK,foy=PY-FY*f.ZK;
 f.side.s=k*fs;f.side.ox=233+dx+(fox-233)*k;f.side.oy=215+(foy-215)*k;
 zoom(v.page!=0,k,dx);f.topDx=dx;
 // Action view: the top view (and its wheels) fades out in the first half, then the side view (with its wing) in -
 // like the texts, one after the other, so a frame paints only one of them. On Aero the side view simply stays.
 f.ax=ui_ax(now);
 if(f.ax>0){const float out=smooth(f.ax*2),in=smooth(f.ax*2-1);
  f.topA*=1-out;f.la[1]*=1-out;f.la[2]*=1-out;f.sideA+=(1-f.sideA)*in;f.la[0]+=(1-f.la[0])*in;}
 return f;
}

// ---- drawing helpers ----
static void ui_glow_line(float x0,float y0,float x1,float y1,float radius,uint16_t color,float alpha,bool glow){
 if(alpha<=0.002f)return;
 if(glow)art_line(g_frame,x0,y0,x1,y1,radius+3,UI_GLOW,alpha8(.3f*alpha));
 art_line(g_frame,x0,y0,x1,y1,radius,color,alpha8(alpha));
}
static void ui_wheel(float x,float y,float deg,uint16_t color,float width,float alpha,bool glow,float from=0,float to=1){
 const float a=deg*HALO_DEG,sx=sinf(a)*11,sy=cosf(a)*11,x0=x-sx,y0=y+sy,dx=2*sx,dy=-2*sy;
 ui_glow_line(x0+dx*from,y0+dy*from,x0+dx*to,y0+dy*to,width/2,color,alpha,glow);
}
// A model box (optionally mirrored at y=270) mapped to the screen, padded: does the rectangle need it?
static bool ui_chunk_hits(const HaloBox &b,const UiXf &xf,bool mirror,float pad){
 const float y0=mirror?540-b.y1:b.y0,y1=mirror?540-b.y0:b.y1;
 return clip_hits(b.x0*xf.s+xf.ox-pad,y0*xf.s+xf.oy-pad,b.x1*xf.s+xf.ox+pad,y1*xf.s+xf.oy+pad);
}
static void HALO_HOT ui_path_xf(int index,const UiXf &xf,float radius,uint16_t color,uint8_t alpha){
 if(!alpha)return;const auto &p=ART_PATHS[index];const HaloBox *boxes=art_chunks(index);
 for(int c=0;c*ART_CHUNK+1<p.count;++c){
  if(!ui_chunk_hits(boxes[c],xf,false,radius+2))continue;
  for(int i=c*ART_CHUNK+1;i<=min((int)p.count-1,c*ART_CHUNK+ART_CHUNK);++i){
   auto a=xf.map(p.points[i-1].x,p.points[i-1].y),b=xf.map(p.points[i].x,p.points[i].y);art_line(g_frame,a.x,a.y,b.x,b.y,radius,color,alpha);}
 }
}
// Floor reflection of the side view: mirrored at y=270, only the band 270..304 (model space), fading out.
static void HALO_HOT ui_reflection(const UiXf &xf,float alpha){
 if(alpha<=0.002f)return;
 const float y_top=270*xf.s+xf.oy,y_bottom=304*xf.s+xf.oy;
 // Bands of 4 rows with one alpha each (the fade is .13 at most: a band step is below 1/255). The band grid
 // starts at the first reflected row on screen, independent of the rectangle being painted.
 int first=(int)floorf(y_top)-1,end=(int)ceilf(y_bottom)+1;
 while(first<end && ((first+.5f)-xf.oy)/xf.s<270)++first;
 while(end>first && ((end-.5f)-xf.oy)/xf.s>=304)--end;
 auto saved=art_clip;
 for(int row=first;row<end;row+=4){
  const int rows=min(4,end-row);
  const int top=max(row,(int)saved.y),bottom=min(row+rows,(int)(saved.y+saved.h));if(bottom<=top)continue;
  const float ym=((row+rows*.5f)-xf.oy)/xf.s;const uint8_t a=alpha8(alpha*(1-(ym-270)/34.f));if(!a)continue;
  art_clip={saved.x,(int16_t)top,saved.w,(int16_t)(bottom-top)};
  for(int i=0;i<6;++i){const auto &p=ART_PATHS[i];const HaloBox *boxes=art_chunks(i);
   for(int c=0;c*ART_CHUNK+1<p.count;++c){
    if(!ui_chunk_hits(boxes[c],xf,true,3))continue;
    for(int n=c*ART_CHUNK+1;n<=min((int)p.count-1,c*ART_CHUNK+ART_CHUNK);++n){
     auto pa=xf.map(p.points[n-1].x,540-p.points[n-1].y),pb=xf.map(p.points[n].x,540-p.points[n].y);
     if(max(pa.y,pb.y)<top-2 || min(pa.y,pb.y)>bottom+2)continue;art_line(g_frame,pa.x,pa.y,pb.x,pb.y,.7f,WHITE,a);}}}
 }
 art_clip=saved;
}
static float ui_level(int32_t now,int i,float &kick_w){
 kick_w=0;const auto &tv=ui.tv;const int32_t te=tv.on && ui.page==2?now-tv.at:1000000;
 static constexpr float delay[4]={60,160,0,110};
 if(te>700)return clamp_float(ui.tq[i],0,1);
 if(te<90)return tv.from[i]*(1-smooth(te/90.f));
 const float e=(te-90-delay[i])/240.f;if(e<=0)return 0;
 const float x=min(1.f,e),rise=1-powf(1-x,3),hit=i==tv.kick && tv.to[i]>0?sinf(PI*min(1.f,e/1.3f)):0;
 kick_w=hit;return tv.to[i]*rise+.14f*hit;
}

// ---- text state (shared by paint and damage): the new name is readable within ~120 ms ----
struct UiTexts {float tA=1,tB=0,nA=1,nB=0,tLS=0,nLS=0,gA=0,gB=0;int8_t title_b=-1;const char *name_b=nullptr;};
static UiTexts ui_texts(int32_t now){
 UiTexts t;const uint8_t page=ui.page;
 const float entry=(now-ui.introAt)/320.f;
 if(entry>=0 && entry<1){const float k=smooth(entry);t.tA=t.nA=k;t.tLS=4*(1-k);t.nLS=7*(1-k);}
 if(ui.tr.active && now-ui.tr.at<140){const float e=now-ui.tr.at,k=smooth(e/120);
  t.title_b=ui.tr.from;t.name_b=UI_MODES[ui.tr.from][ui.tr.prev_pos];t.tB=t.nB=1-smooth(e/80);t.tA=t.nA=k;t.tLS=3*(1-k);t.nLS=5*(1-k);
  t.gA=ui.tr.dir*18*(1-k);t.gB=-ui.tr.dir*18*smooth(e/80);}
 else if(ui.mtValid && now-ui.mtAt<110){const float e=now-ui.mtAt,k=smooth(e/100);t.name_b=UI_MODES[page][ui.mtFrom];t.nB=1-smooth(e/70);t.nA=k;t.nLS=4*(1-k);}
 return t;
}
// Box of a centred text line (+ the outgoing one), wide enough for its tracking and the 18 px glide.
// Texts are constant strings: the measured width is remembered (every damaged rectangle asks for it).
static float ui_text_width(const SDFont &font,const char *a,int8_t track,const char *b){
 struct Memo {const SDFont *font;const char *a,*b;int8_t track;float w;};
 static Memo memos[2][8];static uint8_t nexts[2]={0,0};auto &memo=memos[halo_core()];uint8_t &next=nexts[halo_core()];
 for(const auto &m:memo)if(m.font==&font && m.a==a && m.b==b && m.track==track)return m.w;
 float w=measure_text(font,a,track);if(b)w=max(w,(float)measure_text(font,b,0));
 memo[next]={&font,a,b,track,w};next=(next+1)%8;return w;
}
static HaloBox ui_text_box(const SDFont &font,const char *a,int8_t track,const char *b,float top,float bottom){
 const float w=ui_text_width(font,a,track,b);
 HaloBox box;box.add(233-w/2-24,top);box.add(233+w/2+24,bottom);return box;
}
static constexpr float UI_TITLE_TOP=74,UI_TITLE_BOTTOM=108,UI_NAME_TOP=322,UI_NAME_BOTTOM=378;
// Item boxes in screen space (also used to skip items that a damaged rectangle does not touch).
static HaloBox ui_side_box(const UiFrame &f){HaloBox b;auto p=f.side.map(44,176),q=f.side.map(446,306);b.add(p.x,p.y);b.add(q.x,q.y);return b;}
static HaloBox ui_top_box(const UiFrame &f){HaloBox b;b.add(140+f.topDx,108);b.add(326+f.topDx,338);return b;}
static bool clip_hits(const HaloBox &b){return clip_hits(b.x0,b.y0,b.x1,b.y1);}
// Wheel columns of the top view (wheels, tracks, trails, torque fills) — damaged apart from the body.
// Wheel boxes of the top view: front/rear x left/right (the rows between the front tracks and the rear wheels never change).
static HaloBox ui_wheel_box(const UiFrame &f,int side,bool rear){HaloBox b;const float x=side?276:140;
 b.add(x+f.topDx,rear?254:112);b.add(x+50+f.topDx,rear?338:210);return b;}
// Box of an arc at radius r between two angles (degrees), padded.
static HaloBox ui_arc_box(float r,float a0,float a1,float pad){
 HaloBox b;for(float a=a0;;a+=3){const float t=min(a,a1)*HALO_DEG;b.add(233+r*cosf(t),233+r*sinf(t),pad);if(a>=a1)break;}return b;}
// Aero wing angle as drawn (the swing-in during the page cross-fade included).
static float ui_wing_deg(const UiFrame &f){
 const float swing=f.v.phase==UI_CROSS?clamp_float((f.v.mix-.3f)/.7f,0,1):1,q=swing-1;return ui.wing*(1+1.6f*q*q*q+.6f*q*q);
}
static float ui_ghost(int32_t now,const UiFrame &f){return ui_shown(0)==0?.35f*f.la[0]*(1-f.AZ)*ui_settle(now):0;}
static HaloBox ui_wing_box(const UiFrame &f){
 HaloBox b;auto p=f.side.map(386,208),q=f.side.map(416,240);b.add(p.x,p.y,3);b.add(q.x,q.y,3);return b;
}
// Airflow after an Aero change: the visible piece of the flow line (model space pieces, mapped).
struct UiFlow {uint8_t n=0;ArtPoint a[9],b[9];float alpha=0;};
static UiFlow ui_flow_piece(const UiFrame &f,float fp,float alpha);
static UiFlow ui_flow(int32_t now,const UiFrame &f){
 if(f.la[0]<=0 || f.v.phase!=UI_IDLE)return UiFlow{};
 return ui_flow_piece(f,(now-ui.flowAt)/900.f,1);
}
// Action view airflow, two slots (a stream can overlap): one quick line on Go; during Boost a new line every
// 550 ms at the start up to 1250 ms at the end, each slower (900 -> 1500 ms) as the Boost runs out.
static UiFlow ui_action_flow(int32_t now,const UiFrame &f,int slot){
 if(f.ax<=0 || f.sideA<=0)return UiFlow{};
 if(!ui.axBoost){if(slot || ui.actEvent!=EVENT_LAUNCH_GO)return UiFlow{};return ui_flow_piece(f,(now-ui.actAt)/700.f,f.ax);}
 const int32_t b=now-ui.boostAt;if(b<0 || b>12000)return UiFlow{};
 float start=0;
 for(int k=0;start<=b && k<32;++k){
  const float u=start/TUNING.boost_ms,duration=900+600*u,next=start+550+700*u;
  if((k&1)==slot && b<start+duration)return ui_flow_piece(f,(b-start)/duration,f.ax);
  start=next;
 }
 return UiFlow{};
}
static UiFlow ui_flow_piece(const UiFrame &f,float fp,float alpha){
 UiFlow out;if(fp<0 || fp>=1 || alpha<=0)return out;
 const float bend=clamp_float(ui.wing/58,0,1);
 const ArtPoint pts[10]={{66,240},{110,218},{154,201},{195,189},{233,185},{278,188},{325,199},{369,214},{407,219},{435,219+22*bend}};
 const float head=fp*9,tail=max(0.f,head-1.3f);out.alpha=alpha*.7f*smooth(fp/.15f)*(1-smooth((fp-.78f)/.22f));
 for(int i=0;i<9;++i){const float lo=max(tail,(float)i),hi=min(head,(float)i+1);if(hi<=lo)continue;
  const auto A0=pts[i],B0=pts[i+1];const float l=lo-i,h=hi-i;
  out.a[out.n]=f.side.map(A0.x+(B0.x-A0.x)*l,A0.y+(B0.y-A0.y)*l);out.b[out.n]=f.side.map(A0.x+(B0.x-A0.x)*h,A0.y+(B0.y-A0.y)*h);++out.n;}
 return out;
}

// ---- quick action (V6.0.6.7): an arc of the halo growing from the top both ways, and its texts ----
struct UiArc {float a=0,alpha=0,half=1.3f;uint16_t color=0;bool heads=false;};
static UiArc ui_action_arc(int32_t now){
 UiArc r;const int32_t e=now-ui.actAt;r.color=UI_BLUE;
 if(torus.action==ACTION_LAUNCH){r.a=180;r.alpha=1;r.heads=true;return r;}  // the full hold ring stays: no repaint
 // Boost: the halo stays full while the view changes (300 ms), then counts down the rest of the 10 s.
 if(torus.action==ACTION_BOOST){r.a=180*clamp_float(1-(now-ui.boostAt-300)/(float)(TUNING.boost_ms-300),0,1);r.alpha=1;r.heads=true;return r;}
 if(ui.holdAt!=UI_NEVER){r.a=ui_hold_angle(now);r.alpha=r.a>0?1:0;r.color=ui.holdOk?UI_BLUE:UI_GREY;r.heads=true;return r;}
 if(ui.holdEndAt!=UI_NEVER && now-ui.holdEndAt<140){r.a=ui.holdEndA;r.alpha=1-smooth((now-ui.holdEndAt)/140.f);r.color=ui.holdOk?UI_BLUE:UI_GREY;return r;}
 if(ui.actEvent==EVENT_LAUNCH_GO && e<450){const float k=smooth(e/450.f);r.a=180;r.alpha=1-k;r.half=1.3f+2.5f*k;r.color=UI_PULSE;return r;}
 if(ui.actEvent==EVENT_LAUNCH_CANCEL && e<200){r.a=180;r.alpha=1-smooth(e/200.f);return r;}
 if(ui.actEvent==EVENT_REFUSED && e<260){r.a=180;r.alpha=1-smooth(e/260.f);r.color=UI_GREY;return r;}
 return r;
}
static bool ui_arc_on(const UiArc &r){return r.a>0 && r.alpha>.002f;}
static void ui_paint_arc(const UiArc &r){
 if(!ui_arc_on(r))return;
 if(r.color==UI_BLUE)art_arc(g_frame,233,233,226,270-r.a,270+r.a,r.half+3,UI_GLOW,alpha8(.3f*r.alpha));
 art_arc(g_frame,233,233,226,270-r.a,270+r.a,r.half,r.color,alpha8(r.alpha));
 if(r.heads && r.a<180)for(float ang:(const float[2]){270-r.a,270+r.a}){const float x=233+226*cosf(ang*HALO_DEG),y=233+226*sinf(ang*HALO_DEG);
  art_dot(g_frame,x,y,6,UI_HEAD_GLOW,alpha8(.16f*r.alpha));art_dot(g_frame,x,y,2.5f,0xFFFF,alpha8(r.alpha));}
}
// Launch armed: "Launch Control / Ready" replaces the page texts until launched (then "Go") or cancelled; Boost
// shows "Boost / Active" for its first 1.4 s, the halo keeps counting down.
struct UiOverlay {const char *title=nullptr,*name=nullptr;float k=0;};
static UiOverlay ui_overlay(int32_t now){
 UiOverlay o;o.k=ui_ax(now);if(o.k<=0){o.k=0;return o;}
 if(ui.axBoost){o.title=UI_BOOST;o.name=UI_ACTIVE;}
 else{o.title=UI_LAUNCH;o.name=ui.actEvent==EVENT_LAUNCH_GO?UI_GO:UI_READY;}
 return o;
}
// Start lights (Launch): three dots above the car, lit one by one once armed, out at once on Go.
static constexpr float UI_LIGHT_Y=142,UI_LIGHT_DX=22;
// Lock hint: 0..1 over its 0.8 s (in 80 ms, out over the last 150 ms).
static constexpr float UI_HINT_TOP=380,UI_HINT_BOTTOM=408;
static float ui_lock_k(int32_t now){
 if(ui.lockAt==UI_NEVER)return 0;const int32_t e=now-ui.lockAt;if(e<0 || e>=800)return 0;
 return smooth(e/80.f)*(1-smooth((e-650)/150.f));
}
static float ui_lights_alpha(int32_t now){return ui.axBoost?0:ui_ax(now);}
static float ui_light(int32_t now,int i){return torus.action==ACTION_LAUNCH?smooth((now-ui.actAt-220-160*i)/70.f):0;}
static HaloBox ui_title_box(const UiTexts &t,const UiOverlay &o){
 HaloBox b=ui_text_box(WEB_LABEL,UI_TITLES[ui.page],4,t.title_b>=0?UI_TITLES[t.title_b]:nullptr,UI_TITLE_TOP,UI_TITLE_BOTTOM);
 if(o.k>0)b.add(ui_text_box(WEB_LABEL,o.title,0,nullptr,UI_TITLE_TOP,UI_TITLE_BOTTOM));return b;
}
static HaloBox ui_name_box(const UiTexts &t,const UiOverlay &o,float lock){
 HaloBox b=ui_text_box(WEB_MODE,UI_MODES[ui.page][ui_shown(ui.page)],7,t.name_b,UI_NAME_TOP,UI_NAME_BOTTOM);
 if(o.k>0)b.add(ui_text_box(WEB_MODE,o.name,0,nullptr,UI_NAME_TOP,UI_NAME_BOTTOM));
 if(lock>0)b.add(ui_text_box(WEB_MODE,UI_MODES[ui.page][ui.lockPos],0,nullptr,UI_NAME_TOP,UI_NAME_BOTTOM));return b;
}

// ---- the scene ----
// Development profiler: CPU cycles per part of the scene, summed over all rectangles of a frame (Serial "prof:").
static int prof_core=1;  // the loop core (the paint task's share is not timed)
enum {PROF_SIDE,PROF_REFL,PROF_TOP,PROF_WING,PROF_WHEEL,PROF_RING,PROF_SEG,PROF_TEXT,PROF_PARTS};
static uint32_t prof_cycles[PROF_PARTS],prof_last=0,prof_steps[PROF_PARTS],prof_work_last=0,prof_calls[PROF_PARTS],prof_call_last=0;
#define art_line_calls art_line_calls_total
static inline uint32_t prof_now(){
#if defined(ESP_PLATFORM)
 return esp_cpu_get_cycle_count();
#else
 return 0;
#endif
}
static inline void prof_mark(int part){if(halo_core()!=prof_core)return;const uint32_t n=prof_now();prof_cycles[part]+=n-prof_last;prof_last=n;
#ifdef HALO_WORK_COUNTER
 prof_steps[part]+=halo_work-prof_work_last;prof_work_last=halo_work;prof_calls[part]+=art_line_calls-prof_call_last;prof_call_last=art_line_calls;
#endif
}
static void ui_paint(int32_t now){
 if(halo_core()==prof_core)prof_last=prof_now();
#ifdef HALO_WORK_COUNTER
 prof_work_last=halo_work;prof_call_last=art_line_calls;
#endif
 ui_glass();
 const auto f=ui_frame(now);const auto &v=f.v;const float body=f.body;
 const bool side_hit=clip_hits(ui_side_box(f)),top_hit=clip_hits(ui_top_box(f));
 // Car, side view (Aero) with its floor reflection. Zoomed (Aero selection), its lines fade out between r 192 and
 // 204 so they never cross the ring and the segments. The band moves in from outside the screen with the zoom
 // (unzoomed the car reaches r ~196 and is drawn exactly as before, no step when the zoom starts).
 if(f.AZ>.001f){const float in=48*(1-smooth(f.AZ*3));art_fade_set(UI_TITAN_FADE_R0+in,UI_TITAN_FADE_R1+in);}
 if(f.sideA>0 && side_hit){
  for(int i=0;i<6;++i)ui_path_xf(i,f.side,.85f,WHITE,alpha8(body*f.sideA));
  prof_mark(PROF_SIDE);
  ui_reflection(f.side,.13f*body*f.sideA*ui_settle(now));
 }
 prof_mark(PROF_REFL);
 if(f.topA>0 && top_hit)outline(g_frame,ART_TOP,(int16_t)lroundf(233+f.topDx),213,WHITE,alpha8(body*f.topA));
 prof_mark(PROF_TOP);
 // Active Aero: the wing is part of the outline (closed = the missing piece of the deck line), lifts around its
 // leading edge; airflow once after a change.
 const float la0=f.la[0];
 if(la0>0 && clip_hits(ui_wing_box(f))){
  const UiXf &xf=f.side;const float ghost=ui_ghost(now,f);
  if(ghost>0){
   auto p0=xf.map(392,232),p1=xf.map(411,232),p2=xf.map(392+19*cosf(25*HALO_DEG),232-19*sinf(25*HALO_DEG));
   ui_glow_line(p0.x,p0.y,p1.x,p1.y,.7f,UI_BLUE,ghost,false);ui_glow_line(p0.x,p0.y,p2.x,p2.y,.7f,UI_BLUE,ghost,false);
  }
  halo_wing(xf.s,xf.ox,xf.oy,ui_wing_deg(f),la0,f.AZ);
 }
 const auto flow=ui_flow(now,f);
 for(int i=0;i<flow.n;++i)ui_glow_line(flow.a[i].x,flow.a[i].y,flow.b[i].x,flow.b[i].y,.65f,UI_BLUE,flow.alpha,false);
 for(int slot=0;slot<2;++slot){const auto af=ui_action_flow(now,f,slot);
  for(int i=0;i<af.n;++i)ui_glow_line(af.a[i].x,af.a[i].y,af.b[i].x,af.b[i].y,.65f,UI_BLUE,af.alpha,false);}
 art_fade_off();
 const float lights=ui_lights_alpha(now);
 if(lights>0)for(int i=0;i<3;++i){const float x=233+(i-1)*UI_LIGHT_DX,l=ui_light(now,i);
  if(l>0)art_dot(g_frame,x,UI_LIGHT_Y,7,UI_GLOW,alpha8(.35f*l*lights));
  art_dot(g_frame,x,UI_LIGHT_Y,3,mix565(UI_SEG_OFF,WHITE,l),alpha8(lights));}
 prof_mark(PROF_WING);
 // Top view: RWS wheels (phase colour, tracks, live trail, zero flash) and TV torque levels (pour + kick).
 const float la1=f.la[1],la2=f.la[2],ox=f.topDx;
 if(la1>0 && top_hit){
  const uint8_t mode=ui_shown(1);const bool off=UI_OFF[1][mode];const uint16_t color=off?UI_GREY:UI_BLUE;
  const float kick=ui.page==1 && ui.mtValid?max(0.f,1-(now-ui.mtAt)/240.f):0,fb=clamp_float(ui.fb,0,1),flash=max(0.f,1-(now-ui.zeroAt)/140.f);
  const uint16_t front=mix565(WHITE,UI_BLUE,fb);
  ui_wheel(163+ox,166,13,front,5.6f,la1,fb>.5f);ui_wheel(303+ox,166,13,front,5.6f,la1,fb>.5f);
  auto track=[&](float x,float y,float deg,uint16_t c,float alpha){const float a=deg*HALO_DEG,hx=sinf(a),hy=-cosf(a),x0=x-hx*11,y0=y-hy*11;
   art_line_fade(g_frame,x0,y0,x0-hx*26,y0-hy*26,2.2f,c,alpha,0);};
  for(float x:UI_AXLE_X){track(x+ox,166,13,front,.22f*la1);track(x+ox,274,ui.rear,color,.32f*la1);}
  const float tk=(now-ui.trailAt)/700.f;
  if(ui.page==1 && tk>=0 && tk<1 && !off)for(float x:UI_AXLE_X){
   const float grow=smooth(tk/.32f),fade=1-smooth((tk-.35f)/.65f),a0=ui.rear*HALO_DEG,step=46*grow/14;
   float px=x+ox-sinf(a0)*11,py=274+cosf(a0)*11;
   for(int i=0;i<14;++i){const float ang=a0*(1+.9f*i/14),nx=px-sinf(ang)*step,ny=py+cosf(ang)*step;
    art_line(g_frame,px,py,nx,ny,1.6f*(1-.5f*i/14),UI_BLUE,alpha8(la1*fade*.7f*(1-i/14.f)));px=nx;py=ny;}
  }
  for(float x:UI_AXLE_X){
   ui_wheel(x+ox,274,ui.rear,color,5.6f+1.4f*kick,la1,!off);
   if(flash>0)ui_wheel(x+ox,274,ui.rear,0xFFFF,5.6f+2*flash,flash*la1,true);
  }
 }
 if(la2>0 && top_hit){
  const bool off=UI_OFF[2][ui_shown(2)];
  const float X[4]={163,303,163,303},Y[4]={166,166,274,274},D[4]={13,13,0,0};
  for(int i=0;i<4;++i){
   ui_wheel(X[i]+ox,Y[i],D[i],off?UI_WHEEL_OFF:UI_WHEEL,5.6f,la2,false);
   float kw;const float lv=ui_level(now,i,kw);
   if(!off && lv>.04f)ui_wheel(X[i]+ox,Y[i],D[i],UI_BLUE,5.6f+1.6f*kw,la2,true,0,lv);
  }
 }
 prof_mark(PROF_WHEEL);
 // Halo ring: grey hairline = navigation, blue = choosing a mode.
 const float idle_left=8000-(int32_t)(now-nav_last_input);
 uint16_t ring=ui.edit?mix565(UI_RING,UI_RING_EDIT,clamp_float(idle_left/1000,0,1)):UI_RING;
 if(!ui.edit && now-ui.exitAt<200)ring=mix565(UI_RING,UI_RING_EDIT,ui.exitK*(1-smooth((now-ui.exitAt)/200.f)));
 const float ip=(now-ui.introAt)/520.f;bool act_in_ring=false;
 if(ip>=0 && ip<1){
  const float k=smooth(ip),a=180*k;art_arc(g_frame,233,233,226,270-a,270+a,.6f,ring,255);
  if(k>0 && k<1)for(float ang:(const float[2]){270-a,270+a}){const float x=233+226*cosf(ang*HALO_DEG),y=233+226*sinf(ang*HALO_DEG);
   art_dot(g_frame,x,y,6,UI_HEAD_GLOW,40);art_dot(g_frame,x,y,2.5f,0xFFFF,255);}
 }else{
  const float iq=ui_pulse_phase(now);
  // A full quick-action ring goes into the same one-pass table as the edit pulse (glow and ring over the hairline).
  const UiArc act=ui_action_arc(now);
  if(!(ui.edit && iq>=0 && iq<1) && ui_arc_on(act) && act.a>=180)
   act_in_ring=art_ring_stack(226,.6f,ring,255,226,act.half+3,UI_GLOW,act.color==UI_BLUE?alpha8(.3f*act.alpha):0,act.half,act.color,alpha8(act.alpha));
  if(act_in_ring){}
  else if(ui.edit && iq>=0 && iq<1){
   // The edit pulse: hairline, glow and pulse together (one lookup per black pixel).
   const float rise=smooth(iq/.18f),decay=powf(1-iq,2.2f),r=226+2.5f*decay,half=(1.2f+2.6f*decay)/2;
   if(!art_ring_stack(226,.6f,ring,255,r,half+3,UI_GLOW,alpha8(.25f*rise*decay),half,UI_PULSE,alpha8(rise*decay))){
    art_arc(g_frame,233,233,226,0,360,.6f,ring,255);
    art_ring2(233,233,r,half+3,UI_GLOW,alpha8(.25f*rise*decay),half,UI_PULSE,alpha8(rise*decay));
   }
  }else art_arc(g_frame,233,233,226,0,360,.6f,ring,255);
 }
 const float introK=ip<0?0:smooth(ip);
 const float iq=ui_pulse_phase(now);
 if(ui.edit && iq>=0 && iq<1 && ip>=0 && ip<1){
  // (pulse during the ring intro: drawn over the intro arc as two passes)
  const float rise=smooth(iq/.18f),decay=powf(1-iq,2.2f),r=226+2.5f*decay,half=(1.2f+2.6f*decay)/2;
  art_ring2(233,233,r,half+3,UI_GLOW,alpha8(.25f*rise*decay),half,UI_PULSE,alpha8(rise*decay));
 }
 if(!act_in_ring)ui_paint_arc(ui_action_arc(now));
 auto pageA=[](float i){return 270+(i-1)*11;};
 for(int i=0;i<3;++i)art_arc(g_frame,233,233,214,pageA(i)-3.5f,pageA(i)+3.5f,1.5f,UI_MARK,alpha8(introK));
 art_arc(g_frame,233,233,214,pageA(ui.pagePos)-3.5f,pageA(ui.pagePos)+3.5f,1.5f,ui.edit?UI_MARK_EDIT:WHITE,alpha8((ui.edit?.45f:1)*introK));
 prof_mark(PROF_RING);
 // Mode segments: a short tick in the menu; a click grows it into a segment and the others open out.
 auto segA=[](float i){return 90+(1.5f-i)*19;};
 const uint8_t page=ui.page,chosen=ui.edit?ui.cand:ui.applied[page];
 const float open=ui.edit?smooth((now-ui.editAt)/110.f):1-smooth((now-ui.exitAt)/110.f);
 for(int i=0;i<4;++i){if(i==chosen)continue;const int d=abs(i-(int)chosen);
  const float k=ui.edit?smooth((now-ui.editAt-22*(d-1))/90.f):1-smooth((now-ui.exitAt-18*(3-d))/80.f);
  // A locked option (the situation does not allow it) stays a thin dim segment.
  const bool locked=torus_locked(fw_of(page),i);
  if(k>0)art_arc(g_frame,233,233,214,segA(i)-8*k,segA(i)+8*k,locked?1.3f:2.5f,locked?UI_LOCKED:UI_OFF[page][i]?UI_SEG_OFF:UI_SEG,alpha8(k));}
 const float menuOp=v.phase==UI_CROSS?0:1,barHalf=4+4*open,barW=3+2*open,barOp=max(open,.9f*menuOp*introK)*(1-f.ax);
 const float lo=min(segA(ui.lead),segA(ui.trail))-barHalf,hi=max(segA(ui.lead),segA(ui.trail))+barHalf;
 const float pulse=clamp_float(1-(now-ui.pulseAt)/90.f,0,1),settle=now-ui.commitAt<180?sinf(PI*(now-ui.commitAt)/180.f):0;
 const bool chosen_off=UI_OFF[page][chosen];
 if(!chosen_off && open>.05f)art_arc(g_frame,233,233,214,lo,hi,(barW+6)/2,UI_GLOW,alpha8((.35f+.35f*pulse+.3f*settle)*open*(1-f.ax)));
 art_arc(g_frame,233,233,214,lo,hi,(barW+1.2f*pulse)/2,chosen_off?UI_GREY:UI_BLUE,alpha8(barOp));
 prof_mark(PROF_SEG);
 // Text.
 const auto t=ui_texts(now);const uint16_t title_color=ui.edit?UI_TITLE_EDIT:WHITE;
 const char *title=UI_TITLES[page],*name=UI_MODES[page][ui_shown(page)];
 // The quick action's texts replace the page texts one after the other (never both half visible).
 const auto ov=ui_overlay(now);const float keep=1-smooth(ov.k*2),ovA=smooth(ov.k*2-1);
 if(clip_hits(ui_title_box(t,ov))){
  halo_text(g_frame,WEB_LABEL,title,233+t.gA+t.tLS/2,100,title_color,t.tA*keep,(int8_t)lroundf(t.tLS));
  if(t.title_b>=0)halo_text(g_frame,WEB_LABEL,UI_TITLES[t.title_b],233+t.gB,100,title_color,t.tB*keep);
  if(ovA>0)halo_text(g_frame,WEB_LABEL,ov.title,233,100,WHITE,ovA);
 }
 // Lock hint: the locked option's name (grey) replaces the mode name for a moment, the reason below it.
 const float lk=ui_lock_k(now),lkeep=1-smooth(lk*2),lkA=smooth(lk*2-1);
 if(clip_hits(ui_name_box(t,ov,lk))){
  halo_text(g_frame,WEB_MODE,name,233+t.gA+t.nLS/2,364,WHITE,t.nA*keep*lkeep,(int8_t)lroundf(t.nLS));
  if(t.name_b)halo_text(g_frame,WEB_MODE,t.name_b,233+t.gB,364,WHITE,t.nB*keep*lkeep);
  if(ovA>0)halo_text(g_frame,WEB_MODE,ov.name,233,364,WHITE,ovA);
  if(lkA>0)halo_text(g_frame,WEB_MODE,UI_MODES[page][ui.lockPos],233,364,UI_GREY,lkA);
 }
 if(lkA>0 && clip_hits(ui_text_box(WEB_LABEL,ui.lockReason,0,nullptr,UI_HINT_TOP,UI_HINT_BOTTOM)))
  halo_text(g_frame,WEB_LABEL,ui.lockReason,233,400,UI_GREY,lkA);
 prof_mark(PROF_TEXT);
}

// ---- damage ----
static DamageSlot ui_slots[19];
// The car is damaged along its lines: one box per 16-segment chunk of each side path (and of its reflection),
// one per 16-row band of the top view — a cross-fade or the Aero zoom no longer repaints the whole car box.
static constexpr int TOP_BANDS=(198+15)/16;
static DamageSlot ui_side_slots[6][ART_CHUNKS],ui_refl_slots[6][ART_CHUNKS],ui_top_slots[TOP_BANDS];
static int16_t top_band_x[TOP_BANDS][2];static bool top_bands_ready=false;
static UiArc ui_arc_prev;static bool ui_arc_prev_on=false,ui_arc_prev_ring=false;
static void ui_reset_damage(){
 ui_arc_prev=UiArc{};ui_arc_prev_on=ui_arc_prev_ring=false;
 for(auto &s:ui_slots)s=DamageSlot{};
 for(auto &p:ui_side_slots)for(auto &s:p)s=DamageSlot{};
 for(auto &p:ui_refl_slots)for(auto &s:p)s=DamageSlot{};
 for(auto &s:ui_top_slots)s=DamageSlot{};
}
static void ui_top_bands(){
 if(top_bands_ready)return;
 for(auto &b:top_band_x){b[0]=ART_TOP.width;b[1]=0;}
 for(uint16_t n=0;n<ART_TOP.count;++n){AssetSpan span;memcpy_P(&span,ART_TOP.spans+n,sizeof(span));
  const int row=span.offset/ART_TOP.width,x=span.offset%ART_TOP.width;auto &b=top_band_x[row/16];
  b[0]=min((int)b[0],x);b[1]=max((int)b[1],x+span.length);}
 top_bands_ready=true;
}
static void ui_mark_car(const UiFrame &f,bool side_on,uint32_t side_sig,uint32_t refl_sig,bool top_on,uint32_t top_sig){
 const UiXf &xf=f.side;
 for(int i=0;i<6;++i){const auto &p=ART_PATHS[i];const HaloBox *boxes=art_chunks(i);
  for(int c=0;c<ART_CHUNKS;++c){
   const bool exists=c*ART_CHUNK+1<p.count;HaloBox box,refl;
   if(exists){const auto &b=boxes[c];
    box.add(b.x0*xf.s+xf.ox,b.y0*xf.s+xf.oy,3);box.add(b.x1*xf.s+xf.ox,b.y1*xf.s+xf.oy,3);
    const float m0=max(270.f,540-b.y1),m1=min(304.f,540-b.y0);  // mirrored chunk, inside the reflection band
    if(m1>m0){refl.add(b.x0*xf.s+xf.ox,m0*xf.s+xf.oy,3);refl.add(b.x1*xf.s+xf.ox,m1*xf.s+xf.oy,3);}}
   damage_item(ui_side_slots[i][c],side_on && exists,box,side_sig);
   damage_item(ui_refl_slots[i][c],side_on && exists && !refl.empty(),refl,refl_sig);
  }
 }
 ui_top_bands();
 const int ox=(int)lroundf(233+f.topDx)-ART_TOP.width/2,oy=213-ART_TOP.height/2;
 for(int b=0;b<TOP_BANDS;++b){HaloBox box;
  if(top_band_x[b][1]>top_band_x[b][0]){box.add(ox+top_band_x[b][0],oy+b*16);box.add(ox+top_band_x[b][1],min(oy+b*16+16,oy+(int)ART_TOP.height));}
  damage_item(ui_top_slots[b],top_on && !box.empty(),box,top_sig);
 }
}
static void ui_mark_damage(int32_t now){
 // The Titan's last frame has no quick-action ring or texts: with one already running (a long press while the
 // startup was skipped), the entry repaints the whole screen once instead of adopting it.
 if(damage_adopt && (ui_arc_on(ui_action_arc(now)) || ui_overlay(now).k>0)){damage_adopt=false;dirty(0,0,LCD_W,LCD_H);}
 const auto f=ui_frame(now);const auto &v=f.v;
 // Car: side body, wing, airflow, top body and the two wheel columns are separate items, so a mode change
 // repaints only the wing or the wheels, never the whole car.
 HaloSig sb;sb.add(v.phase);sb.add(v.page);sb.add(v.from);sb.addf(v.mix);sb.addf(v.fade);sb.addf(f.body);
 HaloSig sc=sb,sw=sb,sf,su=sb,sh=sb;
 const bool side_on=f.sideA>0,top_on=f.topA>0;
 // The reflection fades in with the UI entry; the body itself does not (so the entry does not repaint the car).
 sc.addf(f.side.s);sc.addf(f.side.ox);sc.addf(f.side.oy);sc.addf(f.sideA);
 HaloSig sr_refl=sc;sr_refl.addf(ui_settle(now));

 sw.addf(f.side.s);sw.addf(f.side.ox);sw.addf(f.side.oy);sw.addf(f.la[0]);sw.addf(f.AZ);sw.addf(ui_wing_deg(f));sw.addf(ui_ghost(now,f));
 damage_item(ui_slots[7],f.la[0]>0,ui_wing_box(f),sw.h);
 const auto flow=ui_flow(now,f);HaloBox fb;
 for(int i=0;i<flow.n;++i){fb.add(flow.a[i].x,flow.a[i].y,2);fb.add(flow.b[i].x,flow.b[i].y,2);sf.addf(flow.a[i].x);sf.addf(flow.a[i].y);sf.addf(flow.b[i].x);sf.addf(flow.b[i].y);}
 sf.addf(flow.alpha);damage_item(ui_slots[8],flow.n>0,fb,sf.h);
 for(int slot=0;slot<2;++slot){const auto af=ui_action_flow(now,f,slot);HaloBox ab;HaloSig sa;
  for(int i=0;i<af.n;++i){ab.add(af.a[i].x,af.a[i].y,2);ab.add(af.b[i].x,af.b[i].y,2);sa.addf(af.a[i].x);sa.addf(af.a[i].y);sa.addf(af.b[i].x);sa.addf(af.b[i].y);}
  sa.addf(af.alpha);damage_item(ui_slots[slot?17:6],af.n>0,ab,sa.h);}
 {const float lights=ui_lights_alpha(now);HaloBox lb;lb.add(233-UI_LIGHT_DX,UI_LIGHT_Y,9);lb.add(233+UI_LIGHT_DX,UI_LIGHT_Y,9);
  HaloSig sl;sl.addf(lights);for(int i=0;i<3;++i)sl.addf(ui_light(now,i));damage_item(ui_slots[0],lights>0,lb,sl.h);}
 su.addf(f.topDx);su.addf(f.topA);
 ui_mark_car(f,side_on,sc.h,sr_refl.h,top_on,su.h);
 const bool wheels_on=f.la[1]>0 || f.la[2]>0;
 sh.addf(f.topDx);sh.addf(f.la[1]);sh.addf(f.la[2]);sh.add(ui_shown(1));sh.add(ui_shown(2));
 HaloSig sfront=sh,srear=sh;
 sfront.addf(ui.fb);
 srear.addf(ui.rear);srear.addf(ui.page==1 && ui.mtValid?max(0.f,1-(now-ui.mtAt)/240.f):0);srear.addf(max(0.f,1-(now-ui.zeroAt)/140.f));
 const float tk=(now-ui.trailAt)/700.f;srear.addf(ui.page==1 && tk>=0 && tk<1?tk:-1);
 for(int i=0;i<4;++i){float kw;const float lv=ui_level(now,i,kw);HaloSig &target=i<2?sfront:srear;target.addf(lv);target.addf(kw);}
 damage_item(ui_slots[9],wheels_on,ui_wheel_box(f,0,false),sfront.h);damage_item(ui_slots[10],wheels_on,ui_wheel_box(f,1,false),sfront.h);
 damage_item(ui_slots[11],wheels_on,ui_wheel_box(f,0,true),srear.h);damage_item(ui_slots[12],wheels_on,ui_wheel_box(f,1,true),srear.h);
 // Ring (hairline, intro arc with heads, edit pulse) — damaged as a ring of small boxes.
 HaloSig sr;const float idle_left=8000-(int32_t)(now-nav_last_input);
 sr.add(ui.edit?mix565(UI_RING,UI_RING_EDIT,clamp_float(idle_left/1000,0,1)):UI_RING);
 sr.add(!ui.edit && now-ui.exitAt<200?mix565(UI_RING,UI_RING_EDIT,ui.exitK*(1-smooth((now-ui.exitAt)/200.f))):0);
 const float ip=(now-ui.introAt)/520.f;sr.addf(ip>=0 && ip<1?smooth(ip):2);
 const float iq=ui_pulse_phase(now);sr.addf(ui.edit && iq>=0 && iq<1?iq:-1);
 // What changed decides the area: the intro only adds arc pieces (and moves its heads), a colour fade only
 // touches the hairline, the edit pulse the band it swells into.
 static float intro_prev=-1;static bool pulse_prev=false;static uint16_t color_prev=0;
 const uint16_t ring_color=!ui.edit && now-ui.exitAt<200?mix565(UI_RING,UI_RING_EDIT,ui.exitK*(1-smooth((now-ui.exitAt)/200.f))):
  ui.edit?mix565(UI_RING,UI_RING_EDIT,clamp_float(idle_left/1000,0,1)):UI_RING;
 const bool intro=ip>=0 && ip<1,pulse=ui.edit && iq>=0 && iq<1;const float intro_a=intro?180*smooth(ip):180;
 if(sr.h!=ui_slots[1].sig || !ui_slots[1].on){
  if(!damage_adopt){
   if(!ui_slots[1].on || pulse || pulse_prev)damage_ring_bounds(233,233,221,235,RING_PULSE_BOUNDS,7);
   else if(intro_prev>=0 && ring_color==color_prev && intro_a!=intro_prev){
    const float a0=min(intro_prev,intro_a),a1=max(intro_prev,intro_a);
    HaloBox left=ui_arc_box(226,270-a1-1,270-a0+1,8),right=ui_arc_box(226,270+a0-1,270+a1+1,8);
    const HaloBox *const pieces[2]={&left,&right};
    for(const HaloBox *b:pieces)dirty((int16_t)floorf(b->x0),(int16_t)floorf(b->y0),(int16_t)(ceilf(b->x1)-floorf(b->x0))+1,(int16_t)(ceilf(b->y1)-floorf(b->y0))+1);
    if(ring_color!=color_prev)damage_ring_bounds(233,233,223,229,RING_HAIR_BOUNDS,7);
   }
   else damage_ring_bounds(233,233,223,229,RING_HAIR_BOUNDS,7);
  }
  ui_slots[1].on=true;ui_slots[1].sig=sr.h;
 }
 intro_prev=intro || ip>=1?intro_a:-1;pulse_prev=pulse;color_prev=ring_color;
 // Page marks.
 HaloBox marks;marks.add(172,8);marks.add(294,34);HaloSig sm;sm.addf(ui.pagePos);sm.add(ui.edit);sm.addf(ip<0?0:smooth(min(ip,1.f)));
 damage_item(ui_slots[2],true,marks,sm.h);
 // Mode segments (one item each) and the selection bar (its own item): moving the bar repaints only the
 // arc it sweeps, not the whole segment row.
 auto segA=[](float i){return 90+(1.5f-i)*19;};
 const uint8_t seg_page=ui.page,chosen=ui.edit?ui.cand:ui.applied[seg_page];
 for(int i=0;i<4;++i){
  const int d=abs(i-(int)chosen);
  const float k=i==chosen?0:ui.edit?smooth((now-ui.editAt-22*(d-1))/90.f):1-smooth((now-ui.exitAt-18*(3-d))/80.f);
  HaloSig sg;sg.addf(k);sg.add(UI_OFF[seg_page][i]);sg.add(torus_locked(fw_of(seg_page),i));
  damage_item(ui_slots[13+i],k>0,ui_arc_box(214,segA(i)-8,segA(i)+8,5),sg.h);
 }
 const float open=ui.edit?smooth((now-ui.editAt)/110.f):1-smooth((now-ui.exitAt)/110.f);
 const float barHalf=4+4*open,barW=3+2*open;
 const float lo=min(segA(ui.lead),segA(ui.trail))-barHalf,hi=max(segA(ui.lead),segA(ui.trail))+barHalf;
 HaloSig ss;
 ss.add(seg_page);ss.add(UI_OFF[seg_page][chosen]);ss.addf(ui.lead);ss.addf(ui.trail);ss.addf(open);
 ss.add(v.phase==UI_CROSS);ss.addf(ip<0?0:smooth(min(ip,1.f)));ss.addf(clamp_float(1-(now-ui.pulseAt)/90.f,0,1));
 ss.addf(now-ui.commitAt<180?sinf(PI*(now-ui.commitAt)/180.f):0);ss.addf(f.ax);
 damage_item(ui_slots[3],true,ui_arc_box(214,lo,hi,(barW+6)/2+3),ss.h);
 // Title and mode name (cross-fades, tracking, glide).
 const auto t=ui_texts(now);HaloSig st,sn;
 st.add(ui.page);st.add(ui.edit);st.addf(t.tA);st.addf(t.tB);st.addf(t.tLS);st.addf(t.gA);st.addf(t.gB);st.add(t.title_b);
 sn.add(ui.page);sn.add(ui_shown(ui.page));sn.addf(t.nA);sn.addf(t.nB);sn.addf(t.nLS);sn.addf(t.gA);sn.addf(t.gB);
 sn.add(t.name_b?(int32_t)(intptr_t)t.name_b:0);
 const auto ov=ui_overlay(now);
 st.addf(ov.k);st.add(ov.k>0?(int32_t)(intptr_t)ov.title:0);sn.addf(ov.k);sn.add(ov.k>0?(int32_t)(intptr_t)ov.name:0);
 damage_item(ui_slots[4],true,ui_title_box(t,ov),st.h);
 const float lk=ui_lock_k(now);sn.addf(lk);sn.add(ui.lockPos);
 damage_item(ui_slots[5],true,ui_name_box(t,ov,lk),sn.h);
 {const float lkA=smooth(lk*2-1);HaloSig sh2;sh2.addf(lkA);sh2.add(ui.lockReason?(int32_t)(intptr_t)ui.lockReason:0);
  damage_item(ui_slots[18],lkA>0,ui.lockReason?ui_text_box(WEB_LABEL,ui.lockReason,0,nullptr,UI_HINT_TOP,UI_HINT_BOTTOM):HaloBox{},sh2.h);}
 // Quick-action arc: while only its length changes (the hold filling, Boost counting down) just the swept pieces
 // with their heads; any change of its look repaints the halo band.
 // A full ring is painted by the ring table, a partial one as an arc (slightly different pixels): switching
 // between the two repaints the band too.
 const UiArc arc=ui_action_arc(now);const bool arc_on=ui_arc_on(arc),arc_ring=arc_on && arc.a>=180 && !pulse && !intro;
 if(!damage_adopt && (arc_on || ui_arc_prev_on)){
  const auto &p=ui_arc_prev;
  if(arc_on && ui_arc_prev_on && arc_ring==ui_arc_prev_ring && arc.color==p.color && arc.alpha==p.alpha && arc.half==p.half && arc.heads==p.heads){
   if(arc.a!=p.a){const float a0=min(arc.a,p.a),a1=max(arc.a,p.a);
    const HaloBox pieces[2]={ui_arc_box(226,270-a1-1,270-a0+1,10),ui_arc_box(226,270+a0-1,270+a1+1,10)};
    for(const HaloBox &b:pieces)
     dirty((int16_t)floorf(b.x0),(int16_t)floorf(b.y0),(int16_t)(ceilf(b.x1)-floorf(b.x0))+1,(int16_t)(ceilf(b.y1)-floorf(b.y0))+1);}
  }else damage_ring_bounds(233,233,218,236,RING_PULSE_BOUNDS,7);
 }
 ui_arc_prev=arc;ui_arc_prev_on=arc_on;ui_arc_prev_ring=arc_ring;
}
