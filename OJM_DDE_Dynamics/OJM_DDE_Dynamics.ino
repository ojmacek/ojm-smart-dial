#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESP32Encoder.h>
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_heap_caps.h"
// V6.0.6.3: the drawing code is float/pixel loops; the Arduino default -Os is noticeably slower for them.
#pragma GCC optimize("O2")
// Device work counter for the Serial report (inner pixel-loop iterations per frame).
static uint32_t halo_work=0;
#define HALO_COUNT(kind) (++halo_work)
#define HALO_WORK_COUNTER 1
#include "src/SmartDial_Fonts_DINish.h"
#include "src/DdeProtocol.h"

// Hardware, QSPI, CO5300 offsets, TE and encoder setup retained from V5.6.1.
#define TFT_RST 8
#define TFT_CS 10
#define TFT_SCLK 11
#define TFT_D0 12
#define TFT_D1 13
#define TFT_D2 14
#define TFT_D3 15
#define TFT_TE 9
#define ENCODER_A 1
#define ENCODER_B 2
#define ENCODER_BUTTON 0
#define ENCODER_REVERSED 0
#define ENCODER_TRANSITIONS_PER_STEP 4
#define CAN_TX_GPIO 43
#define CAN_RX_GPIO 44
#define LCD_W 466
#define LCD_H 466
#define LCD_CX 233
#define LCD_CY 233
#define DISPLAY_BUS_HZ 80000000L
#define DISPLAY_BRIGHTNESS 225
#define GFX_BITMAP_BIG_ENDIAN 0
#define USE_TE_SYNC 1
#define TE_WAIT_TIMEOUT_US 18000UL
#define BUTTON_DEBOUNCE_MS 24UL
#define BUTTON_LONG_MS 700UL
Arduino_DataBus *bus=new Arduino_ESP32QSPI(TFT_CS,TFT_SCLK,TFT_D0,TFT_D1,TFT_D2,TFT_D3);
Arduino_GFX *display=new Arduino_CO5300(bus,TFT_RST,0,LCD_W,LCD_H,9,0,0,0);
static constexpr size_t FRAME_PIXELS=(size_t)LCD_W*LCD_H,FRAME_BYTES=FRAME_PIXELS*sizeof(uint16_t);
static constexpr size_t PUSH_PIXELS=(size_t)LCD_W*LCD_H,PUSH_BYTES=PUSH_PIXELS*sizeof(uint16_t);
static uint16_t *g_base_frame=nullptr,*g_frame=nullptr,*g_push_buffer=nullptr;
#include "src/BaselineGraphics.h"
#include "src/BaselineShapes.h"
#include "src/BaselineInput.h"
#include "src/CanTransport.h"
#include "src/TorusContext.h"
#include "src/DynamicsModel.h"
#include "src/DdeNavigation.h"
// 1 = DMA panel transfer straight from the framebuffer (V6.0.6.2); 0 = the library transfer of 6.0.6.1.
#define FAST_QSPI 1
#define SHADOW_CHECK 0  // development: compare what went to the panel with a clean render during the logo hold
// Development: knob input over USB serial and a UI report every second (both harmless in use).
#define SERIAL_TEST_INPUT 1
#define UI_REPORT_MS 1000
#if FAST_QSPI
#include "src/FastQspi.h"
#endif
#include "src/DdeRenderer.h"
#if SERIAL_TEST_INPUT
#include "src/DevBench.h"
#endif

// HLAVNI TAB PRO OBHAJOBU
// setup(): puvodni hardware, framebuffery a CAN task.
// loop(): lokalni vstup -> model -> navigace (250 ms potvrzeni) -> publikace stavu -> vykresleni (TE takt).
// src/ oddeluje navigaci, lokalni model, renderer a transport od grafickych dat.
// Dial funguje bez ECU. PC pouze meni a zrcadli rezimy pres CAN.

