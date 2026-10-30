// V6.0.6 host test: the REAL firmware headers (input, navigation, model, startup, Halo 2 UI, renderer) run
// against a memory panel and a simulated TE line. Every pushed frame is compared with a clean full render.
#include "Arduino.h"
#define LCD_W 466
#define LCD_H 466
#define USE_TE_SYNC 1
#define TFT_TE 9
#define TE_WAIT_TIMEOUT_US 18000UL
#define GFX_BITMAP_BIG_ENDIAN 0
#define INPUT 0
uint32_t host_us=0;
#define FRAME_SPIN() (host_us+=5)
static uint64_t host_work[8];static bool host_counting=true;
#define HALO_COUNT(kind) (host_counting?++host_work[kind]:0)
// Deliberately not the nominal 16667 us: the pacing has to learn the real panel period.
static constexpr uint32_t PANEL_TE_US=16600,TE_PULSE_US=200;
typedef int gpio_num_t;
static int gpio_get_level(gpio_num_t){host_us+=5;return (host_us%PANEL_TE_US)<TE_PULSE_US;}
static void pinMode(int,int){}
struct HostBus {void beginWrite(){} void writeC8D8(int,int){} void endWrite(){}};
static HostBus bus_object,*bus=&bus_object;
static void yield(){}
static constexpr size_t FRAME_PIXELS=466*466,FRAME_BYTES=FRAME_PIXELS*2,PUSH_PIXELS=FRAME_PIXELS;
static uint16_t base[FRAME_PIXELS],frame[FRAME_PIXELS],scratch[FRAME_PIXELS],panel[FRAME_PIXELS];
static uint16_t *g_base_frame=base,*g_frame=frame,*g_push_buffer=scratch;
static uint16_t host_pool[64000];
static uint16_t host_slow[1][(size_t)466*466+48*32];  // the PSRAM overflow area
static uint32_t transferred=0,checked_frames=0,late_pushes=0;
static void check_window(int x,int y,int w,int h){
  if(x<0 || y<0 || x+w>466 || y+h>466)abort();
  // Check the hardware contract, not an invented emulation of corrupt writes.
  if((x|y|w|h)&1){
    fprintf(stderr,"FAIL CO5300 unaligned transfer x=%d y=%d w=%d h=%d at %u ms\n",x,y,w,h,millis());exit(1);
  }
  if(host_us%PANEL_TE_US>=TE_PULSE_US+50)++late_pushes;
}
class Arduino_CO5300 {
 public:
  void setBrightness(uint8_t){}
  void draw16bitRGBBitmap(int x,int y,uint16_t *pixels,int w,int h){
    check_window(x,y,w,h);
    for(int row=0;row<h;++row)memcpy(panel+(y+row)*466+x,pixels+row*w,w*2);
    transferred+=w*h;
  }
};
static Arduino_CO5300 display_object,*display=&display_object;
// Host FAST_QSPI: the firmware's own chunk conversion (QspiStream.h) feeds the memory panel. Every chunk is
// decoded from big-endian and placed by a running cursor in the window: stride, chunking and byte order.
#define FAST_QSPI 1
static bool fast_qspi=true;static uint32_t fast_chunks=0;
#include "QspiStream.h"
static uint16_t host_qspi_a[QSPI_CHUNK_PIXELS],host_qspi_b[QSPI_CHUNK_PIXELS];
static uint16_t *const host_qspi[2]={host_qspi_a,host_qspi_b};
static void fast_push_rect(int16_t x,int16_t y,int16_t w,int16_t h,const uint16_t *src,int16_t stride,bool panel_order=false,bool psram=false){
  (void)psram;
  check_window(x,y,w,h);uint32_t cursor=0;const uint16_t *last=nullptr;
  if(panel_order && stride==w){
    // The firmware sends these straight from the pool: they must already be big-endian.
    for(uint32_t i=0;i<(uint32_t)w*h;++i){const uint16_t v=src[i];panel[(y+i/w)*466+x+i%w]=(uint16_t)((v>>8)|(v<<8));}
    transferred+=w*h;return;
  }
  qspi_stream_rect(src,stride,w,h,host_qspi,[&](uint16_t *buffer,uint32_t pixels,bool first){
    if(first!=(cursor==0) || buffer==last){fprintf(stderr,"FAIL qspi chunk order\n");exit(1);}
    last=buffer;++fast_chunks;
    for(uint32_t i=0;i<pixels;++i,++cursor){const uint16_t v=buffer[i];panel[(y+cursor/w)*466+x+cursor%w]=(uint16_t)((v>>8)|(v<<8));}
  });
  if(cursor!=(uint32_t)w*h){fprintf(stderr,"FAIL qspi stream length\n");exit(1);}
  transferred+=w*h;
}
#include "SmartDial_Fonts_DINish.h"
#include "BaselineGraphics.h"
#include "BaselineShapes.h"
#include "DdeProtocol.h"
#define ENCODER_TRANSITIONS_PER_STEP 4
#define ENCODER_REVERSED 0
#define ENCODER_BUTTON 0
#define BUTTON_DEBOUNCE_MS 24UL
#define BUTTON_LONG_MS 700UL
#define HIGH 1
#define LOW 0
static int host_button_raw=HIGH;
static int digitalRead(int){return host_button_raw;}
struct ESP32Encoder {int64_t raw=0;int64_t getCount(){return raw;}};
#include "BaselineInput.h"
struct twai_message_t {uint8_t data[8];};
static twai_message_t host_commands[COMMAND_COUNT];static uint8_t host_command_count=0;
static uint8_t can_take_commands(twai_message_t *out){uint8_t n=host_command_count;memcpy(out,host_commands,sizeof(twai_message_t)*n);host_command_count=0;return n;}
static uint8_t last_ack_status=255;
static void can_queue_ack(const twai_message_t &,uint8_t status){last_ack_status=status;}
static bool can_take_telemetry(Telemetry &,uint32_t &,uint32_t &){return false;}
static RequestedState published;static uint32_t publish_count=0;static uint8_t published_bits=0;
static void can_publish_state(const RequestedState &s,uint8_t bits){published=s;published_bits=bits;++publish_count;}
static void can_request_state(){}
// V6.0.6.7: the car state and the Drive Mode as the CAN worker would hand them over (always fresh while set).
struct CanContext {VehicleState vehicle;uint32_t vehicle_at=0,vehicle_revision=0;uint8_t drive_mode=DRIVE_UNKNOWN;uint32_t drive_at=0,drive_revision=0;};
static bool host_vehicle_on=false;static VehicleState host_vehicle;static uint8_t host_drive=DRIVE_UNKNOWN;
static void can_take_context(CanContext &c){c=CanContext{};
  if(host_vehicle_on){c.vehicle=host_vehicle;c.vehicle_at=millis();c.vehicle_revision=1;}
  if(host_drive!=DRIVE_UNKNOWN){c.drive_mode=host_drive;c.drive_at=millis();c.drive_revision=1;}}
