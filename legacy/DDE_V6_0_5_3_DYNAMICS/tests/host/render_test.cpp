#include "Arduino.h"
#define LCD_W 466
#define LCD_H 466
#define USE_TE_SYNC 0
#define GFX_BITMAP_BIG_ENDIAN 0
uint32_t host_us=0;
static constexpr size_t FRAME_PIXELS=466*466,FRAME_BYTES=FRAME_PIXELS*2,PUSH_PIXELS=FRAME_PIXELS;
static uint16_t base[FRAME_PIXELS],frame[FRAME_PIXELS],scratch[FRAME_PIXELS],panel[FRAME_PIXELS];
static uint16_t *g_base_frame=base,*g_frame=frame,*g_push_buffer=scratch;
static uint32_t transferred=0,max_transfer=0,checked_frames=0,max_vectoring=0;
class Arduino_CO5300 {
 public:
  void setBrightness(uint8_t){}
  void draw16bitRGBBitmap(int x,int y,uint16_t *pixels,int w,int h){
    if(x<0 || y<0 || x+w>466 || y+h>466)abort();
    // The old RAM-only panel stub accepted illegal CO5300 update windows.
    // Check the hardware contract, not an invented emulation of corrupt writes.
    if((x|y|w|h)&1){
      fprintf(stderr,"FAIL CO5300 unaligned transfer x=%d y=%d w=%d h=%d at %u ms\n",x,y,w,h,millis());exit(1);
    }
    for(int row=0;row<h;++row)memcpy(panel+(y+row)*466+x,pixels+row*w,w*2);
    transferred+=w*h;
  }
};
static Arduino_CO5300 display_object,*display=&display_object;
#include "SmartDial_Fonts_DINish.h"
#include "BaselineGraphics.h"
#include "BaselineShapes.h"
#include "DdeProtocol.h"
struct twai_message_t {uint8_t data[8];};
static uint8_t can_take_commands(twai_message_t *){return 0;}
static void can_queue_ack(const twai_message_t &,uint8_t){}
static bool can_take_telemetry(Telemetry &,uint32_t &,uint32_t &){return false;}
static void can_publish_state(const RequestedState &,uint8_t){}
static void can_request_state(){}
#include "DynamicsModel.h"
#include "DdeNavigation.h"
#include "DdeRenderer.h"
static void require(bool condition,const char *message){if(!condition){fprintf(stderr,"FAIL %s at %u ms\n",message,millis());exit(1);}}
static void check_clean_scene(){
  if(startup_active || base_invalid)return;
  static uint16_t clean[FRAME_PIXELS];memcpy(clean,g_base_frame,FRAME_BYTES);
  auto actual=g_frame;g_frame=clean;
  paint_car();labels(millis());
  g_frame=actual;
  for(size_t p=0;p<FRAME_PIXELS;++p)if(panel[p]!=clean[p]){
    fprintf(stderr,"Stale pixel x=%zu y=%zu actual=%04x clean=%04x time=%u page=%u detail=%d\n",p%466,p/466,panel[p],clean[p],millis(),shown_page,detail_open);exit(1);
  }
}
static void save(const char *directory,const char *sequence,unsigned n){
  char path[512];snprintf(path,sizeof(path),"%s/%s_%03u.ppm",directory,sequence,n);
  FILE *f=fopen(path,"wb");require(f!=nullptr,"open snapshot");fprintf(f,"P6\n466 466\n255\n");
  for(auto v:panel){unsigned char rgb[3]={(unsigned char)(((v>>11)&31)*255/31),(unsigned char)(((v>>5)&63)*255/63),(unsigned char)((v&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);
}
static void tick(uint32_t advance=16667,bool live=true){
  host_us+=advance;
  if(live){telemetry_received_at=millis();telemetry_revision=1;}
  model_service();transferred=0;render_tick();++checked_frames;max_transfer=max(max_transfer,transferred);
  for(size_t p=0;p<FRAME_PIXELS;++p)if(panel[p]!=g_frame[p]){
    fprintf(stderr,"Unpushed pixel x=%zu y=%zu time=%u\n",p%466,p/466,millis());exit(1);
  }
  check_clean_scene();
}
static void settle(int n=30,bool live=true){for(int i=0;i<n;++i)tick(16667,live);}
static void check_transfer_windows(){
  // Coordinate-coded pixels catch stride errors when a window is expanded.
  for(size_t p=0;p<FRAME_PIXELS;++p)frame[p]=(uint16_t)(1+p%65535);
  auto check=[](int x,int y,int w,int h,int left,int top,int right,int bottom){
    memset(panel,0,FRAME_BYTES);dirty_count=0;dirty(x,y,w,h);
    require(dirty_count==1,"one expanded window");auto r=dirty_rects[0];
    require(r.x==left && r.y==top && r.x+r.w==right && r.y+r.h==bottom,"minimal outward alignment");
    transferred=0;push_dirty(micros());
    require(transferred==(uint32_t)(right-left)*(bottom-top),"exact aligned transfer size");
    for(int yy=0;yy<LCD_H;++yy)for(int xx=0;xx<LCD_W;++xx){
      bool inside=xx>=left && xx<right && yy>=top && yy<bottom;
      require(panel[yy*LCD_W+xx]==(inside?frame[yy*LCD_W+xx]:0),"aligned stride and untouched outside");
    }
  };
  for(int x=20;x<22;++x)for(int y=40;y<42;++y)
    for(int w=5;w<7;++w)for(int h=7;h<9;++h)
      check(x,y,w,h,x&~1,y&~1,(x+w+1)&~1,(y+h+1)&~1);
  check(143,143,181,47,142,142,324,190); // real wheel/pulse update
  check(50,130,368,155,50,130,418,286); // formerly invalid startup
  check(465,465,1,1,464,464,466,466);
  check(-3,-1,6,4,0,0,4,4);check(463,463,10,10,462,462,466,466);
  check(0,0,466,466,0,0,466,466);
  dirty_count=0;dirty(3,5,0,10);dirty(3,5,10,-1);dirty(466,0,5,5);dirty(-5,0,2,5);
  require(dirty_count==0,"empty/outside windows stay empty");
  dirty(1,1,1,1);dirty(2,2,1,1);
  require(dirty_count==1 && dirty_rects[0].w==4 && dirty_rects[0].h==4,"merge after alignment");
  dirty_count=0;
  for(int i=0;i<17;++i)dirty(1+i*4,1,1,1);
  require(dirty_count==1 && dirty_rects[0].x==0 && dirty_rects[0].w==66,"overflow merge stays aligned");
  dirty_count=0;memset(frame,0,FRAME_BYTES);memset(panel,0,FRAME_BYTES);transferred=0;
  printf("PASS CO5300: 22 patterned transfers, all parity combinations, clipping, empty, merge and overflow\n");
}
int main(int argc,char **argv){
  require(argc==2,"output directory argument");const char *out=argv[1];
  check_transfer_windows();
  // The reveal must crop the original logo at 1:1 coordinates, never rescale it.
  static uint16_t full_logo[FRAME_PIXELS],partial_logo[FRAME_PIXELS];
  auto original_frame=g_frame;g_frame=full_logo;outline(g_frame,ART_LOGO,233,233,WHITE,255);
  for(int step=0;step<=100;++step){
    memset(partial_logo,0,FRAME_BYTES);g_frame=partial_logo;float p=step/100.f;logo_unfold(p);
    int half=lroundf(ART_LOGO.width*.5f*smooth(p));
    for(int y=0;y<466;++y)for(int x=0;x<466;++x){
      uint16_t expected=x>=233-half && x<233+half?full_logo[y*466+x]:0;
      require(partial_logo[y*466+x]==expected,"logo strip reveal preserves exact full-size pixels");
    }
  }
  g_frame=original_frame;
  telemetry.speed=90;telemetry.front=22;telemetry.rear_actual=-5;telemetry.rear_requested=-5;
  telemetry.aero_requested=60;telemetry.aero_actual=60;
  for(int i=0;i<4;++i)telemetry.torque[i]=15+i*20;
  static uint16_t held_logo[FRAME_PIXELS];bool logo_saved=false;unsigned logo_hold_frames=0;
  for(unsigned n=0;n<240;++n){
    tick();save(out,"startup",n);
    if(millis()>=770 && millis()<1770){
      if(!logo_saved){memcpy(held_logo,panel,FRAME_BYTES);logo_saved=true;}
      else require(memcmp(held_logo,panel,FRAME_BYTES)==0,"logo stays fully visible without flicker");
      ++logo_hold_frames;
    }
  }
  require(logo_hold_frames>=60,"logo holds for one second at 60 Hz");
  require(measure_text(SD_FONT_LABEL,MENU_NAMES[0])<=380,"full RWS title fits");
  require(!startup_active && !detail_open,"startup ends in browse");
  require(fabsf(body_level-1)<.001f,"startup hands off without dimming the Titan");
  // A settled click updates context in its first rendered frame and does not
  // launch a whole-body brightness fade. No waiting before the next turn.
  settle(90,false);flow_at=millis()+10000;
  navigation_input(0,BUTTON_SHORT);tick(16667,false);
  require(edit_mix==1 && rail_x==244 && rail_velocity==0,"click enters mode selector in first frame");
  require(body_level==1 && !mode_animating && transferred<45000,"click has no body dimming or label fade");
  tick(16667,false);require(transferred==0,"settled Aero click leaves no animation backlog");
  navigation_input(1,BUTTON_NONE);tick(16667,false);
  require(requested.aero==3,"rotation immediately after click selects AIR BRAKE");
  navigation_input(0,BUTTON_SHORT);tick(16667,false);
  require(edit_mix==0 && rail_x==211 && rail_velocity==0,"click exits in first frame");
  apply_requested(0x10,0);apply_requested(0x11,3);tick(16667,false);
  navigation_input(0,BUTTON_SHORT);tick(16667,false);
  require(rail_x==200,"RWS AUTO is first indicator");
  const uint8_t rws_order[]={3,1,2,0};
  for(int i=1;i<4;++i){navigation_input(1,BUTTON_NONE);tick(16667,false);require(requested.rws==rws_order[i],"RWS forward UI order retains CAN IDs");}
  navigation_input(32767,BUTTON_NONE);require(requested.rws==0 && nav_boundary,"RWS OFF upper boundary");
  for(int i=2;i>=0;--i){navigation_input(-1,BUTTON_NONE);tick(16667,false);require(requested.rws==rws_order[i],"RWS reverse UI order");}
  navigation_input(-32768,BUTTON_NONE);require(requested.rws==3 && nav_boundary,"RWS AUTO lower boundary");
  navigation_input(0,BUTTON_SHORT);apply_requested(0x10,2);apply_requested(0x13,2);tick(16667,false);
  for(int n=0;n<60;++n){if(n%20==0)navigation_input(1,BUTTON_NONE);tick();save(out,"menu",n);}
  require(requested.rws==3 && requested.tv==1 && requested.aero==2,"menu rotation never edits modes");
  apply_requested(0x10,0);navigation_input(-1,BUTTON_NONE);require(requested.page==2,"negative menu wrap");
  navigation_input(32767,BUTTON_NONE);require(requested.page==0,"large positive menu burst");
  navigation_input(-32768,BUTTON_NONE);require(requested.page==1,"large negative menu burst");
  navigation_input(0,BUTTON_SHORT);require(detail_open,"click enters selected page");tick();
  navigation_input(32767,BUTTON_NONE);require(requested.tv==3 && requested.page==1,"detail rotation changes only mode, upper clamp");tick();
  navigation_input(-32768,BUTTON_NONE);require(requested.tv==0,"lower clamp");tick();
  navigation_input(1,BUTTON_NONE);tick();tick(350000);require(!mode_animating,"mode animation completes after skipped frame");
  navigation_input(0,BUTTON_SHORT);tick();require(!detail_open && requested.page==1,"click returns with selection preserved");
  // Interruption during animated selection always follows the latest request.
  for(int n=0;n<150;++n){navigation_input(n%2?2:-1,BUTTON_NONE);tick();}
  navigation_input(0,BUTTON_SHORT);tick();require(detail_open,"click during scroll enters latest selection");
  navigation_input(0,BUTTON_LONG);tick();require(!detail_open,"hold escapes without resetting modes");
  // Each remote page is shown in the current local browse/edit context.
  for(int page=0;page<3;++page){
    apply_requested(0x10,page);tick();require(!detail_open,"remote page does not hijack navigation");
    navigation_input(0,BUTTON_SHORT);tick();
    for(unsigned n=0;n<90;++n){
      telemetry.front=30*sinf(n*.08f);telemetry.rear_actual=-8*sinf(n*.08f);
      for(int i=0;i<4;++i)telemetry.torque[i]=50+50*sinf(n*.08f+i*1.3f);
      telemetry.tc_mask=page==1 && n>=30 && n<60?2:0;
      telemetry.aero_actual=50+50*sinf(n*.06f);telemetry.aero_requested=telemetry.aero_actual;
      telemetry.aero_mode=page==2 && n>=30 && n<60?3:0;
      tick();save(out,page==0?"rws":page==1?"torque":"aero",n);
      if(page==1)max_vectoring=max(max_vectoring,transferred);
    }
    settle(100);for(int n=0;n<60;++n){tick();require(transferred<=31000,"bounded autonomous animation regions");}
    navigation_input(0,BUTTON_SHORT);tick();
  }
  require(max_vectoring<=406*372,"interaction stays inside aligned scene, never full screen");
  for(int from=0;from<3;++from)for(int to=0;to<3;++to){
    detail_open=true;apply_requested(0x10,from);tick();apply_requested(0x10,to);tick();
    for(int n=0;n<25;++n){apply_requested(0x11+to,n%4);telemetry.faults=n%4;tick();}
    telemetry.faults=0;settle();
  }
  // CAN connectivity never controls the local animation model.
  settle(150,false);require(!telemetry_online,"offline timeout");
  navigation_input(0,BUTTON_SHORT);tick(16667,false);navigation_input(1,BUTTON_NONE);tick(130000,false);
  navigation_input(0,BUTTON_SHORT);tick(130000,false);require(detail_open,"offline navigation works");
  require(visual.front>10,"standalone steering stays animated without ECU");
  for(int page=0;page<3;++page){
    apply_requested(0x10,page);detail_open=true;
    for(int mode=0;mode<4;++mode){
      apply_requested(0x11+page,mode);
      for(int n=0;n<90;++n){tick(16667,false);save(out,page==0?"standalone_rws":page==1?"standalone_vectoring":"standalone_aero",mode*90+n);}
      if(page==0 && mode==0)require(fabsf(visual.rear)<.01f,"RWS OFF reaches zero");
      if(page==0 && mode==1)require(visual.rear>3,"STABLE same phase");
      if(page==0 && mode==2)require(visual.rear<-5,"AGILE counter phase");
      if(page==1 && mode==0)require(visual.vector_strength<.001f,"TV OFF extinguishes pulses");
      if(page==2 && mode==1)require(fabsf(visual.aero-5)<.01f,"LOW DRAG local position");
      if(page==2 && mode==2)require(fabsf(visual.aero-75)<.01f,"DOWNFORCE local position");
      if(page==2 && mode==3)require(fabsf(visual.aero-100)<.01f,"AIR BRAKE selectable locally");
    }
  }
  // Malicious/stale legacy telemetry cannot change the standalone model.
  apply_requested(0x10,1);apply_requested(0x12,3);detail_open=true;settle(90,false);
  require(edit_mix==1 && fabsf(rail_x-266)<.1f,"four-mode rail settles at MAX");
  navigation_input(1,BUTTON_NONE);tick(16667,false);require(gesture_edge,"end-stop feedback");
  navigation_input(-3,BUTTON_NONE);tick(16667,false);navigation_input(3,BUTTON_NONE);tick(16667,false);settle(90,false);
  require(requested.tv==3 && fabsf(rail_x-266)<.1f,"rail retargets latest selection without queue");
  navigation_input(0,BUTTON_SHORT);settle(90,false);
  require(edit_mix==0 && fabsf(rail_x-255)<.1f,"confirmation folds rail back to three pages");
  apply_requested(0x10,2);tick(16667,false);require(flow_phase<0,"airflow waits for wing settle");
  telemetry.front=-45;telemetry.rear_actual=-15;telemetry.aero_actual=0;telemetry.faults=3;telemetry.aero_mode=1;
  settle(60,true);require(effective_faults==0 && visual.aero>99,"connected ECU cannot override local AIR BRAKE animation");
  require(telemetry.aero_actual==0,"local illustration is not written into received CAN telemetry");
  RequestedState example;example.page=1;
  float previous=-1;
  for(int mode=0;mode<4;++mode){example.tv=mode;auto t=local_presentation(example,1000);require(t.torque[3]>previous,"all TV modes have distinct loads");previous=t.torque[3];}

  // Skip startup at a visible logo frame and enter a detail immediately.
  memset(base,0,sizeof(base));memset(frame,0,sizeof(frame));memset(panel,0,sizeof(panel));
  startup_active=true;startup_started=millis();detail_open=false;
  settle(60,false);startup_active=false;base_invalid=true;navigation_input(1,BUTTON_SHORT);tick(16667,false);
  require(detail_open && shown_page==requested.page,"startup skip honors input without logo remnants");
  // Every TV pulse/echo/arrival must stay clear of every white outline pixel.
  static uint16_t silhouette[FRAME_PIXELS],effects[FRAME_PIXELS];
  auto original=g_frame;g_frame=silhouette;top_body(g_frame);top_mix=1;gesture=0;
  for(int mode=0;mode<4;++mode){
    requested.tv=mode;visual.vector_strength=mode==0?0:mode==1?.32f:mode==2?.68f:1;
    for(int n=0;n<128;++n){
      visual.pulse_phase=n/128.f;memset(effects,0,FRAME_BYTES);g_frame=effects;
      for(int i=0;i<4;++i){visual.torque[i]=20+25*i;draw_visual(1,i,0,0);}
      for(size_t p=0;p<FRAME_PIXELS;++p)require(!silhouette[p] || !effects[p],"pulse or wheel never crosses white outline");
    }
  }
  g_frame=original;
  printf("PASS: 512 pulse/echo phases never cross the body outline; rail morph, end-stop, latest-wins and airflow delay.\n");
  printf("PASS: 101 logo reveal widths preserve fixed-size source pixels; no zoom or scaling.\n");
  printf("PASS: %u frames; full-panel transfer and clean-render equality; browse/edit navigation; bursts; all standalone modes including AIR BRAKE; legacy telemetry isolation; offline/stalls; startup/skip.\n",checked_frames);
  printf("Max live vectoring: %u pixels/frame; mode-driven standalone animation. Max including scene entry/startup: %u.\n",max_vectoring,max_transfer);
  printf("Host QSPI/TE stubs; these are pixel budgets, NOT measured ESP32 FPS or latency.\n");
}
