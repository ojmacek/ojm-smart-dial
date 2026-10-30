#pragma once
// Host-only shim. No hardware claims: GPIO/TE and QSPI are replaced by memory.
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define PROGMEM
#define memcpy_P memcpy
#define pgm_read_byte(p) (*(const uint8_t *)(p))
#define pgm_read_word(p) (*(const uint16_t *)(p))
#define PI 3.14159265358979323846f
#define DEG_TO_RAD (PI/180.f)
template<class T,class U> auto min(T a,U b)->decltype(a+b){return a<b?a:b;}
template<class T,class U> auto max(T a,U b)->decltype(a+b){return a>b?a:b;}
extern uint32_t host_us;
inline uint32_t micros(){return host_us;}
inline uint32_t millis(){return host_us/1000;}