#include "TorusContext.h"
#include "DynamicsModel.h"
#include "DdeNavigation.h"
#include "DdeRenderer.h"
static void require(bool condition,const char *message){if(!condition){fprintf(stderr,"FAIL %s at %u ms\n",message,millis());exit(1);}}
static void remote(uint8_t command,uint8_t value){host_commands[host_command_count++]={{command,value,1,0,0,0,0,0}};}

// ---- frame checks ----
static uint16_t clean[FRAME_PIXELS];
static void compare_clean(const char *what){
  for(size_t p=0;p<FRAME_PIXELS;++p)if(panel[p]!=clean[p]){
    fprintf(stderr,"FAIL %s: stale pixel x=%zu y=%zu panel=%04x clean=%04x frame=%u ms page=%u edit=%d\n",
      what,p%466,p/466,panel[p],clean[p],frame_now_ms,requested.page,detail_open);exit(1);
  }
}
static void verify(bool was_startup){
  ++checked_frames;
  // The panel is compared with a clean full render of the same moment (the frame buffer is no longer a mirror
  // of the panel: rectangles are painted in the internal pool and sent from there).
  auto actual=g_frame;g_frame=clean;art_clip={0,0,LCD_W,LCD_H};host_counting=false;halo_target_frame(clean);
  if(was_startup){memset(clean,0,FRAME_BYTES);render_startup_scene(frame_now_ms-startup_started);}
  else{memcpy(clean,g_base_frame,FRAME_BYTES);ui_paint(frame_now_ms);}
  g_frame=actual;host_counting=true;halo_target_frame(g_frame);
  compare_clean(was_startup?"startup":"ui");
}
// ---- the main loop at 0.5 ms resolution, saving one picture per panel refresh while recording ----
static const char *out_dir=nullptr,*recording=nullptr;static unsigned record_index=0;
static uint32_t last_pushed_frames=0,last_frame_transfer=0,max_ui_transfer=0,edge_gaps_bad=0,frames_on_one_edge=0;
static uint32_t last_push_edge=0;static bool getenv_rects=false;
struct Budget {const char *name;uint32_t frames,sum,max;uint64_t work[8];double host_ms;};
#include <time.h>
static double host_clock_ms(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e3+t.tv_nsec/1e6;}
static double tick_ms=0;
static Budget budgets[8];static int budget_count=0;
static void account(const char *name,uint32_t px){
  int i=0;while(i<budget_count && strcmp(budgets[i].name,name))++i;
  if(i==budget_count)budgets[budget_count++]={name,0,0,0};
  auto &b=budgets[i];++b.frames;b.sum+=px;b.max=max(b.max,px);
  for(int k=0;k<8;++k){b.work[k]+=host_work[k];host_work[k]=0;}b.host_ms+=tick_ms;
}
static void save_picture(){
  char path[512];snprintf(path,sizeof(path),"%s/%s_%03u.ppm",out_dir,recording,record_index++);
  FILE *f=fopen(path,"wb");require(f!=nullptr,"open snapshot");fprintf(f,"P6\n466 466\n255\n");
  static unsigned char row[466*3];
  for(int y=0;y<466;++y){for(int x=0;x<466;++x){uint16_t v=panel[y*466+x];
    row[x*3]=((v>>11)&31)*255/31;row[x*3+1]=((v>>5)&63)*255/63;row[x*3+2]=(v&31)*255/31;}fwrite(row,1,sizeof(row),f);}
  fclose(f);
}
static void step(){
  const uint32_t edge_before=host_us/PANEL_TE_US;
  host_us+=500;
  // Same order as loop() in the .ino.
  model_service();navigation_service(millis());can_publish_state(requested,effective_faults);
  const bool was_startup=startup_active;transferred=0;
  {const double t0=host_clock_ms();render_tick();tick_ms=host_clock_ms()-t0;}
  if(render_stats.frames!=last_pushed_frames){
    last_pushed_frames=render_stats.frames;last_frame_transfer=transferred;
    const uint32_t edge=host_us/PANEL_TE_US;
    if(edge==last_push_edge)++frames_on_one_edge;
    last_push_edge=edge;
    if(!was_startup)max_ui_transfer=max(max_ui_transfer,transferred);
#ifdef PRINT_WORK
    if(!was_startup && recording && !strcmp(recording,"aero"))printf("work %5u ms px %6u rects %2u line %6llu arc %6llu poly %5llu text %5llu top %5llu hit %5llu\n",frame_now_ms,transferred,dirty_count,
      (unsigned long long)host_work[0],(unsigned long long)host_work[1],(unsigned long long)host_work[2],(unsigned long long)host_work[3],(unsigned long long)host_work[4],(unsigned long long)host_work[6]);
#endif
    account(was_startup?"startup":recording?recording:"other ui",transferred);
    if(getenv_rects && !was_startup && recording && transferred>60000){printf("big %s %u ms: %u px:",recording,frame_now_ms,transferred);
      for(uint8_t i=0;i<dirty_count;++i)printf(" [%d,%d %dx%d]",dirty_rects[i].x,dirty_rects[i].y,dirty_rects[i].w,dirty_rects[i].h);printf("|");}
    verify(was_startup);
  }
  if(recording && host_us/PANEL_TE_US!=edge_before && record_index<2000)save_picture();
}
static void run_ms(uint32_t ms){const uint32_t end=host_us+ms*1000;while((int32_t)(host_us-end)<0)step();}
static void turn(int16_t d){navigation_input(d,BUTTON_NONE);}
static void click(){navigation_input(0,BUTTON_SHORT);}
static void hold(){navigation_input(0,BUTTON_LONG);}
static uint8_t rws_pos(){return mode_position(0,requested.rws);}

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
  check(143,143,181,47,142,142,324,190);
  check(50,130,368,155,50,130,418,286);
  check(465,465,1,1,464,464,466,466);
  check(-3,-1,6,4,0,0,4,4);check(463,463,10,10,462,462,466,466);
  check(0,0,466,466,0,0,466,466);
  dirty_count=0;dirty(3,5,0,10);dirty(3,5,10,-1);dirty(466,0,5,5);dirty(-5,0,2,5);
  require(dirty_count==0,"empty/outside windows stay empty");
  dirty(1,1,3,3);dirty(2,2,3,3);
  require(dirty_count==1 && dirty_rects[0].w==6 && dirty_rects[0].h==6,"overlapping windows merge after alignment");
  dirty_count=0;dirty(0,0,4,4);dirty(4,0,4,4);
  require(dirty_count==2,"touching windows stay two windows");
  dirty_count=0;
  for(int i=0;i<DIRTY_MAX+5;++i)dirty(1+i*6,1+(i%3)*10,1,1);
  require(dirty_count<=DIRTY_MAX,"overflow merges into a neighbour");
  uint32_t area=0;
  for(uint8_t i=0;i<dirty_count;++i){auto r=dirty_rects[i];require(((r.x|r.y|r.w|r.h)&1)==0,"overflow stays aligned");area+=r.w*r.h;
    for(uint8_t k=0;k<i;++k){auto q=dirty_rects[k];require(!(r.x<q.x+q.w && r.x+r.w>q.x && r.y<q.y+q.h && r.y+r.h>q.y),"windows never overlap");}}
  require(area<20000,"overflow never collapses into one screen-sized window");
  // A ring change is a set of bands, never the full screen.
  dirty_count=0;damage_ring(233,233,216,236);area=0;
  for(uint8_t i=0;i<dirty_count;++i)area+=dirty_rects[i].w*dirty_rects[i].h;
  require(dirty_count>=8 && area<110000,"ring damage stays a thin set of bands");
  printf("PASS CO5300: patterned transfers, all parity combinations, clipping, empty, overlap merge, touching kept, overflow, ring bands (%u px).\n",area);
  dirty_count=0;memset(frame,0,FRAME_BYTES);memset(panel,0,FRAME_BYTES);transferred=0;te_known=false;
}
static void check_input(){
  auto reset_input=[](){encoder.raw=g_encoder_last_raw=g_encoder_substeps=0;};
  auto input=[](int64_t raw,int expected){encoder.raw=raw;require(consume_encoder_delta()==expected,"encoder edge sequence");};
  const int noise[]={0,1,0,-1,0,2,1,0,-2,-1,0};
  reset_input();for(int raw:noise)input(raw,0);
  reset_input();input(1,0);input(2,0);input(3,1);input(2,0);input(3,0);input(4,0);
  input(3,0);input(2,0);input(1,-1);input(2,0);input(1,0);input(0,0);
  reset_input();input(3,1);input(-1,-1);input(3,1);input(-1,-1);
  reset_input();for(int n=1;n<=1000;++n)input(n*4,1);
  for(int n=999;n>=0;--n)input(n*4,-1);
  input(400000,32767);input(400000,32767);input(400000,32767);input(400000,1699);input(400000,0);
  input(0,-32767);input(0,-32767);input(0,-32767);input(0,-1699);input(0,0);
  reset_input();
  // The same poll can carry a debounced release and an accumulated turn: the click acts first.
  requested=RequestedState{};requested.page=0;requested.rws=3;detail_open=false;
  host_button_raw=LOW;require(read_encoder_button()==BUTTON_NONE,"button press edge is debounced");
  host_us+=24000;require(read_encoder_button()==BUTTON_NONE,"no short click before release");
  host_button_raw=HIGH;require(read_encoder_button()==BUTTON_NONE,"button release edge is debounced");
  host_us+=24000;ButtonEvent release=read_encoder_button();
  require(release==BUTTON_SHORT,"one short click on release");
  encoder.raw=3;navigation_input(consume_encoder_delta(),release);
  require(detail_open && requested.page==0 && edit_candidate==2 && requested.rws==3,"release plus turn moves the RWS candidate, never the page");
  navigation_input(1,BUTTON_SHORT);
  require(!detail_open && requested.rws==1 && requested.page==1,"exit sends the candidate, then the turn browses");
  navigation_input(-1,BUTTON_LONG);
  require(!detail_open && requested.page==0,"hold plus turn uses browse context");
  require(read_encoder_button()==BUTTON_NONE,"held release does not duplicate short click");
  require(torus.action==ACTION_LAUNCH,"a long press in the menu arms Launch (standing)");
  requested=RequestedState{};detail_open=false;reset_input();torus=TorusContext{};
  printf("PASS input: partial-edge reversal, rebound rejection, 2000 full detents, burst carry, debounced release plus turn.\n");
}

