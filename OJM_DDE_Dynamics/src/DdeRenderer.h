#pragma once
#include "AssetTypes.h"
#include "TitanStrokes.h"
#include "ArtTop.h"
#include "WebFonts.h"
#include "HaloAssets.h"
// Halo 2 renderer (V6.0.6). One rule for every frame: mark exactly what changed, restore it from the base
// frame, repaint the scene clipped to each rectangle, push only those rectangles right after a TE edge.
static constexpr uint16_t WHITE=0xF79D,BACKGROUND=0x0000;
struct DirtyRect {int16_t x,y,w,h;};
static constexpr uint8_t DIRTY_MAX=48;
static DirtyRect dirty_rects[DIRTY_MAX];
static uint8_t dirty_count=0;
static bool base_invalid=true,startup_active=true,startup_skipped=false;
static uint32_t startup_started=0,frame_now_ms=0;  // frame_now_ms: the time the last frame shows
struct RenderStats {
 uint32_t frames=0,draw_us=0,te_us=0,transfer_us=0,total_us=0,pixels=0;
 uint32_t max_draw_us=0,max_te_us=0,max_transfer_us=0,max_total_us=0,max_pixels=0;
 uint32_t over_budget=0;
};
static volatile RenderStats render_stats;
static float smooth(float t){t=clamp_float(t,0,1);return t*t*(3-2*t);}
// V6.0.6.5: the UI is painted on both cores at once (each damaged rectangle on one core), so the clip and the
// render target exist once per core.
static inline int halo_core(){
#if defined(ESP_PLATFORM)
 return xPortGetCoreID();
#else
 return 0;
#endif
}
static DirtyRect art_clip_core[2]={{0,0,LCD_W,LCD_H},{0,0,LCD_W,LCD_H}};
#define art_clip (art_clip_core[halo_core()])
// Radial fade for lines (V6.0.6.5, per core like the clip): between r0 and r1 around the centre of the screen a
// line fades out (linear in r^2), beyond r1 it is not drawn. The zoomed Titan uses it so its lines never cross the
// ring and the segments.
struct ArtFade {bool on;float r0sq,r1sq,inv;};
static ArtFade art_fade_core[2]={{false,0,0,0},{false,0,0,0}};
#define art_fade (art_fade_core[halo_core()])
static inline void art_fade_set(float r0,float r1){art_fade={true,r0*r0,r1*r1,1.f/(r1*r1-r0*r0)};}
static inline void art_fade_off(){art_fade.on=false;}
static bool clipped(int x,int y){return x<art_clip.x || y<art_clip.y || x>=art_clip.x+art_clip.w || y>=art_clip.y+art_clip.h;}
// The damage list is a set of disjoint rectangles covering exactly the union of everything marked
// (aligned). Overlaps are merged only when the bounding box costs almost nothing extra; otherwise the new
// rectangle is cut around the existing one, so two damaged items never become one big box.
static constexpr int32_t DIRTY_MERGE_SLACK=2048;
static void dirty_add(int16_t x,int16_t y,int16_t right,int16_t bottom){
  for(uint8_t i=0;i<dirty_count;++i){auto r=dirty_rects[i];const int16_t rr=r.x+r.w,rb=r.y+r.h;
    if(!(x<rr && right>r.x && y<rb && bottom>r.y))continue; // touching boxes stay apart
    if(x>=r.x && right<=rr && y>=r.y && bottom<=rb)return;
    const int32_t l=min(x,r.x),t=min(y,r.y),R=max(right,rr),B=max(bottom,rb);
    const int32_t ox=min(right,rr)-max(x,r.x),oy=min(bottom,rb)-max(y,r.y);
    const int32_t exact=(int32_t)(right-x)*(bottom-y)+(int32_t)r.w*r.h-ox*oy;
    // Near a full list overlaps always merge (each merge removes a rectangle, so this always terminates).
    if((R-l)*(B-t)<=exact+DIRTY_MERGE_SLACK || (x<=r.x && right>=rr && y<=r.y && bottom>=rb) || dirty_count>=DIRTY_MAX-4){
      dirty_rects[i]=dirty_rects[--dirty_count];dirty_add(l,t,R,B);return;
    }
    // Cut: the parts of the new rectangle above, below, left and right of r.
    if(y<r.y)dirty_add(x,y,right,r.y);
    if(bottom>rb)dirty_add(x,rb,right,bottom);
    const int16_t my=max(y,r.y),mb=min(bottom,rb);
    if(x<r.x)dirty_add(x,my,r.x,mb);
    if(right>rr)dirty_add(rr,my,right,mb);
    return;
  }
  if(dirty_count==DIRTY_MAX){
    // Full list: merge into the nearest rectangle (smallest growth), never into one screen-sized box.
    uint8_t best=0;int32_t growth=INT32_MAX;
    for(uint8_t i=0;i<dirty_count;++i){auto r=dirty_rects[i];
      int32_t l=min(x,r.x),t=min(y,r.y),rr=max(right,(int16_t)(r.x+r.w)),bb=max(bottom,(int16_t)(r.y+r.h));
      int32_t g=(rr-l)*(bb-t)-(int32_t)r.w*r.h;if(g<growth){growth=g;best=i;}}
    auto r=dirty_rects[best];dirty_rects[best]=dirty_rects[--dirty_count];
    dirty_add(min(x,r.x),min(y,r.y),max(right,(int16_t)(r.x+r.w)),max(bottom,(int16_t)(r.y+r.h)));
    return;
  }
  dirty_rects[dirty_count++]={x,y,(int16_t)(right-x),(int16_t)(bottom-y)};
}
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
  dirty_add(x,y,right,bottom);
}
// Does the current clip rectangle touch this box? Lets a scene skip whole items per damaged rectangle.
static bool clip_hits(float x0,float y0,float x1,float y1){
  return x1>=art_clip.x && y1>=art_clip.y && x0<art_clip.x+art_clip.w && y0<art_clip.y+art_clip.h;
}
// The base is black except inside base_box (the glass edge): black rows are cleared, not copied from PSRAM.
static int16_t base_box[4]={0,0,0,0};  // x0,y0,x1,y1 of the non-black part of g_base_frame (empty = all black)
// Hot pixel loops run from internal instruction RAM on the ESP32 (no flash cache misses inside a loop).
#if defined(ESP_PLATFORM)
#define HALO_HOT IRAM_ATTR
#else
#define HALO_HOT
#endif
// ---- render target (V6.0.6.4) ----
// Every pixel the scene writes goes to halo_target: pixel (x,y) is pixels[(y-target.y)*stride+(x-target.x)].
// Each damaged rectangle gets its own contiguous buffer in a pool in internal RAM (painted, then sent from there),
// so the scene never touches PSRAM while drawing (measured on 6.0.6.3: ~210 cycles per inner-loop step).
// A rectangle that does not fit into the pool is painted in the PSRAM frame as before.
struct HaloTarget {uint16_t *pixels;int16_t x,y,stride;};
static HaloTarget halo_target_core[2]={{nullptr,0,0,LCD_W},{nullptr,0,0,LCD_W}};
#define halo_target (halo_target_core[halo_core()])
static void halo_target_frame(uint16_t *frame){halo_target={frame,0,0,LCD_W};}
// Division-free versions of mul_alpha() and blend_rgb565() with exactly the same results for every input they
// get (checked exhaustively: all 64x64x256 channel blends, all 256x256 alpha products).
static inline uint32_t halo_div255(uint32_t v){return (v+1+(v>>8))>>8;}
static inline uint8_t halo_mul(uint8_t a,uint8_t b){return (uint8_t)halo_div255((uint32_t)a*b+127);}
static inline int32_t halo_mix_channel(int32_t d,int32_t s,uint8_t a){
  const int32_t t=(s-d)*a+127;return d+(t>=0?(int32_t)halo_div255((uint32_t)t):-(int32_t)halo_div255((uint32_t)-t));
}
// V6.0.6.5: the three channels are blended at once in one 32-bit word (G moved to the upper half) with a 5-bit
// alpha: 28 instead of ~60 cycles per pixel on the board. R and B have only 32 levels in RGB565 anyway, so the
// result differs from the exact /255 blend by at most one level.
static inline uint16_t halo_blend565(uint16_t d,uint16_t s,uint8_t a){
  if(!a)return d;if(a>=252)return s;
  const uint32_t a5=((uint32_t)a+4)>>3;
  const uint32_t dw=(d|((uint32_t)d<<16))&0x07E0F81Fu,sw=(s|((uint32_t)s<<16))&0x07E0F81Fu;
  const uint32_t w=((sw*a5+dw*(32-a5))>>5)&0x07E0F81Fu;
  return (uint16_t)(w|(w>>16));
}
// Row access: the target is read once per row, not once per pixel (pixel stores may alias the global).
static inline uint16_t *halo_row(int y){return halo_target.pixels+(int32_t)(y-halo_target.y)*halo_target.stride-halo_target.x;}
static inline void halo_put(uint16_t *row,int x,uint16_t color,uint8_t alpha){
  if(!alpha)return;uint16_t &p=row[x];p=halo_blend565(p,color,alpha);
}
static inline void halo_px(int x,int y,uint16_t color,uint8_t alpha){halo_put(halo_row(y),x,color,alpha);}
// panel_order: already big-endian; internal: DMA-capable internal RAM (byte-swapped in place and sent from there)
struct RectSource {uint16_t *pixels;int16_t stride;bool panel_order,internal;};
static RectSource dirty_src[DIRTY_MAX];static bool dirty_src_valid=false;
// Rectangle buffers of one frame: the internal pool first (DMA-capable: painted, byte-swapped and sent in place),
// then a PSRAM area for a whole screen (converted on the way out), so no frame paints into g_frame.
static uint16_t *render_pool=nullptr,*render_slow=nullptr;static uint32_t render_pool_px=0,render_pool_used=0;
static constexpr uint32_t RENDER_SLOW_PX=(uint32_t)LCD_W*LCD_H+DIRTY_MAX*32u;
// Further internal regions join the pool once the UI runs (the rest of the heap, the startup's dead tables).
struct PoolRegion {uint16_t *pixels;uint32_t px,used;};
static constexpr uint8_t POOL_REGIONS_MAX=14;
static PoolRegion pool_regions[POOL_REGIONS_MAX];static uint8_t pool_region_count=0;
static void pool_add_region(void *memory,size_t bytes){
  uintptr_t a=((uintptr_t)memory+3)&~(uintptr_t)3;const size_t skip=a-(uintptr_t)memory;if(bytes<=skip)return;
  const uint32_t px=(uint32_t)((bytes-skip)/sizeof(uint16_t))&~3u;
  if(px>=512 && pool_region_count<POOL_REGIONS_MAX)pool_regions[pool_region_count++]={(uint16_t*)a,px,0};
}
static uint32_t pool_capacity(){uint32_t n=0;for(uint8_t k=0;k<pool_region_count;++k)n+=pool_regions[k].px;return n;}
static void assign_sources(){
  if(!pool_region_count && render_pool)pool_add_region(render_pool,render_pool_px*sizeof(uint16_t));
  for(uint8_t k=0;k<pool_region_count;++k)pool_regions[k].used=0;
  uint32_t fast=0,slow=0;
  for(uint8_t i=0;i<dirty_count;++i){auto r=dirty_rects[i];const uint32_t px=(uint32_t)r.w*r.h;
    // Best fit: the internal region with the least room left that still takes the rectangle.
    int best=-1;for(uint8_t k=0;k<pool_region_count;++k){const auto &g=pool_regions[k];
      if(g.used+px<=g.px && (best<0 || g.px-g.used<pool_regions[best].px-pool_regions[best].used))best=k;}
    if(best>=0){auto &g=pool_regions[best];dirty_src[i]={g.pixels+g.used,r.w,false,true};g.used+=px;fast+=px;}
    else if(render_slow && slow+px<=RENDER_SLOW_PX){dirty_src[i]={render_slow+slow,r.w,false,false};slow+=(px+31)&~31u;}
    else dirty_src[i]={g_frame+(size_t)r.y*LCD_W+r.x,LCD_W,false,false};}
  render_pool_used=fast+slow;dirty_src_valid=true;
}
static void paint_begin();
// Internal RAM pool and the PSRAM overflow area (allocated once in setup, never in the render loop).
static void render_pool_begin(){
#if defined(ESP_PLATFORM)
  for(uint32_t px:{80000u,72000u,64000u,48000u,32000u,16000u}){
    render_pool=(uint16_t*)heap_caps_aligned_alloc(16,px*sizeof(uint16_t),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT|MALLOC_CAP_DMA);
    if(render_pool){render_pool_px=px;break;}
  }
  render_slow=(uint16_t*)heap_caps_aligned_alloc(64,RENDER_SLOW_PX*sizeof(uint16_t),MALLOC_CAP_SPIRAM);
  paint_begin();  // the startup paints on both cores too
#endif
  halo_target_frame(g_frame);
}
// V6.0.6.5: painted pool rectangles are turned into the panel's byte order in place (one pass over internal
// RAM), so the DMA sends them straight from the pool without a CPU conversion during the transfer.
static void panel_order_rect(uint8_t i){
#if defined(FAST_QSPI) && FAST_QSPI
  auto &s=dirty_src[i];auto r=dirty_rects[i];
  if(!fast_qspi || s.stride!=r.w || !s.internal)return;  // only internal buffers (PSRAM: converted on the way)
  uint32_t *w=(uint32_t*)s.pixels;const uint32_t n=((uint32_t)r.w*r.h)/2;
  for(uint32_t k=0;k<n;++k){const uint32_t v=w[k];w[k]=((v&0x00FF00FFu)<<8)|((v>>8)&0x00FF00FFu);}
  s.panel_order=true;
#endif
}
// ---- TE-locked pacing ----
// V6.0.6.1: a frame starts as soon as the previous one is on the glass (the whole period is available to draw),
// and its scene is sampled for the first TE edge it can make. The draw estimate is a plain average (one slow
// frame must not hold the next ones back — that made 6.0.6 wait ~7 ms per frame for TE on the board).
#ifndef HALO_COUNT
#define HALO_COUNT(kind) ((void)0)  // host tests count inner-loop pixel work here
#endif
#ifndef FRAME_SPIN
#define FRAME_SPIN() ((void)0)  // host tests advance their clock here
#endif
static constexpr uint32_t TE_NOMINAL_US=16667,FRAME_MARGIN_US=2000;
static uint32_t te_last_us=0,te_period_us=TE_NOMINAL_US,draw_est_us=8000;
static uint32_t frame_target_us=0;  // the TE edge the current/last frame is made for
static bool te_known=false;
// Development frame trace (Serial 'T'): the next frames one by one, printed when complete.
struct TraceFrame {uint32_t start,draw,te,qspi,px;uint8_t rects;uint16_t prof[8],update,restore,paint,pool;uint32_t ring_steps,queue_us,fails;};
static constexpr int TRACE_FRAMES=150;
static TraceFrame trace_buf[TRACE_FRAMES];static int trace_used=0,trace_left=0;static uint32_t trace_armed_at=0;
struct FramePerf {uint32_t frames=0,draw=0,draw_max=0,te=0,qspi=0,pixels=0,rects=0,late=0,last=0,interval=0,intervals=0,update=0,restore=0,paint=0,work=0,pool=0;uint32_t prof[8]={0,0,0,0,0,0,0,0};};
static uint32_t ui_t_update=0,ui_t_restore=0,ui_t_paint=0,ui_work=0,ui_t_warm=0;  // the last UI frame, for the report
static FramePerf ui_perf;
// ---- the transfer ----
// V6.0.6.5 (board, queued transfer): the UI rectangles are sorted top to bottom and each goes out as soon as it
// is painted and the frame's TE edge has come, while both cores still paint the rest. The panel scans from the
// top from that edge on and the transfer stays ahead of it (painting starts before the edge, the DMA is faster
// than the scan). Never earlier than the edge the scene was sampled for, exactly as before. Without the queued
// device (host tests, FAST_QSPI 0) the frame is sent after painting.
static uint8_t send_count=0,send_next=0;static volatile uint8_t rect_painted[DIRTY_MAX];
static volatile uint32_t send_lock=0;static uint32_t send_cycles=0;  // development: CPU time spent queueing
static volatile bool send_painting=false;static bool send_armed=false,send_synced=false;static uint32_t send_guard=0,send_synced_us=0;
static inline bool queued_transfer(){
#if defined(ESP_PLATFORM) && defined(FAST_QSPI) && FAST_QSPI
  return qspi_dma!=nullptr;
#else
  return false;
#endif
}
static inline void transfer_wait_idle(){
#if defined(ESP_PLATFORM) && defined(FAST_QSPI) && FAST_QSPI
  qspi_wait_idle();
#endif
}
// The frame's rectangles are final and about to be painted.
static void send_begin(uint32_t started){
  send_count=dirty_count;send_next=0;send_synced=false;send_armed=true;
  for(uint8_t i=0;i<dirty_count;++i)rect_painted[i]=0;
  send_guard=te_known?frame_target_us-te_period_us/2:started;
}
// Queue every painted rectangle in order once the edge is there (either core; one at a time).
static void send_advance(){
#if defined(ESP_PLATFORM) && defined(FAST_QSPI) && FAST_QSPI
  if(!qspi_dma || !send_armed || __atomic_exchange_n(&send_lock,1u,__ATOMIC_ACQUIRE))return;
  if(!send_synced && (int32_t)(te_edge_us-send_guard)>=0){send_synced=true;send_synced_us=te_edge_us;}
  if(send_synced && qspi_sct && !qspi_sct_split)for(;;){
    // Consecutive painted panel-order windows in one segmented transaction.
    QxRect batch[DIRTY_MAX];uint8_t n=0;uint16_t segs=0;
    while(send_next+n<send_count && rect_painted[send_next+n]){const DirtyRect r=dirty_rects[send_next+n];const RectSource s=dirty_src[send_next+n];
      if(!s.panel_order || s.stride!=r.w)break;const uint16_t k=sct_needed(r.w,r.h);if(n && segs+k>SCT_BATCH_SEGMENTS)break;
      batch[n++]={r.x,r.y,r.w,r.h,s.pixels};segs+=k;}
    if(!n)break;
    const uint32_t c0=esp_cpu_get_cycle_count();const bool queued=sct_push_batch(batch,n);send_cycles+=esp_cpu_get_cycle_count()-c0;
    if(!queued)break;  // no room now: the next call goes on
    send_next+=n;}
  if(send_synced)while(send_next<send_count && rect_painted[send_next]){
    const DirtyRect r=dirty_rects[send_next];const RectSource s=dirty_src[send_next];
    if(qspi_sct && !qspi_sct_split && s.panel_order && s.stride==r.w)break;  // batched above (no room yet)
    if(!s.panel_order && send_painting)break;  // converted on the way (PSRAM): after painting, not inside a paint
    if(!qx_room(r.w,r.h))break;  // never wait for the DMA inside a paint: the next call goes on
    const uint32_t c0=esp_cpu_get_cycle_count();qx_push_rect(r.x,r.y,r.w,r.h,s.pixels,s.stride,s.panel_order);send_cycles+=esp_cpu_get_cycle_count()-c0;++send_next;}
  __atomic_store_n(&send_lock,0u,__ATOMIC_RELEASE);
#endif
}
static void push_dirty(uint32_t started){
  if(!dirty_count){send_armed=false;return;}
  uint32_t ready=micros();render_stats.draw_us=ready-started;
  draw_est_us=(uint32_t)((int32_t)draw_est_us+((int32_t)min((uint32_t)render_stats.draw_us,(uint32_t)50000u)-(int32_t)draw_est_us)/8);
  uint32_t synced,pixels=0;
  for(uint8_t i=0;i<dirty_count;++i)pixels+=(uint32_t)dirty_rects[i].w*dirty_rects[i].h;
  if(queued_transfer() && dirty_src_valid){
    if(!send_armed){send_begin(started);for(uint8_t i=0;i<dirty_count;++i)rect_painted[i]=1;}  // painted before (startup)
    while(send_next<send_count){
      send_advance();
      if(!send_synced && (int32_t)(micros()-send_guard)>(int32_t)(2*te_period_us+20000)){send_synced=true;send_synced_us=micros();}  // no TE
    }
    synced=send_synced_us;
  }else{
    // Never show the frame on an earlier edge than the one its scene was sampled for.
    if(te_known)while((int32_t)(micros()-(frame_target_us-te_period_us/2))<0){FRAME_SPIN();}
    wait_for_te_edge();synced=micros();
    for(uint8_t i=0;i<dirty_count;++i){auto r=dirty_rects[i];
      const RectSource s=dirty_src_valid?dirty_src[i]:RectSource{g_frame+(size_t)r.y*LCD_W+r.x,LCD_W,false,false};
#if defined(FAST_QSPI) && FAST_QSPI
      if(fast_qspi)fast_push_rect(r.x,r.y,r.w,r.h,s.pixels,s.stride,s.panel_order);else
#endif
      if(s.stride==LCD_W)push_frame_rect(r.x,r.y,r.w,r.h,false);else push_bitmap(r.x,r.y,s.pixels,r.w,r.h);}
  }
#if SHADOW_CHECK
  // Development: what went to the panel, in the CPU byte order (g_push_buffer is not used otherwise).
  {static bool cleared=false;if(g_push_buffer && !cleared){memset(g_push_buffer,0,PUSH_BYTES);cleared=true;}}
  if(g_push_buffer)for(uint8_t i=0;i<dirty_count;++i){const auto r=dirty_rects[i];
    const RectSource s=dirty_src_valid?dirty_src[i]:RectSource{g_frame+(size_t)r.y*LCD_W+r.x,LCD_W,false,false};
    for(int16_t y=0;y<r.h;++y)for(int16_t x=0;x<r.w;++x){uint16_t v=s.pixels[(size_t)y*s.stride+x];if(s.panel_order)v=(uint16_t)((v>>8)|(v<<8));
      g_push_buffer[(size_t)(r.y+y)*LCD_W+r.x+x]=v;}}
#endif
  send_armed=false;dirty_src_valid=false;
  render_stats.te_us=(int32_t)(synced-ready)>0?synced-ready:0;
  if(te_known){
    // Learn the real panel period from edges one or a few frames apart.
    uint32_t gap=synced-te_last_us,n=(gap+te_period_us/2)/te_period_us;
    if(n>=1 && n<=4){uint32_t p=gap/n;if(p>12000 && p<22000)te_period_us+=((int32_t)p-(int32_t)te_period_us)/8;}
    if((int32_t)(synced-frame_target_us)>(int32_t)(te_period_us/2))++ui_perf.late; // shown later than sampled
  }
  te_last_us=synced;te_known=true;frame_target_us=synced;
  render_stats.pixels=pixels;
  uint32_t ended=micros();render_stats.transfer_us=ended-synced;render_stats.total_us=ended-started;++render_stats.frames;
  render_stats.max_draw_us=max(render_stats.max_draw_us,render_stats.draw_us);
  render_stats.max_te_us=max(render_stats.max_te_us,render_stats.te_us);
  render_stats.max_transfer_us=max(render_stats.max_transfer_us,render_stats.transfer_us);
  render_stats.max_total_us=max(render_stats.max_total_us,render_stats.total_us);
  render_stats.max_pixels=max(render_stats.max_pixels,render_stats.pixels);
}
static bool frame_due(uint32_t us,uint32_t &present){
  if(!te_known){present=us;frame_target_us=us;return true;}
  const uint32_t P=te_period_us;
  // The next edge no frame was made for yet (frame_target may lie a few edges after te_last after idle frames).
  const int32_t ahead=(int32_t)(frame_target_us-te_last_us);
  const uint32_t next=te_last_us+(ahead>0?((uint32_t)ahead+P/2)/P*P:0)+P;
  if((int32_t)(us-(next-P))<0)return false;  // the previous target edge has not even passed
  // V6.0.6.5: always aim at the next edge: the whole period is there to draw. (Choosing a later edge from an
  // average draw time skipped edges the frame would have made — measured: ~12 ms TE waits at ~13 ms of work.)
  // Only a frame that starts within 1 ms of that edge takes the following one.
  const uint32_t reach=us+1000;
  present=(int32_t)(reach-next)<=0?next:te_last_us+((reach-te_last_us+P-1)/P)*P;
  frame_target_us=present;return true;
}

