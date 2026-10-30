#pragma once
// V6.0.6.7 "the Torus knows the situation" (Performance Torus). Inputs: the car's state (CAN 0x100: park/drive/
// charging, speed, brake) and the Drive Torus mode (0x202). Outputs: which options are locked, what the Auto
// modes pick, and the quick action of a long press in the menu (Launch when standing, Boost when driving).
// Without a car frame the Torus behaves as standing: everything works as before, only Boost is not offered.
// The concept may still change: every threshold and time is in this table, the logic below only reads it.
struct TorusTuning {
  float stand_kmh=1;                  // below: standing (Launch offered)
  float launch_go_kmh=5;              // Launch armed and the car moves: launched
  float boost_min_kmh=20;             // Boost only while driving at least this fast
  float off_lock_kmh=120,off_unlock_kmh=115;  // RWS Off and TV Off cannot be chosen above (hysteresis)
  float aero_downforce_kmh=80,aero_downforce_off_kmh=75;  // Aero Auto: Low Drag below, Downforce above
  uint8_t aero_brake_pct=40,aero_brake_off_pct=25;        // Aero Auto: Air Brake while braking this hard...
  float aero_brake_kmh=60,aero_brake_off_kmh=40;          // ...at least this fast
  float rws_stable_kmh=60,rws_stable_off_kmh=55;          // RWS Auto: Agile below, Stable above
  uint32_t vehicle_timeout_ms=500,drive_timeout_ms=1200;
  uint32_t launch_timeout_ms=30000,boost_ms=10000,boost_cooldown_ms=20000;
};
static constexpr TorusTuning TUNING{};

enum TorusAction : uint8_t { ACTION_NONE, ACTION_LAUNCH, ACTION_BOOST };
enum TorusEvent : uint8_t { EVENT_NONE, EVENT_LAUNCH_ARMED, EVENT_LAUNCH_GO, EVENT_LAUNCH_CANCEL, EVENT_BOOST, EVENT_BOOST_END, EVENT_REFUSED };
// UI positions (visual order): Aero 0 Auto, 1 Low Drag, 2 Downforce, 3 Air Brake; RWS 0 Off, 1 Auto, 2 Stable,
// 3 Agile; TV 0 Off, 1 Auto, 2 Sport, 3 Drift.
struct TorusContext {
  bool vehicle_known=false;VehicleState vehicle;uint8_t drive_mode=DRIVE_UNKNOWN;
  bool off_locked=false;
  uint8_t aero_auto=0;  // Aero position that Auto shows; 0 = the Auto rest look (no car data)
  uint8_t rws_auto=1;   // RWS position that Auto shows; 1 = neutral (no car data)
  uint8_t action=ACTION_NONE;uint32_t action_at=0,cooldown_until=0;
  uint8_t event=EVENT_NONE;uint32_t event_seq=0;
  bool dev=false;VehicleState dev_vehicle;uint8_t dev_drive=DRIVE_UNKNOWN;  // Serial simulation (development)
};
static TorusContext torus;

static bool torus_standing(){return !torus.vehicle_known || torus.vehicle.speed<TUNING.stand_kmh;}
static void torus_event(uint8_t e){torus.event=e;++torus.event_seq;}
// Locks of the Performance options (fw page 0 RWS, 1 TV, 2 Aero; UI positions).
static bool torus_locked(uint8_t fw_page,uint8_t position){
  if(fw_page==0)return position==0 && torus.off_locked;
  if(fw_page==1)return (position==0 && torus.off_locked) || (position==3 && torus.drive_mode!=DRIVE_TRACK);
  return false;
}
// The same for a CAN setpoint (command code and CAN value): RWS OFF=0, TV OFF=0, TV DRIFT=3.
static bool torus_command_locked(uint8_t command,uint8_t value){
  if(command==0x11)return value==0 && torus.off_locked;
  if(command==0x12)return (value==0 && torus.off_locked) || (value==3 && torus.drive_mode!=DRIVE_TRACK);
  return false;
}
// Why an option is locked, shown for 0.8 s when the knob bounces on it (the speed text follows TUNING).
static const char *torus_lock_reason(uint8_t fw_page,uint8_t position){
  static char speed[24]="";if(!speed[0])snprintf(speed,sizeof(speed),"Above %d km/h",(int)TUNING.off_lock_kmh);
  static const char *const track="Track only";
  return fw_page==1 && position==3?track:speed;
}
// What a long press in the menu would do now.
static uint8_t torus_available_action(uint32_t now){
  if(torus.action!=ACTION_NONE)return ACTION_NONE;
  if(torus_standing())return torus.vehicle.mode==VEHICLE_CHARGING && torus.vehicle_known?ACTION_NONE:ACTION_LAUNCH;
  if(torus.vehicle.mode==VEHICLE_DRIVE && torus.vehicle.speed>=TUNING.boost_min_kmh && (int32_t)(now-torus.cooldown_until)>=0)return ACTION_BOOST;
  return ACTION_NONE;
}
static void torus_long_press(uint32_t now){
  const uint8_t a=torus_available_action(now);
  if(a==ACTION_NONE){torus_event(EVENT_REFUSED);return;}
  torus.action=a;torus.action_at=now;torus_event(a==ACTION_LAUNCH?EVENT_LAUNCH_ARMED:EVENT_BOOST);
}
// Any knob input while Launch is armed cancels it (and is consumed). Boost keeps running.
static bool torus_cancel_input(){
  if(torus.action!=ACTION_LAUNCH)return false;
  torus.action=ACTION_NONE;torus_event(EVENT_LAUNCH_CANCEL);return true;
}
static void torus_end_boost(uint32_t now){torus.action=ACTION_NONE;torus.cooldown_until=now+TUNING.boost_cooldown_ms;torus_event(EVENT_BOOST_END);}
// Status bits for the state frame byte 5 (bit 0 Launch armed, bit 1 Boost running).
static uint8_t torus_status_bits(){return (torus.action==ACTION_LAUNCH?1:0)|(torus.action==ACTION_BOOST?2:0);}