int main(int argc,char **argv){
  setvbuf(stdout,nullptr,_IONBF,0);
#ifdef PRINT_BIG_FRAMES
  getenv_rects=true;
#endif
  require(argc==2,"output directory argument");out_dir=argv[1];
  check_transfer_windows();fast_qspi=false;check_transfer_windows();fast_qspi=true;check_input();
  render_pool=host_pool;render_pool_px=sizeof(host_pool)/sizeof(host_pool[0]);render_slow=host_slow[0];halo_target_frame(g_frame);
  host_us=1000000;nav_last_input=millis();
  startup_prepare();
  require(startup_total_ms>=6400 && startup_total_ms<=6600,"startup is 6.5 s");
  require(close_at>=980 && close_at<=1080,"the infinity closes at about 1.03 s");
  require(dist_at[650]>start_e+start_h1*.45f && dist_at[650]<start_e+start_h1*.55f,"both balls cross the centre at 0.65 s");
  printf("PASS timing: infinity closed %u ms, laser end %u ms, fill %u..%u ms, total %u ms.\n",close_at,trace_end,startup_fill_start,startup_draw_ms,startup_total_ms);

  // ---- startup: every frame equals a clean render of its own time ----
  memset(base,0,FRAME_BYTES);memset(frame,0,FRAME_BYTES);memset(panel,0,FRAME_BYTES);
  startup_active=true;base_invalid=true;startup_started=millis();
  recording="startup";record_index=0;
  uint32_t startup_frames=0,hold_transfers=0,hold_frames=0,max_startup_transfer=0;
  static uint16_t last_startup[FRAME_PIXELS];
  while(startup_active){
    const uint32_t before=render_stats.frames;step();
    if(render_stats.frames!=before){++startup_frames;max_startup_transfer=max(max_startup_transfer,last_frame_transfer);
      const uint32_t e=frame_now_ms-startup_started;
      if(startup_stage(e)==1){++hold_frames;if(last_frame_transfer)++hold_transfers;}
      memcpy(last_startup,panel,FRAME_BYTES);}
  }
  recording=nullptr;
  require(hold_transfers<=1,"the held logo is sent once");
  require(signature_perf[0].frames>=150,"drawing phase renders every refresh");
  // Seamless handover: the first UI frame shows exactly the Titan of the last startup frame.
  {const uint32_t before=render_stats.frames;while(render_stats.frames==before)step();}
  for(int y=150;y<320;++y)for(int x=40;x<426;++x)require(panel[y*466+x]==last_startup[y*466+x],"Titan hands over to the UI without a pop");
  printf("PASS startup: %u frames, all equal to clean renders; max %u px/frame; hold sent %u time(s); Titan -> UI identical.\n",startup_frames,max_startup_transfer,hold_transfers);

  // ---- UI entry and idle: nothing is sent while nothing moves ----
#ifdef PRINT_INTRO
  for(int f=0;f<16;){const uint32_t before=render_stats.frames;step();if(render_stats.frames!=before){++f;
    printf("intro %d %u ms px=%u:",f,frame_now_ms,transferred);for(uint8_t i=0;i<dirty_count;++i)printf(" [%d,%d %dx%d]",dirty_rects[i].x,dirty_rects[i].y,dirty_rects[i].w,dirty_rects[i].h);printf("\n");}}
#endif
  run_ms(1500);
  {uint32_t sent=0;for(int i=0;i<200;++i){transferred=0;step();sent+=transferred;}require(sent==0,"idle UI sends nothing");}
  require(te_period_us>PANEL_TE_US-40 && te_period_us<PANEL_TE_US+40,"pacing learns the real TE period");

  // ---- menu: a turn changes the page at once; cross-fade, same-view slide and fast spin ----
  recording="menu";record_index=0;max_ui_transfer=0;
  const uint8_t start_page=requested.page;
  turn(1);require(requested.page==(start_page+1)%3,"menu turn changes page at once");run_ms(700);
  turn(1);run_ms(700);turn(1);run_ms(700);
  require(requested.page==start_page,"three turns wrap around");
  turn(-1);run_ms(700);turn(1);run_ms(120);turn(1);run_ms(120);turn(1);run_ms(700); // fast spin: quick fades
  recording=nullptr;
  require(requested.rws==3 && requested.tv==1 && requested.aero==2,"menu rotation never edits modes");
  {const uint8_t p=requested.page;turn(32767);require(requested.page==(p+32767%3)%3,"large positive menu burst");}
  turn(-32768);run_ms(800);
  printf("PASS menu: page cross-fade/slide/fast fade; wrap; bursts; max %u px/frame.\n",max_ui_transfer);

  // ---- choosing a mode: candidate, 250 ms commit, edge bounce, click send, hold cancel, idle exit ----
  auto go_page=[](uint8_t fw){while(requested.page!=fw){turn(1);run_ms(400);}run_ms(600);};
  const char *names[3]={"rws","torque","aero"};
  static const uint8_t page_tour[3]={2,0,1};
  host_drive=DRIVE_TRACK;  // the Drive Torus is in Track: every TV option (Drift too) can be chosen
  for(uint8_t fw:page_tour){
    go_page(fw);recording=names[fw];record_index=0;
    click();run_ms(500);require(detail_open,"click opens the selector");
    const uint8_t first=applied_position(fw);
    for(int k=0;k<4;++k){
      const uint8_t pos=(first+1+k)%4;
      turn((int16_t)pos-(int16_t)edit_candidate);
      run_ms(240);require(applied_position(fw)!=pos,"nothing is sent before 250 ms of rest");
      run_ms(30);require(applied_position(fw)==pos,"candidate is sent after 250 ms of rest");
      require(published.rws==requested.rws && published.tv==requested.tv && published.aero==requested.aero,"CAN state follows the sent mode");
      run_ms(900);
    }
    // Fast spin: only the final mode is ever sent.
    const uint8_t at=applied_position(fw);int sent_between=0;
    for(int k=0;k<6;++k){turn(k%2?-1:1);run_ms(60);if(applied_position(fw)!=at)++sent_between;}
    require(sent_between==0,"a fast spin never sends the modes in between");
    run_ms(400);
    // Edge bounce.
    turn(-8);run_ms(400);const uint32_t edges=nav_edge_seq;turn(-1);run_ms(400);require(nav_edge_seq==edges+1 && applied_position(fw)==0,"end stop bounces");
    recording=nullptr;
    // Click sends a pending candidate immediately; hold cancels one.
    require(detail_open,"selector still open after the bounce");turn(2);click();require(!detail_open && applied_position(fw)==2,"click sends the pending candidate at once");
    run_ms(600);click();run_ms(300);turn(1);hold();require(!detail_open && applied_position(fw)==2,"hold leaves without sending");
    run_ms(600);click();run_ms(8100);require(!detail_open,"8 s without input leaves the selector");
    run_ms(500);
  }
  printf("PASS selector: candidate, 250 ms commit, CAN publish, fast spin sends once, edge bounce, click send, hold cancel, idle exit.\n");

  // ---- RWS visual order Off, Auto, Stable, Agile (0,3,1,2) retains the CAN values ----
  go_page(0);click();run_ms(300);turn(-3);run_ms(300);require(requested.rws==0,"RWS OFF first");
  const uint8_t rws_order[]={0,3,1,2};
  for(int i=1;i<4;++i){turn(1);run_ms(300);require(requested.rws==rws_order[i] && rws_pos()==i,"RWS UI order retains CAN IDs");}
  click();run_ms(500);

  // ---- remote commands: a page change while choosing, a mode change mirrored by the open selector ----
  click();run_ms(300);turn(-1);remote(0x10,1);run_ms(50);
  require(requested.rws==1 && requested.page==1 && detail_open && edit_page==1,"remote page sends the local choice (Agile -> Stable) and follows");
  remote(0x12,3);run_ms(50);require(edit_candidate==3,"open selector mirrors a remote mode change");
  run_ms(600);click();run_ms(500);
  remote(0x13,0);remote(0x10,2);run_ms(1200);require(requested.page==2 && requested.aero==0,"remote changes in the menu");
  printf("PASS remote: page follow, mirror, ACK path; RWS order Off, Auto, Stable, Agile (0,3,1,2).\n");

  // ---- V6.0.6.7 Performance Torus: the situation, Auto picks, Launch, Boost and locks ----
  host_drive=DRIVE_UNKNOWN;run_ms(50);require(requested.tv!=3,"without Track Drift falls back");
  go_page(0);recording="action";record_index=0;  // RWS page: Launch and Boost cross-fade to the side view
  // Standing (no car frame): a hold fills the halo, the long press arms Launch; the next turn only cancels it.
  g_button_down=true;g_button_down_at=millis();run_ms(690);g_button_down=false;hold();
  require(torus.action==ACTION_LAUNCH,"a long press while standing arms Launch");
  run_ms(60);require((published_bits&1)==1,"Launch armed is published (state byte 5 bit 0)");run_ms(700);
  {const auto f=ui_frame(frame_now_ms);require(f.ax==1 && f.sideA==1 && f.topA==0,"Launch shows the side view on any page");}
  {const uint8_t p=requested.page;turn(1);require(torus.action==ACTION_NONE && requested.page==p && !detail_open,"a turn only cancels Launch");}
  run_ms(400);
  // A short click never arms anything (the ring shows only after 120 ms and fades).
  g_button_down=true;g_button_down_at=millis();run_ms(200);g_button_down=false;click();require(detail_open && torus.action==ACTION_NONE,"a click still opens the selector");
  run_ms(300);hold();run_ms(400);
  // Launch, then the car moves off: launched.
  hold();require(torus.action==ACTION_LAUNCH,"Launch armed again");run_ms(300);
  host_vehicle_on=true;host_vehicle=VehicleState{};host_vehicle.mode=VEHICLE_DRIVE;host_vehicle.speed=10;run_ms(100);
  require(torus.action==ACTION_NONE && torus.event==EVENT_LAUNCH_GO,"moving off launches");run_ms(1000);
  // Driving: Boost drains the halo for 10 s; browsing goes on meanwhile.
  host_vehicle.speed=60;run_ms(300);
  g_button_down=true;g_button_down_at=millis();run_ms(690);g_button_down=false;hold();
  require(torus.action==ACTION_BOOST,"a long press while driving starts Boost");run_ms(1600);
  recording=nullptr;
  require((published_bits&2)==2,"Boost is published (state byte 5 bit 1)");
  require(ui_ax(frame_now_ms)==1,"Boost shows the side view");
  turn(1);run_ms(500);require(ui_ax(frame_now_ms)==0,"a turn during Boost leaves the action view");
  turn(-1);run_ms(500);require(torus.action==ACTION_BOOST,"Boost keeps running while browsing");
  run_ms(7500);require(torus.action==ACTION_NONE && torus.event==EVENT_BOOST_END,"Boost ends after 10 s");
  hold();require(torus.event==EVENT_REFUSED,"no Boost during the cooldown");run_ms(600);
  // Auto picks from the car state (Aero page shows the wing, RWS page the rear wheels).
  remote(0x13,0);run_ms(400);
  host_vehicle.speed=100;run_ms(900);require(torus.aero_auto==2 && torus.rws_auto==2,"Auto at 100 km/h: Downforce, Stable");
  host_vehicle.brake=80;run_ms(900);require(torus.aero_auto==3,"hard braking at speed: Air Brake");
  host_vehicle.brake=0;run_ms(900);host_vehicle.speed=30;run_ms(900);require(torus.aero_auto==1 && torus.rws_auto==3,"slow: Low Drag, Agile");
  go_page(0);remote(0x11,3);run_ms(500);host_vehicle.speed=100;run_ms(900);host_vehicle.speed=30;run_ms(900);
  // Locks above 120 km/h: RWS/TV Off cannot be chosen by the knob nor set over CAN.
  host_vehicle.speed=130;run_ms(100);require(torus.off_locked,"Off locked above 120 km/h");
  remote(0x11,0);run_ms(50);require(requested.rws==3 && last_ack_status==1,"a remote RWS Off is refused at speed");
  recording="locks";record_index=0;
  click();run_ms(300);{const uint32_t edges=nav_edge_seq;turn(-3);run_ms(400);
   require(requested.rws==3 && nav_edge_seq==edges+1,"the knob stops before the locked Off");
   require(nav_lock_pos==0 && ui.lockReason && !strcmp(ui.lockReason,"Above 120 km/h"),"the bounce shows why Off is locked");}
  click();run_ms(400);
  go_page(1);click();run_ms(300);turn(3);run_ms(400);require(requested.tv==2,"Drift is locked outside Track");
  require(nav_lock_pos==3 && !strcmp(ui.lockReason,"Track only"),"the bounce on Drift says Track only");run_ms(600);
  recording=nullptr;click();run_ms(400);
  host_drive=DRIVE_TRACK;run_ms(100);click();run_ms(300);turn(1);run_ms(400);require(requested.tv==3,"Drift in Track");click();run_ms(400);
  host_drive=DRIVE_UNKNOWN;run_ms(100);require(requested.tv==2,"leaving Track falls back to Sport");
  host_vehicle.speed=100;run_ms(300);host_vehicle.speed=110;run_ms(300);require(!torus.off_locked,"unlocked again below 115 km/h");
  // The car frame stops: back to the standing defaults (Auto rest looks).
  host_vehicle_on=false;run_ms(1200);require(!torus.vehicle_known && torus.aero_auto==0 && torus.rws_auto==1,"no car frame: standing defaults");
  printf("PASS Performance Torus: hold ring, Launch arm/cancel/go, Boost 10 s + cooldown, Auto picks, Off and Drift locks, CAN refusal.\n");

  // ---- Aero zoom and cross-fades stay exact with arbitrary frame times ----
  click();for(int i=0;i<40;++i){turn(i%3?1:-1);run_ms(37+i%5*11);}hold();run_ms(1500);

  // ---- startup skip at a visible logo frame ----
  memset(base,0,FRAME_BYTES);memset(frame,0,FRAME_BYTES);memset(panel,0,FRAME_BYTES);
  startup_active=true;base_invalid=true;startup_started=millis();ui=UiState{};
  run_ms(2600);startup_active=false;startup_skipped=true;base_invalid=true;click();run_ms(600);
  require(detail_open,"startup skip honours the input");

  require(frames_on_one_edge==0,"never two frames for one TE edge");
  require(late_pushes==0,"every transfer starts at a TE edge");
  printf("PASS: %u frames checked against clean renders; every push at a TE edge, one frame per edge; TE period learned %u us.\n",checked_frames,te_period_us);
  for(int i=0;i<budget_count;++i)printf("budget %-9s %4u frames  mean %6u px  max %6u px\n",budgets[i].name,budgets[i].frames,budgets[i].sum/budgets[i].frames,budgets[i].max);
  for(int i=0;i<budget_count;++i){auto &b=budgets[i];printf("work   %-9s per frame: line %6llu arc %6llu poly %5llu text %5llu top %4llu logo %5llu linehit %6llu hostms %.3f\n",b.name,
    (unsigned long long)(b.work[0]/b.frames),(unsigned long long)(b.work[1]/b.frames),(unsigned long long)(b.work[2]/b.frames),(unsigned long long)(b.work[3]/b.frames),(unsigned long long)(b.work[4]/b.frames),(unsigned long long)(b.work[5]/b.frames),(unsigned long long)(b.work[6]/b.frames),b.host_ms/b.frames);}
  printf("Max UI transfer %u px/frame. Host QSPI/TE stubs: pixel budgets, NOT measured ESP32 FPS or latency.\n",max_ui_transfer);
}