// Spans are sorted by offset (row by row): a first-span index per row lets a rectangle visit only its rows.
static constexpr uint16_t OUTLINE_MAX_ROWS=256;
static const OutlineAsset *outline_indexed=nullptr;static uint16_t outline_rows[OUTLINE_MAX_ROWS+1];
static void outline_index(const OutlineAsset &asset){
  if(outline_indexed==&asset || asset.height>OUTLINE_MAX_ROWS)return;
  uint16_t n=0;
  for(uint16_t row=0;row<=asset.height;++row){
    while(n<asset.count){AssetSpan span;memcpy_P(&span,asset.spans+n,sizeof(span));if(span.offset/asset.width>=row)break;++n;}
    outline_rows[row]=n;
  }
  outline_indexed=&asset;
}
static void HALO_HOT outline(uint16_t *target,const OutlineAsset &asset,int16_t cx,int16_t cy,uint16_t color=WHITE,uint8_t opacity=230){
  if(!opacity)return;
  int16_t ox=cx-asset.width/2,oy=cy-asset.height/2;
  if(ox>=art_clip.x+art_clip.w || oy>=art_clip.y+art_clip.h || ox+asset.width<=art_clip.x || oy+asset.height<=art_clip.y)return;
  uint16_t from=0,to=asset.count;
  if(asset.height<=OUTLINE_MAX_ROWS){
    if(outline_indexed!=&asset){  // built once in render_pool_begin() for ART_TOP, before any parallel paint
      uint16_t n=0;
      for(uint16_t row=0;row<=asset.height;++row){
        while(n<asset.count){AssetSpan span;memcpy_P(&span,asset.spans+n,sizeof(span));if(span.offset/asset.width>=row)break;++n;}
        outline_rows[row]=n;
      }
      outline_indexed=&asset;
    }
    from=outline_rows[clamp_i32(art_clip.y-oy,0,asset.height)];to=outline_rows[clamp_i32(art_clip.y+art_clip.h-oy,0,asset.height)];
  }
  for(uint16_t n=from;n<to;++n){AssetSpan span;memcpy_P(&span,asset.spans+n,sizeof(span));
    int16_t x=ox+span.offset%asset.width,y=oy+span.offset/asset.width;
    if(y<art_clip.y || y>=art_clip.y+art_clip.h)continue;
    uint8_t alpha=halo_mul(span.alpha,opacity);
    HALO_COUNT(4);uint16_t *row=halo_row(y);
    for(uint8_t j=0;j<span.length;++j)if(!clipped(x+j,y))halo_put(row,x+j,color,alpha);
  }
}

