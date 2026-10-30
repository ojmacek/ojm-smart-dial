#define main original_render_tests_main
#include "render_test.cpp"
#undef main
#include <time.h>
static double time_us(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000000.0+t.tv_nsec/1000.0;}
// One CSV row per pushed startup frame: its time on the glass, a hash of the whole panel, pixels sent.
int main(){
 puts("ms,hash,pixels,host_draw_us");
 host_us=1000000;startup_prepare();startup_active=true;base_invalid=true;startup_started=millis();
 while(startup_active){
  const uint32_t frames=render_stats.frames;host_us+=500;model_service();transferred=0;
  double start=time_us();render_tick();double duration=time_us()-start;
  if(render_stats.frames==frames)continue;
  uint64_t hash=1469598103934665603ULL;
  for(uint16_t p:panel){hash^=p;hash*=1099511628211ULL;}
  printf("%u,%016llx,%u,%.3f\n",frame_now_ms-startup_started,(unsigned long long)hash,transferred,duration);
 }
 return 0;
}
