#pragma once
// Development micro-benchmark (Serial 'B'): CPU cycles per call of the drawing building blocks on this board.
// Runs only on request, between frames; it paints into the internal pool, never onto the panel.
static uint32_t bench_cycles(){return esp_cpu_get_cycle_count();}
__attribute__((optimize("no-math-errno"))) static float bench_sqrt_inline(float v){return sqrtf(v);}
__attribute__((optimize("no-math-errno"))) static uint8_t bench_cov_new(float d2,float outer){
  float v=outer-sqrtf(d2);v=v<0?0:(v>1?1:v);return (uint8_t)(int)(v*255+.5f);}
// Fast path: reciprocal square root with two Newton steps; exact reference only next to a rounding boundary.
__attribute__((optimize("no-math-errno"))) static uint8_t bench_cov_fast2(float d2,float outer){
  if(d2>=1.e-8f && outer<=16.f){uint32_t bits;memcpy(&bits,&d2,4);bits=0x5f3759dfU-(bits>>1);float inv;memcpy(&inv,&bits,4);
   const float h=.5f*d2;inv*=1.5f-h*inv*inv;inv*=1.5f-h*inv*inv;float v=outer-d2*inv;v=v<0?0:(v>1?1:v);v*=255;
   const int i=(int)v;const float f=v-i;if(f<.45f || f>.55f)return (uint8_t)(f<.5f?i:i+1);}
  float v=outer-sqrtf(d2);v=v<0?0:(v>1?1:v);return (uint8_t)(int)(v*255+.5f);}