static void torus_context_service(uint32_t now){
  CanContext c;can_take_context(c);
  const bool vehicle_fresh=c.vehicle_revision && (uint32_t)(now-c.vehicle_at)<=TUNING.vehicle_timeout_ms;
  const bool drive_fresh=c.drive_revision && (uint32_t)(now-c.drive_at)<=TUNING.drive_timeout_ms;
  if(torus.dev){torus.vehicle_known=true;torus.vehicle=torus.dev_vehicle;torus.drive_mode=torus.dev_drive;}
  else{torus.vehicle_known=vehicle_fresh;torus.vehicle=vehicle_fresh?c.vehicle:VehicleState{};torus.drive_mode=drive_fresh?c.drive_mode:DRIVE_UNKNOWN;}
  const VehicleState &v=torus.vehicle;const bool known=torus.vehicle_known;
  torus.off_locked=known && (torus.off_locked?v.speed>TUNING.off_unlock_kmh:v.speed>TUNING.off_lock_kmh);
  // Aero Auto: Air Brake while braking hard at speed, otherwise Downforce when fast and Low Drag when slow.
  if(!known)torus.aero_auto=0;
  else{
    const bool braking=torus.aero_auto==3?v.brake>=TUNING.aero_brake_off_pct && v.speed>=TUNING.aero_brake_off_kmh:
      v.brake>=TUNING.aero_brake_pct && v.speed>=TUNING.aero_brake_kmh;
    const bool fast=torus.aero_auto==2 || torus.aero_auto==3?v.speed>=TUNING.aero_downforce_off_kmh:v.speed>=TUNING.aero_downforce_kmh;
    torus.aero_auto=braking?3:fast?2:1;
  }
  // RWS Auto: rear wheels against the front when slow (Agile), with them when fast (Stable).
  if(!known)torus.rws_auto=1;
  else torus.rws_auto=(torus.rws_auto==2?v.speed>=TUNING.rws_stable_off_kmh:v.speed>=TUNING.rws_stable_kmh)?2:3;
  // The quick action.
  if(torus.action==ACTION_LAUNCH){
    if(known && v.speed>=TUNING.launch_go_kmh){torus.action=ACTION_NONE;torus_event(EVENT_LAUNCH_GO);}
    else if((uint32_t)(now-torus.action_at)>=TUNING.launch_timeout_ms || (known && v.mode==VEHICLE_CHARGING)){torus.action=ACTION_NONE;torus_event(EVENT_LAUNCH_CANCEL);}
  }else if(torus.action==ACTION_BOOST){
    if((uint32_t)(now-torus.action_at)>=TUNING.boost_ms || !known || v.speed<TUNING.stand_kmh)torus_end_boost(now);
  }
}
// Development (Serial): '0'..'9' drive at 0..225 km/h, 'b' brake, 'p' charging, 'k' Drive Mode, 'x' back to CAN.
static bool torus_dev_key(int c){
  auto &d=torus.dev_vehicle;
  if(c>='0' && c<='9'){torus.dev=true;d.speed=(c-'0')*25.f;if(d.mode!=VEHICLE_CHARGING)d.mode=d.speed>0?VEHICLE_DRIVE:VEHICLE_PARK;return true;}
  if(c=='b'){torus.dev=true;d.brake=d.brake?0:80;return true;}
  if(c=='p'){torus.dev=true;d.mode=d.mode==VEHICLE_CHARGING?VEHICLE_PARK:VEHICLE_CHARGING;if(d.mode==VEHICLE_CHARGING)d.speed=0;return true;}
  if(c=='k'){torus.dev=true;torus.dev_drive=torus.dev_drive==DRIVE_UNKNOWN?DRIVE_COMFORT:torus.dev_drive>=DRIVE_TRACK?DRIVE_UNKNOWN:torus.dev_drive+1;return true;}
  if(c=='x'){torus.dev=false;torus.dev_vehicle=VehicleState{};torus.dev_drive=DRIVE_UNKNOWN;return true;}
  return false;
}