// Report only when a serial listener has space; no on-screen debug overlay
// or writes during the animation. Three one-shot lines diagnose real hardware.
static uint8_t startup_report_next=0;  // 4 = all startup lines sent
static void report_startup_perf(){
 static char line[192];static size_t length=0,sent=0;uint8_t &next=startup_report_next;
 if(startup_active || next>=4 || !Serial)return;
 static const char *const names[]={"infinity+laser","close","Titan"};
 if(!length){
  if(next<3){const auto &p=signature_perf[next];if(!p.frames){++next;return;}
   length=snprintf(line,sizeof(line),"startup %s: fps=%.1f draw=%.2fms TE=%.2fms QSPI=%.2fms max=%.2fms over33=%lu\n",
    names[next],p.interval_us?1000000.f*p.intervals/p.interval_us:0.f,
    p.draw_us/(1000.f*p.frames),p.te_us/(1000.f*p.frames),p.transfer_us/(1000.f*p.frames),
    p.max_frame_us/1000.f,(unsigned long)p.over_30);
  }else length=snprintf(line,sizeof(line),"startup AA: %s fast=%luus reference=%luus\n",
   coverage_use_fast?"fast":"reference",(unsigned long)coverage_fast_us,(unsigned long)coverage_reference_us);
 }
 int space=Serial.availableForWrite();if(space<=0)return;
 size_t count=min((size_t)space,length-sent);sent+=Serial.write((const uint8_t*)line+sent,count);
 if(sent==length){++next;length=sent=0;}
}