#include "FastCoverage.h"
// x range of row `py` (relative to the start point) that can lie within `outer` of the segment (0,0)-(dx,dy):
// the capsule is convex, so its row is one interval = union of the two end caps and the band between them.
// Square root for run bounds only (runs are padded by a pixel; the per-pixel test stays exact): reciprocal
// square root bit trick + two Newton steps, ~20 cycles instead of ~85 for sqrtf on the board.
static inline float halo_fast_rsqrt(float v){
 uint32_t bits;memcpy(&bits,&v,4);bits=0x5f3759dfU-(bits>>1);float inv;memcpy(&inv,&bits,4);
 const float h=.5f*v;inv*=1.5f-h*inv*inv;inv*=1.5f-h*inv*inv;return inv;
}
// floor/ceil without the library call (~20-40 cycles each on the board, four per line).
static inline int halo_floor(float v){const int i=(int)v;return i-(v<i);}
static inline int halo_ceil(float v){const int i=(int)v;return i+(v>i);}
static inline float halo_fast_sqrt(float v){
 if(v<=0)return 0;uint32_t bits;memcpy(&bits,&v,4);bits=0x5f3759dfU-(bits>>1);float inv;memcpy(&inv,&bits,4);
 const float h=.5f*v;inv*=1.5f-h*inv*inv;inv*=1.5f-h*inv*inv;return v*inv;
}
// Per-line constants for the row runs: the divisions and the normal are computed once per line, not per row.
struct LineGeo {float dx,dy,len2,outer,outer2,nx,ny,inv_nx,inv_dx;bool band,has_nx,has_dx;};
static inline LineGeo line_geo(float dx,float dy,float len2,float outer,float outer2){
 LineGeo g{dx,dy,len2,outer,outer2,0,0,0,0,len2>.001f,false,false};
 if(g.band){const float inv_len=1.f/sqrtf(len2);g.nx=-dy*inv_len;g.ny=dx*inv_len;
  g.has_nx=fabsf(g.nx)>1e-6f;if(g.has_nx)g.inv_nx=1.f/g.nx;g.has_dx=fabsf(dx)>1e-6f;if(g.has_dx)g.inv_dx=1.f/dx;}
 return g;
}
static bool HALO_HOT line_row_span(const LineGeo &g,float py,float &lo,float &hi){
 lo=1e9f;hi=-1e9f;
 for(int e=0;e<2;++e){const float cx=e?g.dx:0,ey=py-(e?g.dy:0),r2=g.outer2-ey*ey;if(r2>0){const float w=halo_fast_sqrt(r2)*1.001f+.01f;lo=min(lo,cx-w);hi=max(hi,cx+w);}}
 if(g.band){
  float a=-1e9f,b=1e9f;
  if(g.has_nx){float u=(-g.outer-g.ny*py)*g.inv_nx,v=(g.outer-g.ny*py)*g.inv_nx;a=max(a,min(u,v));b=min(b,max(u,v));}
  else if(fabsf(g.ny*py)>g.outer)b=-1e9f;
  if(g.has_dx){float u=(0-g.dy*py)*g.inv_dx,v=(g.len2-g.dy*py)*g.inv_dx;a=max(a,min(u,v));b=min(b,max(u,v));}
  else if(g.dy*py<0 || g.dy*py>g.len2)b=-1e9f;
  if(a<=b){lo=min(lo,a);hi=max(hi,b);}
 }
 return lo<=hi;
}
static uint32_t art_line_calls_total=0;
static void HALO_HOT art_line(uint16_t *dst,float x0,float y0,float x1,float y1,float radius,uint16_t color,uint8_t alpha=255){
 ++art_line_calls_total;
 if(!alpha)return;
 float dx=x1-x0,dy=y1-y0,len2=dx*dx+dy*dy;
 const float outer=radius+.5f,outer2=outer*outer,inner=max(0.f,radius-.5f),inner2=inner*inner;
 int left=halo_floor(min(x0,x1)-radius-1),right=halo_ceil(max(x0,x1)+radius+1);
 int top=halo_floor(min(y0,y1)-radius-1),bottom=halo_ceil(max(y0,y1)+radius+1);
 if(right<art_clip.x || bottom<art_clip.y || left>=art_clip.x+art_clip.w || top>=art_clip.y+art_clip.h)return;
 const ArtFade fade=art_fade;
 if(fade.on){  // the whole line beyond the fade: nothing to draw
  const float cx=233-x0,cy=233-y0,t=len2>.001f?clamp_float((cx*dx+cy*dy)/len2,0,1):0,ex=cx-t*dx,ey=cy-t*dy,near=sqrtf(ex*ex+ey*ey)-outer;
  if(near>0 && near*near>=fade.r1sq)return;
 }
 // One reciprocal square root per line gives both 1/len (the normal) and 1/len^2 (the projection).
 const bool band=len2>.001f;const float inv_len=band?halo_fast_rsqrt(len2):0.f,inv_len2=inv_len*inv_len;
 const int xmin=max((int)art_clip.x,left),xmax=min((int)(art_clip.x+art_clip.w-1),right);
 // Row runs pay off only when the box is much larger than the line (long diagonals, zoom); tiny segments
 // (the laser outline has a thousand of them) keep the plain box loop.
#ifndef LINE_RUN_MIN
#define LINE_RUN_MIN 12
#endif
 const bool long_line=(xmax-xmin)>LINE_RUN_MIN;
 LineGeo geo;if(long_line)geo=line_geo(dx,dy,len2,outer,outer2);
 // V6.0.6.5: between the two end caps the distance to the segment is the distance to its line, which changes
 // linearly along a row: no sqrt and no coverage call for those pixels (measured on the board: the coverage
 // call was ~180 of the ~190 cycles per pixel). Only pixels beyond the ends use the exact cap distance.
 const float nx=-dy*inv_len,ny=dx*inv_len;
 const float t_step=dx*inv_len2;
 // V6.0.6.3: per row only the run the line can touch (a long diagonal used to visit its whole bounding box);
 // the per-pixel test is unchanged, so the pixels are identical.
 for(int y=max((int)art_clip.y,top);y<=min((int)(art_clip.y+art_clip.h-1),bottom);++y){
  int xa=xmin,xb=xmax;
  if(long_line){float lo,hi;if(!line_row_span(geo,y+.5f-y0,lo,hi))continue;
   xa=max(xmin,(int)floorf(x0+lo-.5f)-1);xb=min(xmax,(int)ceilf(x0+hi-.5f)+1);}
  const float py=y+.5f-y0;float px=xa+.5f-x0,t=(px*dx+py*dy)*inv_len2,d=px*nx+py*ny;uint16_t *row=halo_row(y);
  const float fy=y+.5f-233,fy2=fy*fy;
  for(int x=xa;x<=xb;++x,px+=1,t+=t_step,d+=nx){
  HALO_COUNT(0);
  uint8_t coverage;
  if(band && t>=0 && t<=1){
   const float a=fabsf(d);if(a>=outer)continue;
   coverage=(radius>=.5f && a<=inner)?255:(uint8_t)(int)((outer-a)*255+.5f);
  }else{
   const float tc=clamp_float(t,0,1),ex=px-tc*dx,ey=py-tc*dy,distance2=ex*ex+ey*ey;
   if(distance2>=outer2)continue;
   // Only the antialiased edge needs sqrt; interior and exterior are exact.
   coverage=255;
   if(radius<.5f || distance2>inner2)coverage=fast_aa_coverage(distance2,outer);
  }
  if(fade.on){const float fx=x+.5f-233,r2=fx*fx+fy2;
   if(r2>fade.r0sq){if(r2>=fade.r1sq)continue;coverage=(uint8_t)(coverage*((fade.r1sq-r2)*fade.inv));}}
  HALO_COUNT(6);
  halo_put(row,x,color,halo_mul(alpha,coverage));
  }
 }
}
static void art_dot(uint16_t *dst,float x,float y,float radius,uint16_t color,uint8_t alpha=255){art_line(dst,x,y,x,y,radius,color,alpha);}
#include "HaloDraw.h"
// Bounding boxes of the Titan side paths, 16 segments per box (model space): a rectangle skips whole parts.
static constexpr int ART_CHUNK=16,ART_CHUNKS=8;
static HaloBox art_path_boxes[6][ART_CHUNKS];static bool art_boxes_ready=false;
static const HaloBox *art_chunks(int index){
 if(!art_boxes_ready){
  for(int i=0;i<6;++i){const auto &p=ART_PATHS[i];
   for(int c=0;c*ART_CHUNK+1<p.count && c<ART_CHUNKS;++c){HaloBox b;
    for(int k=c*ART_CHUNK;k<=min((int)p.count-1,c*ART_CHUNK+ART_CHUNK);++k)b.add(p.points[k].x,p.points[k].y);
    art_path_boxes[i][c]=b;}}
  art_boxes_ready=true;
 }
 return art_path_boxes[index];
}
static void HALO_HOT path_draw(uint16_t *dst,int index,float progress,uint8_t alpha=255,bool dot=false){
 const auto &p=ART_PATHS[index];float amount=clamp_float(progress,0,1)*(p.count-1);
 const HaloBox *boxes=art_chunks(index);const int segments=(int)ceilf(amount);
 for(int c=0;c*ART_CHUNK<segments;++c){
  const auto &bx=boxes[c];if(!clip_hits(bx.x0-3,bx.y0-3,bx.x1+3,bx.y1+3))continue;
  for(int i=c*ART_CHUNK;i<min(segments,c*ART_CHUNK+ART_CHUNK);++i){
   auto a=p.points[i],b=p.points[i+1];float t=min(1.f,amount-i);
   // A finished segment ends exactly on its point, so the finished Titan equals the UI body pixel for pixel.
   if(t<1)b={a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t};
   art_line(dst,a.x,a.y,b.x,b.y,.85f,WHITE,alpha);
  }
 }
 if(dot && amount>0 && progress<1){int i=min((int)amount,(int)p.count-2);auto a=p.points[i],b=p.points[i+1];float t=amount-i;
  art_dot(dst,a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,2,WHITE,alpha);
 }
}
// Static layer of the UI: black plus a faint glass edge along the top of the bezel. V6.0.6.5: the glass edge is
// the first item of the scene (the same arc over black, so the same pixels) and the base is plain black: clearing
// a rectangle no longer reads the base frame from PSRAM (~20 cycles per pixel measured on the board).
static constexpr float GLASS_A0=205,GLASS_A1=335;
static inline void ui_glass(){art_arc(g_frame,233,233,208,GLASS_A0,GLASS_A1,.5f,WHITE,15);}
#include "HaloStartup.h"
#include "SignaturePerf.h"
#include "HaloUi.h"
static void build_base(){base_box[0]=base_box[1]=base_box[2]=base_box[3]=0;}
// Once, when the UI starts (board only; host tests restart the startup): what is left of the internal heap
// beyond a reserve, and the startup's geometry tables, which nothing reads after the startup (it never runs again
// on the board), become further pool regions. Not an allocation in the render loop: one block, once.
static void pool_lend_regions(){
#if defined(ESP_PLATFORM)
  static bool lent=false;if(lent)return;lent=true;
  if(!pool_region_count && render_pool)pool_add_region(render_pool,render_pool_px*sizeof(uint16_t));
  const size_t reserve=48*1024,free_now=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  size_t want=min(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA),free_now>reserve?free_now-reserve:0);
  if(want>=8192){void *p=heap_caps_malloc(want&~(size_t)15,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA|MALLOC_CAP_8BIT);if(p)pool_add_region(p,want&~(size_t)15);}
  pool_add_region(dist_at,sizeof(dist_at));pool_add_region(tour_pts,sizeof(tour_pts));pool_add_region(tour_cum,sizeof(tour_cum));
  pool_add_region(inf_pts,sizeof(inf_pts));pool_add_region(track_a_pts,sizeof(track_a_pts));pool_add_region(track_b_pts,sizeof(track_b_pts));
  pool_add_region(track_a_cum,sizeof(track_a_cum));pool_add_region(track_b_cum,sizeof(track_b_cum));
  pool_add_region(half_a_cum,sizeof(half_a_cum));pool_add_region(half_b_cum,sizeof(half_b_cum));
