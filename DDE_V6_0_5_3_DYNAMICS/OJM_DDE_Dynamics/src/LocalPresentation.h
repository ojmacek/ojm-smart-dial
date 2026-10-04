#pragma once
// Autonomous HMI illustration, not measured vehicle telemetry. Never written
// into the CAN telemetry mailbox. Requested modes are still the shared truth.
static Telemetry local_presentation(const RequestedState &state,uint32_t now){
  Telemetry t;
  float wave=sinf((now%12000)*(2.f*PI/12000.f));
  t.front=22.f+6.f*wave;
  // AUTO has a quiet neutral interval between same-phase and counter-phase.
  float auto_wave=fabsf(wave)<.12f?0.f:(wave>0?1.f:-1.f)*(fabsf(wave)-.12f)/.88f;
  float factor=state.rws==0?0.f:state.rws==1?.26f:state.rws==2?-.46f:.40f*auto_wave;
  t.rear_actual=t.rear_requested=t.front*factor;
  t.rws_actual=state.rws;
  float strength=state.tv==0?0.f:state.tv==1?.32f:state.tv==2?.68f:1.f;
  float load=state.tv==0?0.f:state.tv==1?26.f:state.tv==2?50.f:74.f;
  float split=(.32f+.18f*wave)*strength;
  for(int i=0;i<4;++i){
    float rear_bias=i>=2?1.f+.18f*strength:1.f-.18f*strength;
    t.torque[i]=clamp_float(load*rear_bias*(i%2?1.f+split:1.f-split),0,100);
  }
  t.aero_requested=t.aero_actual=state.aero==3?100.f:state.aero==1?5.f:state.aero==2?75.f:40.f+25.f*wave;
  t.aero_mode=state.aero;
  return t;
}