// UI frame report every 3 s while something moves (only after the startup report, only into free TX space).
static void report_jobs(){
 if(!job_trace_ready)return;
 char l[400];int n=0;n+=snprintf(l+n,sizeof(l)-n,"jobs warm=%lu:",(unsigned long)ui_t_warm);
 for(int k=0;k<job_trace_count && n<(int)sizeof(l)-40;++k){const auto &c=job_cost[k];n+=snprintf(l+n,sizeof(l)-n," [%d-%d c%u %lu..%lu]",c.y0,c.y1,c.core,(unsigned long)c.start,(unsigned long)c.end);}
 if(Serial.availableForWrite()<n+2)return;
 Serial.print(l);Serial.print('\n');job_trace_ready=false;
}
static void report_rects(){
 static int sent=0;
 if(!rect_trace_ready)return;
 while(sent<rect_cost_count){const auto &k=rect_cost[sent];const auto &r=k.r;char l[120];
  snprintf(l,sizeof(l),"rect %2d %3d,%3d %3dx%3d px=%6d core=%u restore=%6lu paint=%7lu order=%6lu cyc/px=%.0f\n",sent,r.x,r.y,r.w,r.h,r.w*r.h,k.core,
   (unsigned long)k.restore,(unsigned long)k.paint,(unsigned long)k.order,(float)(k.restore+k.paint+k.order)/(r.w*r.h));
  if(Serial.availableForWrite()<(int)strlen(l))return;
  Serial.print(l);++sent;}
 sent=0;rect_trace_ready=false;
}
static void report_trace(){
 static int sent=-1;
 if(trace_left && millis()-trace_armed_at>2500)trace_left=0;  // at most 2.5 s of frames
 if(trace_left || trace_used==0){if(trace_left)sent=-1;return;}
 if(sent<0)sent=0;
 while(sent<trace_used){const auto &t=trace_buf[sent];
  const uint32_t gap=sent?t.start-trace_buf[sent-1].start:0;char l[240];
  snprintf(l,sizeof(l),"trace %3d gap=%5.1f draw=%5.1f te=%5.1f qspi=%4.1f px=%6lu rects=%u\n",sent,gap/1000.f,t.draw/1000.f,t.te/1000.f,t.qspi/1000.f,(unsigned long)t.px,t.rects);
  const int n=strlen(l)-1;snprintf(l+n,sizeof(l)-n," up=%.1f pt=%.1f warm=%.2f pool=%u | side=%.1f refl=%.1f top=%.1f wing=%.1f wheel=%.1f ring=%.1f seg=%.1f text=%.1f send=%ldus queue=%ldus fails=%ld\n",t.update/1000.f,t.paint/1000.f,t.restore/1000.f,t.pool*16,t.prof[0]/1000.f,t.prof[1]/1000.f,t.prof[2]/1000.f,t.prof[3]/1000.f,t.prof[4]/1000.f,t.prof[5]/1000.f,t.prof[6]/1000.f,t.prof[7]/1000.f,(long)t.ring_steps,(long)t.queue_us,(long)t.fails);
  if(Serial.availableForWrite()<(int)strlen(l))break;  // only into free TX space
  Serial.print(l);++sent;}
 if(sent>=trace_used){trace_used=0;sent=-1;}
}
static void report_ui_perf(){
 static char line[560];static size_t length=0,sent=0;static uint32_t at=0;
 if(startup_active || startup_report_next<4 || !Serial)return;
 if(!length){
  const uint32_t now=millis();if(now-at<UI_REPORT_MS)return;at=now;
  FramePerf p=ui_perf;ui_perf=FramePerf{};if(!p.frames)return;
  length=snprintf(line,sizeof(line),"ui: fps=%.1f draw=%.2fms (upd=%.2f rest=%.2f paint=%.2f work=%lu pool=%lu%%) max=%.2fms TE=%.2fms QSPI=%.2fms px=%lu rects=%.1f late=%lu frames=%lu\n",
   p.interval?1000000.f*p.intervals/p.interval:0.f,p.draw/(1000.f*p.frames),p.update/(1000.f*p.frames),p.restore/(1000.f*p.frames),p.paint/(1000.f*p.frames),(unsigned long)(p.work/p.frames),(unsigned long)(p.pixels?100ull*p.pool/p.pixels:0),p.draw_max/1000.f,p.te/(1000.f*p.frames),
   p.qspi/(1000.f*p.frames),(unsigned long)(p.pixels/p.frames),(float)p.rects/p.frames,(unsigned long)p.late,(unsigned long)p.frames);
  // The profile goes on the same line, replacing its newline (ms per frame).
  if(length>0 && length<sizeof(line))length=length-1+snprintf(line+length-1,sizeof(line)-length+1," prof side=%.2f refl=%.2f top=%.2f wing=%.2f wheel=%.2f ring=%.2f seg=%.2f text=%.2f\n",
   p.prof[0]/1000.f/p.frames,p.prof[1]/1000.f/p.frames,p.prof[2]/1000.f/p.frames,p.prof[3]/1000.f/p.frames,p.prof[4]/1000.f/p.frames,p.prof[5]/1000.f/p.frames,p.prof[6]/1000.f/p.frames,p.prof[7]/1000.f/p.frames);
  if(length>=sizeof(line))length=sizeof(line)-1;
  if(length>0 && length<sizeof(line))length=length-1+snprintf(line+length-1,sizeof(line)-length+1," steps side=%lu/%lu refl=%lu/%lu wing=%lu ring=%lu seg=%lu text=%lu\n",
   (unsigned long)(prof_steps[0]/p.frames),(unsigned long)(prof_calls[0]/p.frames),(unsigned long)(prof_steps[1]/p.frames),(unsigned long)(prof_calls[1]/p.frames),
   (unsigned long)(prof_steps[3]/p.frames),(unsigned long)(prof_steps[5]/p.frames),(unsigned long)(prof_steps[6]/p.frames),(unsigned long)(prof_steps[7]/p.frames));
  for(auto &v:prof_steps)v=0;for(auto &v:prof_calls)v=0;
  if(length>=sizeof(line))length=sizeof(line)-1;
 }
 int space=Serial.availableForWrite();if(space<=0)return;
 size_t count=min((size_t)space,length-sent);sent+=Serial.write((const uint8_t*)line+sent,count);
 if(sent==length)length=sent=0;
}
#if FAST_QSPI && QSPI_SELFTEST
// Development: one 140x140 square per transfer method, all left on the panel for 6 s (yellow top = library,
// blue left = single transactions, green right = segmented with single-line commands, red bottom = segmented
// with quad commands). The squares that appear show which methods the panel takes.
static void qspi_square(uint16_t color,int16_t x,int16_t y){
  const uint32_t n=140u*140u;if(!render_pool || render_pool_px<n || !qspi_dma)return;
  const uint16_t pc=(uint16_t)((color>>8)|(color<<8));for(uint32_t i=0;i<n;++i)render_pool[i]=pc;
  qx_push_rect(x,y,140,140,render_pool,140,true);qspi_wait_idle();
}
static void qspi_selftest_single(){Serial.println("selftest: blue (single)");Serial.flush();qx_window[0]=-1;qspi_square(0x001F,20,163);Serial.println("selftest: blue done");}
static void qspi_selftest_segmented(){
  if(!qspi_sct)return;
  Serial.println("selftest: green (segmented, single-line commands)");Serial.flush();
  qspi_sct_split=true;qx_window[0]=-1;qspi_square(0x07E0,306,163);
  Serial.println("selftest: red (segmented, quad commands)");Serial.flush();
  qspi_sct_split=false;qx_window[0]=-1;qspi_square(0xF800,163,306);
  qspi_sct_split=QSPI_SPLIT;qx_window[0]=-1;
  Serial.println("qspi selftest: yellow top=library, blue left=single, green right=segmented single-line commands, red bottom=segmented quad commands");
  delay(6000);
}
#endif
void setup(){
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  pinMode(ENCODER_BUTTON,INPUT_PULLUP);
  ESP32Encoder::useInternalWeakPullResistors=puType::up;
  encoder.attachFullQuad(ENCODER_A,ENCODER_B);encoder.setFilter(1023);encoder.clearCount();
  g_encoder_last_raw=0;g_encoder_substeps=0;
  if(!display->begin(DISPLAY_BUS_HZ)){Serial.println("FATAL: display begin failed");while(true)delay(1000);}
  enable_te_output();
#if FAST_QSPI
  if(!fast_qspi_begin())Serial.println("FAST_QSPI unavailable: library transfer");
#endif
  render_pool_begin();
  g_base_frame=(uint16_t*)heap_caps_malloc(FRAME_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  g_frame=(uint16_t*)heap_caps_malloc(FRAME_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  g_push_buffer=(uint16_t*)heap_caps_malloc(PUSH_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!g_base_frame || !g_frame || !g_push_buffer){Serial.println("FATAL: PSRAM allocation failed");while(true)delay(1000);}
  memset(g_base_frame,0,FRAME_BYTES);memset(g_frame,0,FRAME_BYTES);
  set_brightness(0);display->fillScreen(0);
  // Panel settling retained; no multi-second logo delay. Startup runs in loop().
  delay(220);set_brightness(DISPLAY_BRIGHTNESS);delay(45);
#if FAST_QSPI && QSPI_SELFTEST
  display->fillRect(163,20,140,140,0xFFE0);  // yellow, top: the library's own transfer
#endif
#if FAST_QSPI
  if(!qspi_takeover())Serial.println("queued QSPI unavailable: transfer after painting");
#endif
#if FAST_QSPI && QSPI_SELFTEST
  qspi_selftest_segmented();
#endif
  coverage_select_backend(); // Choose the cheaper AA path on this exact MCU.
  startup_prepare();         // Startup geometry once, before the first frame (no first-frame stall).
  can_publish_state(requested,0);can_begin();startup_started=millis();
  Serial.println("OJM SYSTEMS DDE V6.0.7 / DYNAMICS / CAN v2 / Halo 2 / Performance Torus");
#if FAST_QSPI
  Serial.printf("qspi: %s transfer, TE edges per 50 ms: on %lu, TEOFF %lu, TEON %lu | segmented: TEOFF %lu, TEON quad %lu, off+on %lu\n",qspi_mode,
   (unsigned long)qspi_te_check[0],(unsigned long)qspi_te_check[1],(unsigned long)qspi_te_check[2],(unsigned long)qspi_te_check[3],(unsigned long)qspi_te_check[4],(unsigned long)qspi_te_check[5]);
#endif
}
void loop(){
  // Input always precedes model/ACK application and rendering. No CAN driver waits here.
  int16_t delta=consume_encoder_delta();ButtonEvent button=read_encoder_button();
#if SERIAL_TEST_INPUT
  // Development input over USB serial (same path as the knob): '+'/'-' turn, 'c' click, 'l' long press;
  // car simulation (TorusContext.h): '0'..'9' speed x25 km/h, 'b' brake, 'p' charging, 'k' Drive Mode, 'x' off.
  while(Serial.available()>0){const int c=Serial.read();
    if(c=='+')++delta;else if(c=='-')--delta;else if(c=='c')button=BUTTON_SHORT;else if(c=='l')button=BUTTON_LONG;else if(c=='B')dev_bench();else if(c=='J'){job_trace_ready=false;job_trace_armed=true;}else if(c=='R'){rect_trace_ready=false;rect_trace_armed=true;}else if(c=='T'){trace_used=0;trace_left=TRACE_FRAMES;trace_armed_at=millis();}else torus_dev_key(c);}
#endif
  model_service(); // Drain already-received remote requests; newest local input then wins.
  if(delta || button!=BUTTON_NONE){if(startup_active){startup_active=false;startup_skipped=true;base_invalid=true;}}
  navigation_input(delta,button);navigation_service(millis()); // 250 ms commit, auto-exit, remote follow.
  can_publish_state(requested,effective_faults);render_tick();report_startup_perf();report_ui_perf();report_trace();report_rects();report_jobs();
  // Sleep only while the next frame is far away; close to it, just yield so the TE edge is not missed.
  if(!te_known || (int32_t)(frame_target_us-micros())>2500)delay(1);else yield();
}
