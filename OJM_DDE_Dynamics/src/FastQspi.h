#pragma once
// Fast panel transfer (V6.0.6.2). The library sends a window in polled 1024-pixel blocks after a PSRAM copy
// (measured on the board: ~0.3 us per pixel). Here the same bus device, the same commands (window via the
// library, then RAMWR 0x32/0x003C00 in quad mode exactly like Arduino_ESP32QSPI::writePixels) and the same
// pins and clock are used, but the pixels go out by DMA from two internal buffers while the next chunk is
// converted straight from the framebuffer. Pins, QSPI clock, TE and the CO5300 setup are unchanged.
// FAST_QSPI 0 in the .ino returns to the library path.
#include "QspiStream.h"
#include "esp_private/spi_master_internal.h"
#ifndef QSPI_SELFTEST
#define QSPI_SELFTEST 0  // development: transfer-method test squares at boot
#endif
#ifndef QSPI_SPLIT
#define QSPI_SPLIT 1  // segmented: commands single-line in their own transaction
#endif
#ifndef QSPI_SCT
#define QSPI_SCT 1  // segmented transactions (one per window); 0: one transaction per panel command
#endif
// The library keeps its SPI device handle private; this standard explicit-instantiation idiom reads it
// without changing the library.
template<class Tag,typename Tag::type Member> struct QspiMember {friend typename Tag::type qspi_member(Tag){return Member;}};
struct QspiHandleTag {typedef spi_device_handle_t Arduino_ESP32QSPI::*type;friend type qspi_member(QspiHandleTag);};
template struct QspiMember<QspiHandleTag,&Arduino_ESP32QSPI::_handle>;
static constexpr uint32_t QSPI_DIRECT_PIXELS=8192;  // bus limit: max_transfer_sz 16392 bytes
static spi_device_handle_t qspi_device=nullptr;
static uint16_t *qspi_buffers[2]={nullptr,nullptr};
static spi_transaction_ext_t qspi_trans[2];
static bool fast_qspi=false;
static bool fast_qspi_begin(){
  qspi_device=((Arduino_ESP32QSPI*)bus)->*qspi_member(QspiHandleTag());
  for(auto &buffer:qspi_buffers){
    buffer=(uint16_t*)heap_caps_aligned_alloc(16,QSPI_CHUNK_PIXELS*sizeof(uint16_t),MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
    if(!buffer)return false;
  }
  fast_qspi=qspi_device!=nullptr;
  return fast_qspi;
}
// The library's window offsets are protected; a derived class names them (the standard access idiom, no instance).
struct Co5300Window : Arduino_CO5300 {
  static uint8_t x_start(Arduino_CO5300 *d){return d->*(&Co5300Window::_xStart);}
  static uint8_t y_start(Arduino_CO5300 *d){return d->*(&Co5300Window::_yStart);}
};

// ---- V6.0.6.5: queued transfer on an own device ----
// After setup (the library has initialised the panel, set TE and the brightness) the bus is set up again with the
// same pins, mode and clock, but with DMA descriptors for a whole frame, and one device with the hardware CS on the
// same pin and a deep queue. The library is not used after that.
// SCT mode (segmented transfer of the ESP32-S3): one queued hardware transaction per window holds all its panel
// commands as segments, CS released between them exactly as the library frames them by hand: CASET and PASET
// (sent only when the window changes, like the library), RAMWR (0x2C, no data), then the pixels 0x32/0x003C00 in
// quad mode (further chunks as 0x3C00 memory-write-continue). One interrupt per window instead of one per command:
// with ~140 single transactions per frame the SPI interrupt took ~40 % of a core. The segments of one transaction
// share one line mode, so the commands go as 0x32 (quad data) too; setup checks on the panel that it takes them
// (TE switched off and on again) and otherwise stays with single transactions (0x02 commands, CS held over chunks).
static spi_device_handle_t qspi_dma=nullptr;static bool qspi_sct=false;
// split: the commands of a window as their own segmented transaction in single-line mode (0x02, as the library
// sends them) and the pixels as a second one in quad mode; otherwise one transaction with quad commands (0x32).
static bool qspi_sct_split=QSPI_SPLIT;
static constexpr int QX_QUEUE=128;
static spi_transaction_ext_t qx[QX_QUEUE];static uint8_t qx_next=0,qx_pending=0;
static int16_t qx_window[4]={-1,-1,-1,-1};
static volatile uint32_t te_edge_us=0,te_edge_count=0;
static void IRAM_ATTR te_isr(){te_edge_us=micros();++te_edge_count;}
// -- segmented transactions --
static constexpr uint32_t SEG_PIXELS=16376;      // SCT data phase limit: 0x3FFFA bits
static constexpr uint16_t SEG_RING=192;static constexpr uint8_t SEG_GAP_CLOCKS=8;  // CS high 100 ns between segments
static spi_multi_transaction_t seg_ring[SEG_RING];static uint32_t seg_data[SEG_RING];  // command parameters (DMA-read)
struct SctRecord {uint16_t total,descs;};
// DMA descriptors in flight, counted here: the driver must never run out (a refused call leaked its buffer).
static constexpr uint16_t SCT_DESC_TOTAL=320,SCT_DESC_BUDGET=280;static uint16_t sct_desc_used=0;
static SctRecord sct_fifo[QX_QUEUE];static uint8_t sct_head=0,sct_tail=0;static uint16_t seg_head=0,seg_used=0;
static uint16_t sct_first=0,sct_count=0,sct_skip=0;  // the transaction being built
static void sct_collect(bool wait){spi_multi_transaction_t *r;
  while(qx_pending && spi_device_get_multi_trans_result(qspi_dma,&r,wait?portMAX_DELAY:0)==ESP_OK){
    seg_used-=sct_fifo[sct_tail].total;sct_desc_used-=sct_fifo[sct_tail].descs;sct_tail=(uint8_t)((sct_tail+1)%QX_QUEUE);--qx_pending;if(wait)return;}}
// Begin a transaction of up to n segments: false if the ring or the queue has no room now.
static bool sct_begin(uint16_t n){
  sct_collect(false);if(qx_pending>=QX_QUEUE-1)return false;
  const uint16_t skip=seg_head+n>SEG_RING?SEG_RING-seg_head:0;
  if(seg_used+skip+n>SEG_RING)return false;
  sct_skip=skip;sct_first=skip?0:seg_head;sct_count=0;return true;
}
static spi_multi_transaction_t &sct_seg(uint8_t c,uint32_t addr,bool quad=true){auto &s=seg_ring[sct_first+sct_count];memset(&s,0,sizeof(s));
  s.base.flags=quad?SPI_TRANS_MODE_QIO:0;s.base.cmd=c;s.base.addr=addr;s.sct_gap_len=SEG_GAP_CLOCKS;++sct_count;return s;}
static void sct_command(uint8_t c,const uint8_t *bytes,uint8_t n){
  auto &s=qspi_sct_split?sct_seg(0x02,(uint32_t)c<<8,false):sct_seg(0x32,(uint32_t)c<<8);
  if(n){uint32_t &w=seg_data[&s-seg_ring];memcpy(&w,bytes,n);s.base.tx_buffer=&w;s.base.length=n*8;}}
static uint32_t sct_fails=0,sct_queue_cycles=0,sct_segments=0,sct_drops=0,sct_fails_total=0;static esp_err_t sct_err_last=ESP_OK;  // development counters
static bool sct_end(){
  uint16_t descs=0;for(uint16_t i=0;i<sct_count;++i)descs+=1+(seg_ring[sct_first+i].base.length/8+4091)/4092;  // conf + data
  if(sct_desc_used+descs>SCT_DESC_BUDGET){++sct_fails;return false;}
  const uint32_t c0=esp_cpu_get_cycle_count();const esp_err_t e=spi_device_queue_multi_trans(qspi_dma,seg_ring+sct_first,sct_count,0);sct_queue_cycles+=esp_cpu_get_cycle_count()-c0;
  if(e!=ESP_OK){++sct_fails;++sct_fails_total;sct_err_last=e;return false;}  // no room: not queued
  sct_segments+=sct_count;
  const uint16_t total=sct_skip+sct_count;seg_used+=total;seg_head=(uint16_t)((sct_first+sct_count)%SEG_RING);
  sct_fifo[sct_head].total=total;sct_fifo[sct_head].descs=descs;sct_desc_used+=descs;sct_head=(uint8_t)((sct_head+1)%QX_QUEUE);++qx_pending;return true;
}
static void sct_window(int16_t x,int16_t y,int16_t w,int16_t h){
  if(x!=qx_window[0] || y!=qx_window[1] || w!=qx_window[2] || h!=qx_window[3]){
    auto *panel=(Arduino_CO5300*)display;const uint16_t xs=Co5300Window::x_start(panel),ys=Co5300Window::y_start(panel);
    const uint16_t x0=x+xs,x1=x+xs+w-1,y0=y+ys,y1=y+ys+h-1;
    const uint8_t ca[4]={(uint8_t)(x0>>8),(uint8_t)x0,(uint8_t)(x1>>8),(uint8_t)x1},pa[4]={(uint8_t)(y0>>8),(uint8_t)y0,(uint8_t)(y1>>8),(uint8_t)y1};
    sct_command(0x2A,ca,4);sct_command(0x2B,pa,4);}
  sct_command(0x2C,nullptr,0);
}
static void sct_note_window(int16_t x,int16_t y,int16_t w,int16_t h){qx_window[0]=x;qx_window[1]=y;qx_window[2]=w;qx_window[3]=h;}
static void sct_pixels(const uint16_t *src,uint32_t n){auto &s=sct_seg(0x32,0x003C00);s.base.tx_buffer=src;s.base.length=n*16;}
static inline uint16_t sct_needed(int16_t w,int16_t h){return (uint16_t)(4*(((uint32_t)w*h+SEG_PIXELS-1)/SEG_PIXELS));}  // windows of whole rows
// Several panel-order windows as one segmented transaction: the driver's cost (~90 us measured) is per call.
static constexpr uint16_t SCT_BATCH_SEGMENTS=16;
struct QxRect {int16_t x,y,w,h;const uint16_t *src;};
static bool sct_push_batch(const QxRect *r,uint8_t n){
  uint16_t segs=0;for(uint8_t i=0;i<n;++i)segs+=sct_needed(r[i].w,r[i].h);
  if(!sct_begin(segs))return false;
  int16_t saved[4];memcpy(saved,qx_window,sizeof(saved));
  for(uint8_t i=0;i<n;++i){const uint32_t total=(uint32_t)r[i].w*r[i].h;sct_window(r[i].x,r[i].y,r[i].w,r[i].h);
    for(uint32_t done=0;done<total;){const uint32_t k=min(SEG_PIXELS,total-done);sct_pixels(r[i].src+done,k);done+=k;}
    sct_note_window(r[i].x,r[i].y,r[i].w,r[i].h);}
  if(!sct_end()){memcpy(qx_window,saved,sizeof(saved));return false;}
  return true;
}
// -- single transactions (fallback) --
static void qx_collect(bool wait){
  if(qspi_sct){sct_collect(wait);return;}
  spi_transaction_t *r;
  while(qx_pending && spi_device_get_trans_result(qspi_dma,&r,wait?portMAX_DELAY:0)==ESP_OK){--qx_pending;if(wait)return;}}
// Everything queued is on the panel (the pool and the bounce buffers are free again).
static void qspi_wait_idle(){if(qspi_dma)while(qx_pending)qx_collect(true);}
static spi_transaction_ext_t &qx_slot(){
  qx_collect(false);if(qx_pending>=QX_QUEUE)qx_collect(true);  // records are reused in order, the oldest is done
  auto &t=qx[qx_next];qx_next=(uint8_t)((qx_next+1)%QX_QUEUE);memset(&t,0,sizeof(t));return t;
}
static void qx_send(spi_transaction_ext_t &t){spi_device_queue_trans(qspi_dma,(spi_transaction_t*)&t,portMAX_DELAY);++qx_pending;}
static void qx_command(uint8_t c,uint16_t a,uint16_t b,uint8_t bytes){auto &t=qx_slot();
  t.base.flags=SPI_TRANS_MULTILINE_CMD|SPI_TRANS_MULTILINE_ADDR|(bytes?SPI_TRANS_USE_TXDATA:0);t.base.cmd=0x02;t.base.addr=(uint32_t)c<<8;
  if(bytes==4){t.base.tx_data[0]=a>>8;t.base.tx_data[1]=a;t.base.tx_data[2]=b>>8;t.base.tx_data[3]=b;}else t.base.tx_data[0]=a;
  t.base.length=bytes*8;qx_send(t);}
static void qx_window_set(int16_t x,int16_t y,int16_t w,int16_t h){
  if(x!=qx_window[0] || y!=qx_window[1] || w!=qx_window[2] || h!=qx_window[3]){
    auto *panel=(Arduino_CO5300*)display;const uint16_t xs=Co5300Window::x_start(panel),ys=Co5300Window::y_start(panel);
    qx_command(0x2A,x+xs,x+xs+w-1,4);qx_command(0x2B,y+ys,y+ys+h-1,4);
    qx_window[0]=x;qx_window[1]=y;qx_window[2]=w;qx_window[3]=h;}
  qx_command(0x2C,0,0,0);
}
static void qx_pixels(const uint16_t *src,uint32_t n,bool first,bool last){auto &t=qx_slot();
  if(first){t.base.flags=SPI_TRANS_MODE_QIO;t.base.cmd=0x32;t.base.addr=0x003C00;}
  else t.base.flags=SPI_TRANS_MODE_QIO|SPI_TRANS_VARIABLE_CMD|SPI_TRANS_VARIABLE_ADDR|SPI_TRANS_VARIABLE_DUMMY;
  if(!last)t.base.flags|=SPI_TRANS_CS_KEEP_ACTIVE;
  t.base.tx_buffer=src;t.base.length=n*16;qx_send(t);}
// Room to queue this window now without waiting (finished transactions are collected first).
static bool qx_room(int16_t w,int16_t h){
  qx_collect(false);
  if(qspi_sct){const uint16_t n=sct_needed(w,h);return qx_pending+n/2<QX_QUEUE && seg_used+n+n<=SEG_RING;}  // n more for a wrap

  return qx_pending+3+((uint32_t)w*h+QSPI_DIRECT_PIXELS-1)/QSPI_DIRECT_PIXELS<=QX_QUEUE;
}
// One window, queued. Panel-order contiguous internal buffers go straight out; anything else is converted into
// the two bounce buffers (each reused only after its previous chunk is on the panel).
static void qx_push_rect(int16_t x,int16_t y,int16_t w,int16_t h,const uint16_t *src,int16_t stride,bool panel_order){
  const uint32_t total=(uint32_t)w*h;
  if(qspi_sct){
    // Every pixel transfer is ONE segment: a window larger than a segment (or than a bounce buffer, when converted)
    // goes as several windows of whole rows. (Continuing a window with 0x3C00 after CS was released between
    // segments lost pixels on the panel: a stale dot at the end of the logo rectangle in the startup hold.)
    const bool direct=panel_order && stride==w;
    const uint32_t cap=direct?SEG_PIXELS:QSPI_CHUNK_PIXELS;
    const int16_t rows=(int16_t)max<uint32_t>(2,(cap/(uint32_t)w)&~1u);
    if(!direct)qspi_wait_idle();  // the bounce buffers are free
    uint8_t k=0;
    for(int16_t y0=0;y0<h;y0+=rows,k^=1){
      const int16_t hh=min<int16_t>(rows,h-y0);const uint32_t n=(uint32_t)w*hh;const uint16_t *pixels;
      if(direct)pixels=src+(size_t)y0*w;
      else{while(qx_pending>(qspi_sct_split?2:1))sct_collect(true);  // this buffer's previous window is out
        uint32_t *dst=(uint32_t*)qspi_buffers[k];
        for(int16_t r=0;r<hh;++r){const uint32_t *from=(const uint32_t*)(src+(size_t)(y0+r)*stride);
          for(int16_t c=0;c<w/2;++c){const uint32_t v=from[c];*dst++=((v&0x00FF00FFu)<<8)|((v>>8)&0x00FF00FFu);}}
        pixels=qspi_buffers[k];}
      if(qspi_sct_split){
        while(!sct_begin(3))sct_collect(true);
        sct_window(x,y+y0,w,hh);while(!sct_end()){if(!qx_pending){++sct_drops;return;}sct_collect(true);}
        sct_note_window(x,y+y0,w,hh);
        while(!sct_begin(1))sct_collect(true);
        sct_pixels(pixels,n);while(!sct_end()){if(!qx_pending){++sct_drops;return;}sct_collect(true);}
      }else{
        while(!sct_begin(4))sct_collect(true);
        sct_window(x,y+y0,w,hh);sct_pixels(pixels,n);while(!sct_end()){if(!qx_pending){++sct_drops;return;}sct_collect(true);}
        sct_note_window(x,y+y0,w,hh);
      }
    }
    return;
  }
  if(panel_order && stride==w){
    qx_window_set(x,y,w,h);
    for(uint32_t done=0;done<total;){const uint32_t n=min(QSPI_DIRECT_PIXELS,total-done);qx_pixels(src+done,n,!done,done+n==total);done+=n;}
    return;
  }
  qspi_wait_idle();qx_window_set(x,y,w,h);uint32_t sent=0;
  qspi_stream_rect(src,stride,w,h,qspi_buffers,[&](uint16_t *buffer,uint32_t pixels,bool first){
    qx_pixels(buffer,pixels,first,sent+pixels==total);sent+=pixels;
    while(qx_pending>1)qx_collect(true);  // the other buffer is free for the next chunk
  });
}
// TE edges within 50 ms after a command took effect.
static uint32_t qspi_te_edges(){delay(5);const uint32_t c=te_edge_count;delay(50);return te_edge_count-c;}
static uint32_t qspi_te_check[6]={0,0,0,0,0,0};static const char *qspi_mode="library";
static void sct_te(const uint8_t *cmds,uint8_t n){  // TEOFF 0x34 / TEON 0x35 (+mode 0) as one segmented transaction
  while(!sct_begin(n))sct_collect(true);
  for(uint8_t i=0;i<n;++i){static const uint8_t zero=0;sct_command(cmds[i],&zero,cmds[i]==0x35?1:0);}
  while(!sct_end()){if(!qx_pending){++sct_drops;return;}sct_collect(true);}
  qspi_wait_idle();
}
#if QSPI_SELFTEST
static void qspi_selftest_single();
#endif
static bool qspi_takeover(){
  if(!fast_qspi || qspi_dma)return qspi_dma!=nullptr;
  // The library's device and bus go; the bus comes back with the same pins and descriptors for a whole frame.
  spi_device_release_bus(qspi_device);spi_bus_remove_device(qspi_device);spi_bus_free(ESP32QSPI_SPI_HOST);
  spi_bus_config_t b;memset(&b,0,sizeof(b));
  b.mosi_io_num=TFT_D0;b.miso_io_num=TFT_D1;b.sclk_io_num=TFT_SCLK;b.quadwp_io_num=TFT_D2;b.quadhd_io_num=TFT_D3;
  b.data4_io_num=b.data5_io_num=b.data6_io_num=b.data7_io_num=-1;
  b.max_transfer_sz=SCT_DESC_TOTAL*4092;b.flags=SPICOMMON_BUSFLAG_MASTER|SPICOMMON_BUSFLAG_GPIO_PINS;b.isr_cpu_id=ESP_INTR_CPU_AFFINITY_AUTO;
  if(spi_bus_initialize(ESP32QSPI_SPI_HOST,&b,SPI_DMA_CH_AUTO)!=ESP_OK){Serial.println("FATAL: QSPI bus init");return false;}
  spi_device_interface_config_t c;memset(&c,0,sizeof(c));
  c.command_bits=8;c.address_bits=24;c.mode=0;c.clock_source=SPI_CLK_SRC_DEFAULT;c.clock_speed_hz=DISPLAY_BUS_HZ;
  c.cs_ena_pretrans=2;c.cs_ena_posttrans=2;c.spics_io_num=TFT_CS;c.flags=SPI_DEVICE_HALFDUPLEX;c.queue_size=QX_QUEUE;
  if(spi_bus_add_device(ESP32QSPI_SPI_HOST,&c,&qspi_dma)!=ESP_OK){qspi_dma=nullptr;Serial.println("FATAL: QSPI device");return false;}
  qspi_device=qspi_dma;  // the bench reads the clock from here
  spi_device_acquire_bus(qspi_dma,portMAX_DELAY);  // CS may stay active over the chunks of one window
  attachInterrupt(digitalPinToInterrupt(TFT_TE),te_isr,RISING);
  qspi_mode="queued";
  // Commands through the new device reach the panel: TE stops after TEOFF (0x34) and runs again after TEON
  // (0x35, mode 0, the same command enable_te_output() sends through the library).
  qspi_te_check[0]=qspi_te_edges();
  qx_command(0x34,0,0,0);qspi_wait_idle();qspi_te_check[1]=qspi_te_edges();
  qx_command(0x35,0x00,0,1);qspi_wait_idle();qspi_te_check[2]=qspi_te_edges();
#if QSPI_SELFTEST
  qspi_selftest_single();
#endif
#if QSPI_SCT
  // Segmented transactions: TEON with quad data alone, then off+on in one transaction (both segments, CS between).
  if(spi_bus_multi_trans_mode_enable(qspi_dma,true)==ESP_OK){
    qspi_sct=true;
    const uint8_t off[1]={0x34},on[1]={0x35},on_off[2]={0x35,0x34},off_on[2]={0x34,0x35};
    sct_te(off,1);qspi_te_check[3]=qspi_te_edges();sct_te(on,1);qspi_te_check[4]=qspi_te_edges();
    sct_te(on_off,2);const uint32_t stopped=qspi_te_edges();sct_te(off_on,2);qspi_te_check[5]=qspi_te_edges();
    const bool ok=qspi_te_check[3]<=1 && qspi_te_check[4]>=2 && stopped<=1 && qspi_te_check[5]>=2;
    if(ok)qspi_mode="segmented";
    else{qspi_sct=false;spi_bus_multi_trans_mode_enable(qspi_dma,false);qx_command(0x35,0x00,0,1);qspi_wait_idle();}
  }
#endif
  qx_window[0]=qx_window[1]=qx_window[2]=qx_window[3]=-1;
  return true;
}
// Synchronous transfer of one window (startup frames, development bench).
static void fast_push_rect(int16_t x,int16_t y,int16_t w,int16_t h,const uint16_t *src,int16_t stride,bool panel_order=false){
  if(qspi_dma){qx_push_rect(x,y,w,h,src,stride,panel_order);qspi_wait_idle();return;}
  display->startWrite();
  ((Arduino_CO5300*)display)->writeAddrWindow(x,y,w,h); // CASET/PASET when changed + RAMWR, as the library does
  gpio_set_level((gpio_num_t)TFT_CS,0);
  int pending=-1;
  auto finish=[&](){if(pending>=0){spi_transaction_t *done;spi_device_get_trans_result(qspi_device,&done,portMAX_DELAY);pending=-1;}};
  auto send=[&](const uint16_t *buffer,uint32_t pixels,bool first,int k){
    auto &t=qspi_trans[k];memset(&t,0,sizeof(t));
    if(first){t.base.flags=SPI_TRANS_MODE_QIO;t.base.cmd=0x32;t.base.addr=0x003C00;}
    else t.base.flags=SPI_TRANS_MODE_QIO|SPI_TRANS_VARIABLE_CMD|SPI_TRANS_VARIABLE_ADDR|SPI_TRANS_VARIABLE_DUMMY;
    t.base.tx_buffer=buffer;t.base.length=pixels*16;
    spi_device_queue_trans(qspi_device,(spi_transaction_t*)&t,portMAX_DELAY);pending=k;
  };
  if(panel_order && stride==w){
    // Already in panel order and contiguous (internal, DMA-capable pool): straight from the buffer, 8192 px a time.
    const uint32_t total=(uint32_t)w*h;uint32_t done=0;int k=0;
    while(done<total){const uint32_t n=min((uint32_t)QSPI_DIRECT_PIXELS,total-done);finish();send(src+done,n,done==0,k);done+=n;k^=1;}
    finish();gpio_set_level((gpio_num_t)TFT_CS,1);display->endWrite();return;
  }
  qspi_stream_rect(src,stride,w,h,qspi_buffers,[&](uint16_t *buffer,uint32_t pixels,bool first){
    finish(); // the previous chunk (other buffer) is on the wire until here
    send(buffer,pixels,first,buffer==qspi_buffers[0]?0:1);
  });
  finish();
  gpio_set_level((gpio_num_t)TFT_CS,1);
  display->endWrite();
}
