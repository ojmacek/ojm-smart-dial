#pragma once
// Two reciprocal-square-root refinements replace most libm sqrt calls.
// Near an 8-bit rounding boundary we use the original sqrtf, so the stored
// coverage is unchanged. For outer<=16, the conservative .05 coverage guard
// exceeds the normal-float approximation error (under .021 coverage units).
#ifdef DDE_COVERAGE_METRICS
static uint32_t coverage_fast_calls=0,coverage_reference_calls=0;
#endif
#ifdef DDE_REFERENCE_COVERAGE
static bool coverage_use_fast=false;
#else
static bool coverage_use_fast=true;
#endif
static uint32_t coverage_fast_us=0,coverage_reference_us=0;
static uint8_t fast_aa_coverage(float distance2,float outer){
 if(!coverage_use_fast)return (uint8_t)lroundf(clamp_float(outer-sqrtf(distance2),0,1)*255);
 if(distance2>=1.e-8f && outer<=16.f){
  uint32_t bits;memcpy(&bits,&distance2,sizeof(bits));bits=0x5f3759dfU-(bits>>1);
  float inverse;memcpy(&inverse,&bits,sizeof(inverse));float half=.5f*distance2;
  inverse*=1.5f-half*inverse*inverse;
  inverse*=1.5f-half*inverse*inverse;
  float value=clamp_float(outer-distance2*inverse,0,1)*255;
  if(fabsf(value-((int)value+.5f))>.05f){
#ifdef DDE_COVERAGE_METRICS
   ++coverage_fast_calls;
#endif
   return (uint8_t)(value+.5f);
  }
 }
#ifdef DDE_COVERAGE_METRICS
 ++coverage_reference_calls;
#endif
 return (uint8_t)lroundf(clamp_float(outer-sqrtf(distance2),0,1)*255);
}

// Select on the actual ESP32, not a desktop sqrt benchmark. Runs once before
// the animation/CAN worker starts, with fixed work and no allocations or I/O.
static void coverage_select_backend(){
 struct Sample{float d,outer;};
 static constexpr Sample samples[]={{0.273637676f,0.700000000f},{0.023123604f,0.750000000f},{0.030410078f,0.850000000f},{0.308802870f,0.925000000f},{0.165451491f,1.350000000f},{2.329678997f,1.750000000f},{3.476028927f,2.100000000f},{5.025381551f,3.200000000f},{10.454842860f,3.550000000f},{25.358651804f,5.533000000f},{60.224486507f,8.125000000f},{0.445706923f,0.700000000f},{0.547700863f,0.750000000f},{0.120943672f,0.850000000f},{0.609699694f,0.925000000f},{1.291187767f,1.350000000f},{2.964440321f,1.750000000f},{1.274170519f,2.100000000f},{9.846382032f,3.200000000f},{7.685545799f,3.550000000f},{21.738079725f,5.533000000f},{59.636355623f,8.125000000f},{0.344805383f,0.700000000f},{0.514813939f,0.750000000f},{0.095875029f,0.850000000f},{0.586162871f,0.925000000f},{0.145001530f,1.350000000f},{2.953263201f,1.750000000f},{1.941730364f,2.100000000f},{7.526837970f,3.200000000f},{7.152076471f,3.550000000f},{20.947352295f,5.533000000f},{50.875264100f,8.125000000f},{0.176492585f,0.700000000f},{0.177108360f,0.750000000f},{0.481016851f,0.850000000f},{0.619848101f,0.925000000f},{0.598229680f,1.350000000f},{2.551502384f,1.750000000f},{1.691902671f,2.100000000f},{9.491880324f,3.200000000f},{7.843043639f,3.550000000f},{29.951016925f,5.533000000f},{56.389710919f,8.125000000f},{0.089786569f,0.700000000f},{0.151118828f,0.750000000f},{0.002507937f,0.850000000f},{0.004798894f,0.925000000f},{0.800913374f,1.350000000f},{2.143566060f,1.750000000f},{1.310449743f,2.100000000f},{6.795151286f,3.200000000f},{8.526213521f,3.550000000f},{28.240238429f,5.533000000f},{65.860317102f,8.125000000f},{0.004021050f,0.700000000f},{0.166273084f,0.750000000f},{0.007842415f,0.850000000f},{0.525566525f,0.925000000f},{1.669824898f,1.350000000f},{2.121319905f,1.750000000f},{2.827581821f,2.100000000f},{5.019947833f,3.200000000f},{7.002707768f,3.550000000f}};
 volatile uint32_t sink=0;
 auto measure=[&](bool fast){coverage_use_fast=fast;uint32_t at=micros();
  for(int repeat=0;repeat<32;++repeat)for(const auto &sample:samples)sink+=fast_aa_coverage(sample.d,sample.outer);
  return (uint32_t)(micros()-at);
 };
 coverage_fast_us=measure(true);coverage_reference_us=measure(false);
 coverage_reference_us+=measure(false);coverage_fast_us+=measure(true);
 coverage_use_fast=coverage_fast_us+coverage_fast_us/20<coverage_reference_us;
 (void)sink;
}