static void dev_bench(){
  qspi_wait_idle();  // the pool and the bus are free only between transfers
  {// Pieces of a culled pass: powf, a culled arc, a culled ring stack, ui_frame.
   char l[200];const auto clip=art_clip;art_clip={0,0,0,0};volatile float fs=0;const int32_t now=(int32_t)millis();
   uint32_t t0=bench_cycles();for(int i=0;i<100;++i)fs+=powf(.37f+i*.001f,2.2f);const uint32_t c_pow=(bench_cycles()-t0)/100;
   t0=bench_cycles();for(int i=0;i<100;++i)art_arc(g_frame,233,233,214,255.f+i*.01f,262.f,1.5f,UI_MARK,200);const uint32_t c_arc=(bench_cycles()-t0)/100;
   t0=bench_cycles();for(int i=0;i<100;++i)art_ring_stack(226,.6f,UI_RING_EDIT,255,227.5f,1.6f+3,UI_GLOW,60,1.6f,UI_PULSE,230);const uint32_t c_rs=(bench_cycles()-t0)/100;
   t0=bench_cycles();for(int i=0;i<100;++i){const auto f=ui_frame(now+i);fs+=f.body;}const uint32_t c_frame=(bench_cycles()-t0)/100;
   t0=bench_cycles();for(int i=0;i<100;++i){fs+=sinf(i*.01f);}const uint32_t c_sin=(bench_cycles()-t0)/100;
   art_clip=clip;
   snprintf(l,sizeof(l),"bench: culled pieces: powf %lu, arc %lu, ring_stack %lu, ui_frame %lu, sinf %lu cyc\n",(unsigned long)c_pow,(unsigned long)c_arc,(unsigned long)c_rs,(unsigned long)c_frame,(unsigned long)c_sin);
   Serial.print(l);}
  {// Fixed cost of one scene pass (empty clip) per section: what every damaged rectangle pays before drawing.
   char l[200];uint32_t saved[PROF_PARTS];memcpy(saved,prof_cycles,sizeof(saved));memset(prof_cycles,0,sizeof(prof_cycles));
   const auto clip=art_clip;art_clip={0,0,0,0};const int32_t now=(int32_t)millis();
   const uint32_t t0=bench_cycles();for(int i=0;i<20;++i)ui_paint(now);const uint32_t all=(bench_cycles()-t0)/20;art_clip=clip;
   snprintf(l,sizeof(l),"bench: empty ui_paint %lu cyc: side %lu refl %lu top %lu wing %lu wheel %lu ring %lu seg %lu text %lu\n",(unsigned long)all,
    (unsigned long)(prof_cycles[0]/20),(unsigned long)(prof_cycles[1]/20),(unsigned long)(prof_cycles[2]/20),(unsigned long)(prof_cycles[3]/20),
    (unsigned long)(prof_cycles[4]/20),(unsigned long)(prof_cycles[5]/20),(unsigned long)(prof_cycles[6]/20),(unsigned long)(prof_cycles[7]/20));
   Serial.print(l);memcpy(prof_cycles,saved,sizeof(saved));}
  if(!render_pool || render_pool_px<466*40)return;
  char line[300];volatile float fsink=0;volatile uint32_t isink=0;
  auto per=[](uint32_t c,uint32_t n){return (float)c/n;};
  const int N=4000;
  float vals[64];for(int i=0;i<64;++i)vals[i]=0.37f+i*0.731f;
  uint32_t t=bench_cycles();for(int i=0;i<N;++i)fsink+=sqrtf(vals[i&63]);const float c_sqrt=per(bench_cycles()-t,N);
  t=bench_cycles();for(int i=0;i<N;++i)isink+=lroundf(vals[i&63]*10);const float c_lround=per(bench_cycles()-t,N);
  t=bench_cycles();for(int i=0;i<N;++i)isink+=(int)(vals[i&63]*10+.5f);const float c_round=per(bench_cycles()-t,N);
  t=bench_cycles();for(int i=0;i<N;++i)fsink+=1.f/vals[i&63];const float c_div=per(bench_cycles()-t,N);
  float chain=1.f;t=bench_cycles();for(int i=0;i<N;++i)chain=chain*1.0001f+.5f;const float c_fma=per(bench_cycles()-t,N);fsink+=chain;
  float acc=0;t=bench_cycles();for(int i=0;i<N;++i)acc+=vals[i&63]*vals[(i+7)&63];const float c_mul=per(bench_cycles()-t,N);fsink+=acc;
  int iacc=0;t=bench_cycles();for(int i=0;i<N;++i)iacc+=(int)(vals[i&63]*3.f);const float c_trunc=per(bench_cycles()-t,N);isink+=iacc;
  const bool saved=coverage_use_fast;
  coverage_use_fast=false;t=bench_cycles();for(int i=0;i<N;++i)isink+=fast_aa_coverage(vals[i&63]*.02f,.85f+(i&3)*.5f);const float c_ref=per(bench_cycles()-t,N);
  coverage_use_fast=true;t=bench_cycles();for(int i=0;i<N;++i)isink+=fast_aa_coverage(vals[i&63]*.02f,.85f+(i&3)*.5f);const float c_fast=per(bench_cycles()-t,N);
  coverage_use_fast=saved;
  t=bench_cycles();for(int i=0;i<N;++i)fsink+=bench_sqrt_inline(vals[i&63]);const float c_sqrt_inline=per(bench_cycles()-t,N);
  t=bench_cycles();for(int i=0;i<N;++i)isink+=bench_cov_new(vals[i&63]*.02f,.85f+(i&3)*.5f);const float c_cov_new=per(bench_cycles()-t,N);
  t=bench_cycles();for(int i=0;i<N;++i)isink+=bench_cov_fast2(vals[i&63]*.02f,.85f+(i&3)*.5f);const float c_cov_fast2=per(bench_cycles()-t,N);
  // Exactness of the new coverage on this FPU against the reference formula (sqrtf + lroundf).
  uint32_t mismatches=0,mism2=0,checked=0;uint32_t seed=12345;
  for(int i=0;i<200000;++i){seed=seed*1664525u+1013904223u;const float outer=.5f+(seed>>8)*(15.5f/16777216.f);
   seed=seed*1664525u+1013904223u;const float root=max(0.f,outer-1.2f)+(seed>>8)*(1.4f/16777216.f),d2=root*root;
   const uint8_t a=(uint8_t)lroundf(clamp_float(outer-sqrtf(d2),0,1)*255),b=bench_cov_new(d2,outer),c=bench_cov_fast2(d2,outer);mismatches+=a!=b;mism2+=a!=c;++checked;}
  t=bench_cycles();uint16_t px=0x1234;for(int i=0;i<N;++i)px=halo_blend565(px,0xF79D,(uint8_t)(i*37));const float c_blend=per(bench_cycles()-t,N);isink+=px;
  // Pixel writes: through halo_px (global target), a local row pointer, and a packed 5-bit-alpha blend.
  auto saved_t=halo_target;halo_target={render_pool,0,0,LCD_W};
  t=bench_cycles();for(int i=0;i<N;++i)halo_px(i%400,i/400,0xF79D,(uint8_t)(i*37|1));const float c_px=per(bench_cycles()-t,N);
  uint16_t *row=render_pool;
  t=bench_cycles();for(int i=0;i<N;++i){uint16_t &p=row[i%400];p=halo_blend565(p,0xF79D,(uint8_t)(i*37|1));}const float c_row=per(bench_cycles()-t,N);
  t=bench_cycles();for(int i=0;i<N;++i){uint16_t &p=row[i%400];const uint32_t a5=((uint8_t)(i*37|1)+4)>>3;
   uint32_t d=(p|((uint32_t)p<<16))&0x07E0F81Fu,c=(0xF79Du|(0xF79Du<<16))&0x07E0F81Fu;d=(d+(((c-d)*a5)>>5))&0x07E0F81Fu;p=(uint16_t)(d|(d>>16));}
  const float c_pack=per(bench_cycles()-t,N);
  halo_target=saved_t;
  // Primitives into a 466x40 strip of the pool.
  auto saved_clip=art_clip;auto saved_target=halo_target;
  halo_target={render_pool,0,200,LCD_W};art_clip={0,200,LCD_W,40};
  const uint32_t w0=halo_work;t=bench_cycles();
  for(int i=0;i<50;++i)art_line(render_pool,20+i*8,205,60+i*8,235,.85f,WHITE,230);
  const uint32_t c_lines=bench_cycles()-t,w_lines=halo_work-w0;
  const uint32_t w1=halo_work;t=bench_cycles();
  for(int i=0;i<10;++i)art_arc(render_pool,233,233,226,0,360,1.9f,UI_PULSE,200);
  const uint32_t c_arcs=bench_cycles()-t,w_arcs=halo_work-w1;
  // The edit pulse ring over black and over a non-black background (top 137 rows of the panel).
  halo_target={render_pool,0,0,LCD_W};art_clip={0,0,LCD_W,137};
  uint32_t c_rs[3],w_rs[3];
  for(int pass=0;pass<3;++pass){
   const uint16_t bg=pass==2?0x0841:0;for(int i=0;i<LCD_W*137;++i)render_pool[i]=bg;
   const uint32_t w=halo_work;t=bench_cycles();
   art_ring_stack(226,.6f,UI_RING_EDIT,255,227.5f,1.6f+3,UI_GLOW,60,1.6f,UI_PULSE,230);
   c_rs[pass]=bench_cycles()-t;w_rs[pass]=halo_work-w;}
  snprintf(line,sizeof(line),"bench: ring_stack first %lu cyc, black %lu cyc / %lu px = %.0f cyc/px, grey %lu cyc = %.0f cyc/px\n",
   (unsigned long)c_rs[0],(unsigned long)c_rs[1],(unsigned long)w_rs[1],w_rs[1]?(float)c_rs[1]/w_rs[1]:0.f,(unsigned long)c_rs[2],w_rs[2]?(float)c_rs[2]/w_rs[2]:0.f);
  Serial.print(line);
  art_clip=saved_clip;halo_target=saved_target;
  snprintf(line,sizeof(line),"bench: px=%.0f row=%.0f pack=%.0f fma=%.1f mul=%.1f trunc=%.1f sqrt_inline=%.0f cov_new=%.0f exact=%lu/%lu cov_fast2=%.0f exact2=%lu sqrtf=%.0f lroundf=%.0f round=%.0f div=%.0f cov_ref=%.0f cov_fast=%.0f blend=%.0f cyc | line %.0f cyc/step (%lu) | arc %.0f cyc/step (%lu)\n",
    c_px,c_row,c_pack,c_fma,c_mul,c_trunc,c_sqrt_inline,c_cov_new,(unsigned long)(checked-mismatches),(unsigned long)checked,c_cov_fast2,(unsigned long)(checked-mism2),c_sqrt,c_lround,c_round,c_div,c_ref,c_fast,c_blend,w_lines?(float)c_lines/w_lines:0.f,(unsigned long)w_lines,w_arcs?(float)c_arcs/w_arcs:0.f,(unsigned long)w_arcs);
  Serial.print(line);
  int khz=0;spi_device_get_actual_freq(qspi_device,&khz);
  snprintf(line,sizeof(line),"bench: heap internal free=%u largest=%u pool=%lu px\n",(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA),(unsigned long)pool_capacity());Serial.print(line);
  // Transfer timing: one 466x40 window and 40 windows of 466x1 from the pool.
  uint32_t u=micros();fast_push_rect(0,200,466,40,render_pool,466);const uint32_t one=micros()-u;
  u=micros();for(int i=0;i<40;++i)fast_push_rect(0,200+i,466,2,render_pool,466);const uint32_t many=micros()-u;
  // Window overhead of the queued transfer: 40 windows of 464x2 already in panel order, queued back to back.
  uint32_t queued=0;
  if(qspi_dma){const uint32_t u0=micros();for(int i=0;i<40;++i)qx_push_rect((i&1)*2,200+(i&1)*2,464,2,render_pool+i*928,464,true);qspi_wait_idle();queued=micros()-u0;}
  snprintf(line,sizeof(line),"bench: 40 windows 464x2 queued %lu us (pixels alone %.0f us at 80 MHz quad)\n",(unsigned long)queued,37120*.05f);
  Serial.print(line);
  // Painting while the DMA runs: the same ring into rows 0..69 of the pool (32 k px) alone, during one big window
  // from the pool's upper half (memory contention) and during 60 small windows (interrupt load).
  if(qspi_dma && render_pool_px>=72000){
   uint32_t c[3];auto saved_t2=halo_target;auto saved_c2=art_clip;
   for(int mode=0;mode<3;++mode){
    halo_target={render_pool,0,0,LCD_W};art_clip={0,0,LCD_W,68};memset(render_pool,0,LCD_W*68*2);
    if(mode==1)qx_push_rect(0,200,466,76,render_pool+36000,466,true);
    if(mode==2)for(int i=0;i<60;++i)qx_push_rect((i&1)*2,200+(i&1)*2,200,2,render_pool+36000+i*400,200,true);
    const uint32_t t0=bench_cycles();for(int k=0;k<3;++k)art_ring_stack(226,.6f,UI_RING_EDIT,255,227.5f,1.6f+3,UI_GLOW,60,1.6f,UI_PULSE,230);c[mode]=bench_cycles()-t0;
    qspi_wait_idle();}
   halo_target=saved_t2;art_clip=saved_c2;
   snprintf(line,sizeof(line),"bench: paint alone %lu cyc, during one big DMA %lu cyc, during 60 small windows %lu cyc\n",(unsigned long)c[0],(unsigned long)c[1],(unsigned long)c[2]);
   Serial.print(line);}
  snprintf(line,sizeof(line),"bench: spi=%d kHz one 466x40 window %lu us (%.3f us/px), 40 windows %lu us (+%.0f us per window)\n",khz,(unsigned long)one,one/18640.f,(unsigned long)many,(many-one*2.f)/40);
  Serial.print(line);(void)fsink;(void)isink;
}
