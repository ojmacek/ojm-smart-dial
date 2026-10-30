#define main original_render_tests_main
#include "render_test.cpp"
#undef main
// The loop can stall (CAN burst, flash cache miss, serial): the startup must stay exact after any gap.
int main(int argc,char **argv){
 (void)argc;(void)argv;
 const uint32_t stall_us[]={0,17000,0,33000,0,65000,0,50000,0,250000,0,9000};
 host_us=1000000;startup_prepare();startup_active=true;base_invalid=true;startup_started=millis();
 unsigned n=0;
 // Each stall is followed by normal 0.5 ms polling for 12 ms (a frame or so), then the next stall.
 while(startup_active){host_us+=stall_us[n%12];for(int i=0;i<24 && startup_active;++i)step();++n;require(n<40000,"startup completes after stalls");}
 {const uint32_t before=render_stats.frames;while(render_stats.frames==before)step();}
 require(checked_frames>50,"all startup phases exercised at irregular cadence");
 require(frames_on_one_edge==0 && late_pushes==0,"TE pacing survives stalls");
 printf("PASS: %u startup frames with 9/17/33/50/65/250 ms loop stalls match clean renders; aligned transfers at TE edges.\n",checked_frames);
 return 0;
}
