#pragma once
#include "LocalPresentation.h"
static RequestedState requested;
static Telemetry telemetry;
static uint32_t telemetry_revision=0,telemetry_received_at=0;
static uint32_t mode_changed_at=0,model_last_us=0,aero_mismatch_at=0;
static bool telemetry_online=false;
static uint8_t effective_faults=0;
struct VisualState {
  float front=0,rear=0,torque[4]={0,0,0,0},peak[4]={0,0,0,0},tc[4]={0,0,0,0};
  float aero=0,yaw=0,airbrake=0,vector_strength=0,pulse_phase=0;
  float front_velocity=0,rear_velocity=0,aero_velocity=0;
};
static VisualState visual;
// A restrained mechanical settle. Small substeps keep a long frame stable.
static void settle_axis(float &value,float &velocity,float target,float dt,float frequency){
  int steps=max(1,(int)ceilf(dt/.008f));float h=dt/steps;
  for(int i=0;i<steps;++i){velocity+=(frequency*frequency*(target-value)-1.64f*frequency*velocity)*h;value+=velocity*h;}
}
static const char *const PAGE_NAMES[]={"RWS","TORQUE VECTORING","ACTIVE AERO"};
static const char *const RWS_NAMES[]={"OFF","STABLE","AGILE","AUTO"};
static const char *const TV_NAMES[]={"OFF","NORMAL","SPORT","MAX"};
static const char *const AERO_NAMES[]={"AUTO","LOW DRAG","DOWNFORCE","AIR BRAKE"};
static uint8_t page_mode(uint8_t page){return page==0?requested.rws:page==1?requested.tv:requested.aero;}
static const char *mode_name(uint8_t page){return page==0?RWS_NAMES[requested.rws]:page==1?TV_NAMES[requested.tv]:AERO_NAMES[requested.aero];}
static void apply_requested(uint8_t command,uint8_t value){
  if(!dde_valid_command(command,value))return;
  uint8_t *slot=command==0x10?&requested.page:command==0x11?&requested.rws:command==0x12?&requested.tv:&requested.aero;
  if(*slot!=value){*slot=value;mode_changed_at=millis();}
}
static void model_service(){
  twai_message_t commands[COMMAND_COUNT];uint8_t count=can_take_commands(commands);
  for(uint8_t i=0;i<count;++i){apply_requested(commands[i].data[0],commands[i].data[1]);can_queue_ack(commands[i],0);}
  can_take_telemetry(telemetry,telemetry_revision,telemetry_received_at);
  const uint32_t now=millis();
  telemetry_online=telemetry_revision && (uint32_t)(now-telemetry_received_at)<=OFFLINE_MS;
  // This dial is the presentation authority. CAN carries selected modes;
  // optional legacy telemetry must never alter the animation or impose faults.
  effective_faults=0;
  const uint32_t us=micros();float dt=model_last_us?fminf(.05f,(uint32_t)(us-model_last_us)*.000001f):.001f;model_last_us=us;
  // Standalone presentation is independent of CAN; received telemetry remains untouched.
  Telemetry target=local_presentation(requested,now);
  const float k=1.f-expf(-dt/.12f);
  const float strength=requested.tv==0?0.f:requested.tv==1?.32f:requested.tv==2?.68f:1.f;
  visual.vector_strength+=(strength-visual.vector_strength)*(1.f-expf(-dt/.18f));
  visual.pulse_phase=fmodf(visual.pulse_phase+dt/(1.45f-.65f*visual.vector_strength),1.f);
  settle_axis(visual.front,visual.front_velocity,target.front,dt,19.f);
  settle_axis(visual.rear,visual.rear_velocity,target.rear_actual,dt,requested.rws==2?23.f:17.f);
  settle_axis(visual.aero,visual.aero_velocity,target.aero_actual,dt,16.f);
  visual.yaw+=(target.yaw-visual.yaw)*k;
  const float peak_decay=expf(-dt/.38f),tc_k=1.f-expf(-dt/.075f);
  visual.airbrake+=(0.f-visual.airbrake)*tc_k;
  for(uint8_t i=0;i<4;++i){
    float load=target.torque[i];
    // A transient can survive one telemetry frame as a restrained fading halo.
    visual.peak[i]=fmaxf(load,visual.peak[i]*peak_decay);
    visual.torque[i]+=(load-visual.torque[i])*k;
    visual.tc[i]+=(0.f-visual.tc[i])*tc_k;
  }
  can_publish_state(requested,effective_faults);
  if(count)can_request_state();
}
