#pragma once
struct SignaturePerf {
 uint32_t frames=0,draw_us=0,te_us=0,transfer_us=0,pixels=0;
 uint32_t last_start=0,interval_us=0,intervals=0,max_frame_us=0,over_30=0;
};
static SignaturePerf signature_perf[3];
static void signature_record_frame(uint8_t stage,uint32_t started,uint32_t draw,uint32_t te,uint32_t transfer,uint32_t pixels){
 if(stage==1)return; // Static 2.5 s hold must not distort moving FPS.
 auto &p=signature_perf[stage==0?0:stage==2?1:2];
 if(p.frames){p.interval_us+=(uint32_t)(started-p.last_start);++p.intervals;}
 p.last_start=started;++p.frames;p.draw_us+=draw;p.te_us+=te;p.transfer_us+=transfer;p.pixels+=pixels;
 p.max_frame_us=max(p.max_frame_us,draw+te+transfer);if(draw+te+transfer>33333)++p.over_30;
}