#endif
}
// ---- painting on both cores ----
// Paint jobs: a damaged rectangle, or a band of rows of a large one, so both cores can share it. A job clears its
// rows to the base and paints the scene clipped to them; the last band of a rectangle byte-swaps the whole
// rectangle and hands it to the transfer.
// Development rectangle trace (Serial 'R'): the cost of every rectangle of the next frame with >= 24 of them.
struct RectCost {DirtyRect r;uint32_t restore,paint,order;uint8_t core;};
static RectCost rect_cost[DIRTY_MAX];static uint8_t rect_cost_count=0;static volatile bool rect_trace_armed=false,rect_trace_ready=false;static bool rect_trace_on=false;
struct PaintJob {uint8_t rect;int16_t y0,y1;};
static PaintJob paint_jobs[DIRTY_MAX*4];static uint8_t paint_job_count=0;static volatile uint32_t rect_bands_left[DIRTY_MAX];
static bool paint_startup=false;static uint32_t paint_elapsed=0;  // the jobs paint the startup scene (else the UI)
static void build_paint_jobs(uint32_t split_px,uint32_t max_bands){
  paint_job_count=0;
  for(uint8_t i=0;i<dirty_count;++i){const auto r=dirty_rects[i];const uint32_t px=(uint32_t)r.w*r.h;
    const uint32_t bands=px>split_px?min(max_bands,min((uint32_t)(r.h/2),px/split_px+1)):1;uint32_t made=0;
    for(uint32_t b=0;b<bands;++b){
      const int16_t y0=r.y+(int16_t)(((uint32_t)r.h*b/bands)&~1u),y1=b+1==bands?r.y+r.h:r.y+(int16_t)(((uint32_t)r.h*(b+1)/bands)&~1u);
      if(y1>y0){paint_jobs[paint_job_count++]={i,y0,y1};++made;}}
    rect_bands_left[i]=made;}
}
static void restore_rows(uint8_t i,int16_t y0,int16_t y1){
  const auto r=dirty_rects[i];const auto s=dirty_src[i];
  const bool touches=r.x<base_box[2] && r.x+r.w>base_box[0] && y0<base_box[3] && y1>base_box[1];
  for(int16_t y=y0;y<y1;++y){uint16_t *line=s.pixels+(size_t)(y-r.y)*s.stride;
    if(touches)memcpy(line,g_base_frame+(size_t)y*LCD_W+r.x,(size_t)r.w*sizeof(uint16_t));
    else memset(line,0,(size_t)r.w*sizeof(uint16_t));}
}
// Development job trace (Serial 'J'): start/end of every job of the next frame with >= 2 jobs, per core.
struct JobCost {uint32_t start,end;int16_t y0,y1;uint8_t core;};
static JobCost job_cost[DIRTY_MAX*4];static volatile bool job_trace_armed=false,job_trace_ready=false;static bool job_trace_on=false;static uint32_t job_trace_t0=0;static uint8_t job_trace_count=0;
static void paint_job(const PaintJob &j,int32_t now){
#if defined(ESP_PLATFORM)
  const uint32_t a=rect_trace_on?esp_cpu_get_cycle_count():0;const uint32_t js=job_trace_on?micros():0;
#endif
  const auto r=dirty_rects[j.rect];const auto s=dirty_src[j.rect];
  restore_rows(j.rect,j.y0,j.y1);
#if defined(ESP_PLATFORM)
  const uint32_t b=rect_trace_on?esp_cpu_get_cycle_count():0;
#endif
  halo_target={s.pixels+(size_t)(j.y0-r.y)*s.stride,r.x,j.y0,s.stride};art_clip={r.x,j.y0,r.w,(int16_t)(j.y1-j.y0)};
  if(paint_startup)render_startup_scene(paint_elapsed);else ui_paint(now);
#if defined(ESP_PLATFORM)
  const uint32_t c=rect_trace_on?esp_cpu_get_cycle_count():0;
#endif
  if(__atomic_sub_fetch(&rect_bands_left[j.rect],1u,__ATOMIC_ACQ_REL)==0){
    panel_order_rect(j.rect);
#if defined(ESP_PLATFORM)
    if(rect_trace_on){const uint32_t d=esp_cpu_get_cycle_count();rect_cost[j.rect]={r,b-a,c-b,d-c,(uint8_t)halo_core()};}
#endif
    rect_painted[j.rect]=1;send_advance();}
#if defined(ESP_PLATFORM)
  if(job_trace_on){job_cost[&j-paint_jobs]={js-job_trace_t0,micros()-job_trace_t0,j.y0,j.y1,(uint8_t)halo_core()};}
#endif
}
// The loop core and a paint task on core 0 (below the CAN worker's priority) take the jobs from one shared counter
// (in list order: top to bottom, the order they go out in), so they finish together. Everything a paint may cache
// is filled first by one pass with an empty clip, so during the parallel part both cores only read shared data.
#if defined(ESP_PLATFORM)
static TaskHandle_t paint_task=nullptr,paint_waiter=nullptr;
static int32_t paint_now=0;static volatile uint32_t paint_next=0;
static void paint_take(){for(;;){const uint32_t k=__atomic_fetch_add(&paint_next,1,__ATOMIC_RELAXED);if(k>=paint_job_count)return;paint_job(paint_jobs[k],paint_now);}}
static void paint_worker(void*){
 for(;;){
  ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
  paint_take();
  xTaskNotifyGive(paint_waiter);
 }
}
static void paint_begin(){
 arc_lut_prepare();
 if(!paint_task)xTaskCreatePinnedToCore(paint_worker,"paint",8192,nullptr,1,&paint_task,0);
}
#else
static void paint_begin(){}
#endif
static void paint_jobs_on_cores(int32_t now){
#if defined(ESP_PLATFORM)
 job_trace_on=job_trace_armed && !job_trace_ready && paint_job_count>=2;if(job_trace_on){job_trace_t0=micros();job_trace_count=paint_job_count;}
 if(paint_task && paint_job_count>=2 && halo_core()==1){
  const uint32_t w0=micros();art_clip={0,0,0,0};  // warm every cache this frame needs (nothing is drawn)
  if(paint_startup)render_startup_scene(paint_elapsed);else ui_paint(now);
  ui_t_warm=micros()-w0;
  paint_now=now;paint_next=0;paint_waiter=xTaskGetCurrentTaskHandle();
  xTaskNotifyGive(paint_task);
  paint_take();
  ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
  if(job_trace_on){job_trace_on=false;job_trace_armed=false;job_trace_ready=true;}
  return;
 }
#endif
 for(uint8_t k=0;k<paint_job_count;++k)paint_job(paint_jobs[k],now);
}
static void paint_ui_rects(int32_t now){
 build_paint_jobs(16000,2);  // large rectangles (the car) in two bands: both cores share them
 send_painting=true;paint_jobs_on_cores(now);send_painting=false;
}
static void render_startup(uint32_t now){
 transfer_wait_idle();  // the previous frame's DMA reads the pool until here
 uint32_t elapsed=now-startup_started;
 base_box[0]=base_box[1]=base_box[2]=base_box[3]=0;  // the startup is drawn on black
 startup_mark_damage(elapsed);
 assign_sources();
 // V6.0.6.5: on both cores too (the logo is one ~36 k px rectangle: in bands of rows), the same pixels.
 build_paint_jobs(4500,8);paint_startup=true;paint_elapsed=elapsed;paint_jobs_on_cores((int32_t)now);paint_startup=false;
 art_clip={0,0,LCD_W,LCD_H};halo_target_frame(g_frame);
 if(elapsed>=startup_total_ms){startup_active=false;base_invalid=true;}
}
static void render_ui(uint32_t now){
 if(base_invalid){
  // Entering the UI (after the Titan, or a skipped startup): the static layer and every item once.
  build_base();base_invalid=false;ui_reset_damage();
  // After the Titan the panel already shows the first UI frame (verified by the host test): only the new glass
  // edge of the base has to appear. A skipped startup leaves anything behind: then the whole screen once.
  if(startup_skipped){dirty(0,0,LCD_W,LCD_H);startup_skipped=false;}
  else{damage_arc(233,233,208,GLASS_A0,GLASS_A1,3);damage_adopt=true;}
  ui_begin_scene(now);outline_index(ART_TOP);paint_begin();  // shared tables before any parallel paint
  pool_lend_regions();
 }
 const uint32_t t0=micros();
#ifdef HALO_WORK_COUNTER
 const uint32_t w0=halo_work;
#endif
 ui_update(now);ui_mark_damage(now);damage_adopt=false;
 const uint32_t t1=micros();
 // Top to bottom (the order the rectangles go out in while painting): insertion sort by y, then x.
 for(uint8_t i=1;i<dirty_count;++i){const DirtyRect v=dirty_rects[i];int j=i;
  while(j>0 && (dirty_rects[j-1].y>v.y || (dirty_rects[j-1].y==v.y && dirty_rects[j-1].x>v.x))){dirty_rects[j]=dirty_rects[j-1];--j;}dirty_rects[j]=v;}
 transfer_wait_idle();  // the previous frame's DMA reads the pool until here
 assign_sources();  // the rectangle list is final now; each core clears and converts its own rectangles
 if(dirty_count)send_begin(micros());
 rect_trace_on=rect_trace_armed && !rect_trace_ready && dirty_count>=24;
 const uint32_t t2=micros();
 paint_ui_rects(now);
 halo_target_frame(g_frame);
 if(rect_trace_on){rect_cost_count=dirty_count;rect_trace_on=false;rect_trace_armed=false;rect_trace_ready=true;}
 const uint32_t t3=micros();ui_t_update=t1-t0;ui_t_restore=t2-t1;ui_t_paint=t3-t2;
#ifdef HALO_WORK_COUNTER
 ui_work=halo_work-w0;
#endif
 art_clip={0,0,LCD_W,LCD_H};
}
// The scene is sampled at the moment it will be on the glass (the TE edge it is pushed on), so motion steps
// stay perfectly even even when the loop starts a frame a little early or late.
static void render_tick(){
 uint32_t us=micros(),present;
 if(!frame_due(us,present))return;
 const uint32_t now=millis()+((uint32_t)max((int32_t)0,(int32_t)(present-us))+500)/1000;dirty_count=0;frame_now_ms=now;
 if(startup_active){
  const uint8_t stage=startup_stage(now-startup_started);
  render_startup(now);push_dirty(us);
#if SHADOW_CHECK
  {static bool checked=false;const uint32_t e=now-startup_started;
   if(!checked && stage==1 && e>startup_draw_ms+300 && g_push_buffer){checked=true;
    memset(g_frame,0,FRAME_BYTES);halo_target_frame(g_frame);art_clip={0,0,LCD_W,LCD_H};render_startup_scene(e);
    uint32_t n=0;int x0=999,y0=999,x1=-1,y1=-1;char l[200];
    for(int y=0;y<LCD_H;++y)for(int x=0;x<LCD_W;++x){const size_t i=(size_t)y*LCD_W+x;if(g_frame[i]!=g_push_buffer[i]){
      if(n<6){snprintf(l,sizeof(l),"shadow diff %d,%d panel %04X clean %04X\n",x,y,g_push_buffer[i],g_frame[i]);Serial.print(l);}
      ++n;x0=min(x0,x);y0=min(y0,y);x1=max(x1,x);y1=max(y1,y);}}
    snprintf(l,sizeof(l),"shadow check at %lu ms: %lu px differ, box %d,%d..%d,%d | sct drops %lu refused %lu last err %d\n",(unsigned long)e,(unsigned long)n,x0,y0,x1,y1,(unsigned long)sct_drops,(unsigned long)sct_fails_total,(int)sct_err_last);Serial.print(l);}}
#endif
  if(dirty_count){signature_record_frame(stage,us,render_stats.draw_us,render_stats.te_us,render_stats.transfer_us,render_stats.pixels);
   if(render_stats.total_us>33333)++render_stats.over_budget;
   if(trace_left){auto &t=trace_buf[trace_used++];t=TraceFrame{};t.start=us;t.draw=render_stats.draw_us;t.te=render_stats.te_us;t.qspi=render_stats.transfer_us;t.px=render_stats.pixels;t.rects=dirty_count;--trace_left;}}
  return;
 }
 render_ui(now);push_dirty(us);
 if(dirty_count){
  if(render_stats.total_us>33333)++render_stats.over_budget;
  // Frame rate only over continuous motion (gaps of more than 3 TE periods are idle, not slow frames).
  auto &p=ui_perf;if(p.frames && us-p.last<3*te_period_us){p.interval+=us-p.last;++p.intervals;}p.last=us;++p.frames;p.draw+=render_stats.draw_us;p.draw_max=max(p.draw_max,(uint32_t)render_stats.draw_us);
  p.te+=render_stats.te_us;p.qspi+=render_stats.transfer_us;p.pixels+=render_stats.pixels;p.rects+=dirty_count;
  p.update+=ui_t_update;p.restore+=ui_t_restore;p.paint+=ui_t_paint;p.work+=ui_work;p.pool+=render_pool_used;
  if(trace_left){auto &t=trace_buf[trace_used++];t={us,render_stats.draw_us,render_stats.te_us,render_stats.transfer_us,render_stats.pixels,dirty_count};
   for(int k=0;k<8;++k)t.prof[k]=(uint16_t)min<uint32_t>(prof_cycles[k]/240,65535);t.update=ui_t_update;t.restore=(uint16_t)ui_t_warm;t.paint=ui_t_paint;t.pool=(uint16_t)min<uint32_t>(render_pool_used/16,65535);t.ring_steps=send_cycles/240;send_cycles=0;
#if defined(ESP_PLATFORM) && defined(FAST_QSPI) && FAST_QSPI
   t.queue_us=sct_queue_cycles/240;t.fails=sct_fails;sct_queue_cycles=0;sct_fails=0;
#endif
--trace_left;}
  for(int k=0;k<8;++k){p.prof[k]+=prof_cycles[k]/240;prof_cycles[k]=0;}  // microseconds at 240 MHz
 }
}
