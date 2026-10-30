#pragma once
// Local mode model. The dial is the presentation authority: CAN carries the selected modes; optional legacy
// telemetry (0x401) is still received and kept for tests, but it never drives the animation or imposes faults.
// V6.0.6.7: the situation (TorusContext.h: car state 0x100, Drive Mode 0x202) can lock options and runs Launch/Boost.
static RequestedState requested;
static Telemetry telemetry;
static uint32_t telemetry_revision=0,telemetry_received_at=0;
static bool telemetry_online=false;
static uint8_t effective_faults=0;
static uint8_t page_mode(uint8_t page){return page==0?requested.rws:page==1?requested.tv:requested.aero;}
static void apply_requested(uint8_t command,uint8_t value){
  if(!dde_valid_command(command,value))return;
  uint8_t *slot=command==0x10?&requested.page:command==0x11?&requested.rws:command==0x12?&requested.tv:&requested.aero;
  *slot=value;
}
static void model_service(){
  torus_context_service(millis());
  // V6.0.6.7: a setpoint the situation does not allow (RWS/TV Off at speed, Drift outside Track) is refused (ACK 1).
  twai_message_t commands[COMMAND_COUNT];uint8_t count=can_take_commands(commands);
  for(uint8_t i=0;i<count;++i){
    const bool locked=torus_command_locked(commands[i].data[0],commands[i].data[1]);
    if(!locked)apply_requested(commands[i].data[0],commands[i].data[1]);can_queue_ack(commands[i],locked?1:0);
  }
  // Drift needs Drive Mode Track: when Track ends (or the Drive Torus goes silent) the TV falls back to Sport.
  if(requested.tv==3 && torus.drive_mode!=DRIVE_TRACK)requested.tv=2;
  can_take_telemetry(telemetry,telemetry_revision,telemetry_received_at);
  telemetry_online=telemetry_revision && (uint32_t)(millis()-telemetry_received_at)<=OFFLINE_MS;
  // Byte 5 of the state (reserved faults until V6.0.6.6): bit 0 Launch armed, bit 1 Boost running.
  effective_faults=torus_status_bits();
  can_publish_state(requested,effective_faults);
  if(count)can_request_state();
}
