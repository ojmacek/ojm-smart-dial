#pragma once
#include <stdint.h>
#include <string.h>

// DDE v2 reserves three independent profiles; the transport is profile-agnostic.
struct DialProfile { uint16_t state,command,ack,telemetry; };
static constexpr DialProfile DYNAMICS_PROFILE = {0x201,0x301,0x211,0x401};
static constexpr DialProfile DRIVE_PROFILE = {0x202,0x302,0x212,0x402};
static constexpr DialProfile ENERGY_PROFILE = {0x203,0x303,0x213,0x403};
static constexpr DialProfile PROFILE = DYNAMICS_PROFILE;
static constexpr uint8_t DDE_MAGIC=0xD2, COMMAND_BASE=0x10, COMMAND_COUNT=4;
static constexpr uint32_t HEARTBEAT_MS=200, OFFLINE_MS=1200;
enum ButtonEvent : uint8_t { BUTTON_NONE, BUTTON_SHORT, BUTTON_LONG };
struct RequestedState { uint8_t page=2,rws=3,tv=1,aero=2; };
struct Telemetry {
  float speed=0,front=0,brake=0,rear_requested=0,rear_actual=0;
  uint8_t rws_actual=3;
  float torque[4]={0,0,0,0},yaw=0,aero_requested=0,aero_actual=0;
  uint8_t aero_mode=0,tc_mask=0,faults=0;
  float lateral=0,bias=0;
};
static uint8_t dde_checksum(const uint8_t *data,uint8_t length) {
  uint8_t result=0; for(uint8_t i=0;i<length;++i) result^=data[i]; return result;
}
static bool dde_valid_command(uint8_t command,uint8_t value) {
  if(command<COMMAND_BASE || command>=COMMAND_BASE+COMMAND_COUNT) return false;
  return value<(command==0x10?3:4);
}
static int16_t dde_i16(const uint8_t *p) { return (int16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8)); }
static bool dde_valid_fragment(const uint8_t *d) {
  if(d[0]<0xA0 || d[0]>0xA4 || dde_checksum(d,7)!=d[7]) return false;
  const uint8_t *p=d+2;
  switch(d[0]) {
    case 0xA0: return ((uint16_t)p[0]|((uint16_t)p[1]<<8))<=4000 && dde_i16(p+2)>=-450 && dde_i16(p+2)<=450 && p[4]<=100;
    case 0xA1: return dde_i16(p)>=-150 && dde_i16(p)<=150 && dde_i16(p+2)>=-150 && dde_i16(p+2)<=150 && p[4]<=3;
    case 0xA2: return p[0]<=100 && p[1]<=100 && p[2]<=100 && p[3]<=100 && (int8_t)p[4]>=-100 && (int8_t)p[4]<=100;
    case 0xA3: return p[0]<=100 && p[1]<=100 && p[2]<=3 && p[3]<=15 && p[4]<=3;
    case 0xA4: return dde_i16(p)>=-300 && dde_i16(p)<=300 && (int8_t)p[2]>=-100 && (int8_t)p[2]<=100 && p[3]==0 && p[4]==0;
  }
  return false;
}
struct TelemetryAssembler {
  uint8_t parts[5][5]={},mask=0,generation=0;
  bool known=false,complete=false;
  uint32_t last_at=0;
  bool accept(const uint8_t *d,uint32_t now,Telemetry &out) {
    if(!dde_valid_fragment(d)) return false;
    if((uint32_t)(now-last_at)>300) {known=false;mask=0;complete=false;}
    uint8_t delta=d[1]-generation;
    if(known && (delta>=128 || (delta==0 && complete))) return false;
    if(!known || d[1]!=generation) {mask=0;complete=false;generation=d[1];known=true;}
    last_at=now;
    memcpy(parts[d[0]-0xA0],d+2,5); mask|=1U<<(d[0]-0xA0);
    if(mask!=31) return false;
    complete=true;
    out.speed=((uint16_t)parts[0][0]|((uint16_t)parts[0][1]<<8))*.1f;
    out.front=dde_i16(parts[0]+2)*.1f;out.brake=parts[0][4];
    out.rear_requested=dde_i16(parts[1])*.1f;out.rear_actual=dde_i16(parts[1]+2)*.1f;out.rws_actual=parts[1][4];
    for(uint8_t i=0;i<4;++i) out.torque[i]=parts[2][i];
    out.yaw=(int8_t)parts[2][4];out.aero_requested=parts[3][0];out.aero_actual=parts[3][1];
    out.aero_mode=parts[3][2];out.tc_mask=parts[3][3];out.faults=parts[3][4];
    out.lateral=dde_i16(parts[4])*.01f;out.bias=(int8_t)parts[4][2];return true;
  }
};
