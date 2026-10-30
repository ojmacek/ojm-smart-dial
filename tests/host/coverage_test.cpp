#define DDE_COVERAGE_METRICS
#define main original_render_tests_main
#include "render_test.cpp"
#undef main
int main(){
 uint32_t seed=123456789,checked=0;
 auto random=[&](){seed=1664525U*seed+1013904223U;return (seed>>8)*(1.f/16777216.f);};
 const float radii[]={.2f,.25f,.35f,.4f,.425f,.55f,.65f,.8f,.85f,1.25f,1.6f,2.7f,3.05f,5.033f,7.625f,15.5f};
 for(float radius:radii)for(int i=0;i<250000;++i){
  float outer=radius+.5f,root=max(0.f,radius-.5f)+random()*min(1.f,outer);
  float d=root*root;
  uint8_t expected=lroundf(clamp_float(outer-sqrtf(d),0,1)*255);
  if(fast_aa_coverage(d,outer)!=expected){fprintf(stderr,"Coverage mismatch d=%g outer=%g\n",d,outer);return 1;}
  ++checked;
 }
 // Exercise changing ribbon radii, and both sides of exact rounding boundaries.
 for(int i=0;i<2000000;++i){
  float outer=.5f+random()*15.5f,k=floorf(random()*255)+.5f;
  float root=max(0.f,outer-k/255.f+(random()-.5f)*.0001f),d=root*root;
  require(fast_aa_coverage(d,outer)==lroundf(clamp_float(outer-sqrtf(d),0,1)*255),"boundary coverage equality");++checked;
 }
 printf("PASS: %u edge samples exactly equal sqrtf reference; fast=%u reference=%u.\n",checked,coverage_fast_calls,coverage_reference_calls);
 coverage_select_backend();require(!coverage_use_fast,"equal zero host clock costs prefer reference backend");
 coverage_use_fast=true;coverage_fast_calls=coverage_reference_calls=0;
 host_us=1000000;startup_prepare();startup_active=true;base_invalid=true;startup_started=millis();
 while(startup_active)step(); // the real loop cadence; every frame also checked against a clean render
 printf("Startup edge calls: fast=%u reference=%u (%.1f%% avoid sqrtf/lroundf).\n",coverage_fast_calls,coverage_reference_calls,100.f*coverage_fast_calls/(coverage_fast_calls+coverage_reference_calls));
}
