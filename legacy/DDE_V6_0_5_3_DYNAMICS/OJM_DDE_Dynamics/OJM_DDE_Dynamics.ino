#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESP32Encoder.h>
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_heap_caps.h"
#include "src/SmartDial_Fonts_DINish.h"
#include "src/DdeProtocol.h"

// Hardware, QSPI, CO5300 offsets, TE and encoder setup retained from V5.6.1.
#define TFT_RST 8
#define TFT_CS 10
#define TFT_SCLK 11
#define TFT_D0 12
#define TFT_D1 13
#define TFT_D2 14
#define TFT_D3 15
#define TFT_TE 9
#define ENCODER_A 1
#define ENCODER_B 2
#define ENCODER_BUTTON 0
#define ENCODER_REVERSED 0
#define ENCODER_TRANSITIONS_PER_STEP 4
#define CAN_TX_GPIO 43
#define CAN_RX_GPIO 44
#define LCD_W 466
#define LCD_H 466
#define LCD_CX 233
#define LCD_CY 233
#define DISPLAY_BUS_HZ 80000000L
#define DISPLAY_BRIGHTNESS 225
#define GFX_BITMAP_BIG_ENDIAN 0
#define USE_TE_SYNC 1
#define TE_WAIT_TIMEOUT_US 18000UL
#define BUTTON_DEBOUNCE_MS 24UL
#define BUTTON_LONG_MS 700UL
Arduino_DataBus *bus=new Arduino_ESP32QSPI(TFT_CS,TFT_SCLK,TFT_D0,TFT_D1,TFT_D2,TFT_D3);
Arduino_GFX *display=new Arduino_CO5300(bus,TFT_RST,0,LCD_W,LCD_H,9,0,0,0);
static constexpr size_t FRAME_PIXELS=(size_t)LCD_W*LCD_H,FRAME_BYTES=FRAME_PIXELS*sizeof(uint16_t);
static constexpr size_t PUSH_PIXELS=(size_t)LCD_W*LCD_H,PUSH_BYTES=PUSH_PIXELS*sizeof(uint16_t);
static uint16_t *g_base_frame=nullptr,*g_frame=nullptr,*g_push_buffer=nullptr;
#include "src/BaselineGraphics.h"
#include "src/BaselineShapes.h"
#include "src/BaselineInput.h"
#include "src/CanTransport.h"
#include "src/DynamicsModel.h"
#include "src/DdeNavigation.h"
#include "src/DdeRenderer.h"

// HLAVNI TAB PRO OBHAJOBU
// setup(): puvodni hardware, framebuffery a CAN task.
// loop(): lokalni vstup -> model -> publikace stavu -> vykresleni.
// src/ oddeluje navigaci, lokalni model, renderer a transport od grafickych dat.
// Dial funguje bez ECU. PC pouze meni a zrcadli rezimy pres CAN.

void setup(){
  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);
#endif
  pinMode(ENCODER_BUTTON,INPUT_PULLUP);
  ESP32Encoder::useInternalWeakPullResistors=puType::up;
  encoder.attachFullQuad(ENCODER_A,ENCODER_B);encoder.setFilter(1023);encoder.clearCount();
  g_encoder_last_raw=0;g_encoder_substeps=0;
  if(!display->begin(DISPLAY_BUS_HZ)){Serial.println("FATAL: display begin failed");while(true)delay(1000);}
  enable_te_output();
  g_base_frame=(uint16_t*)heap_caps_malloc(FRAME_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  g_frame=(uint16_t*)heap_caps_malloc(FRAME_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  g_push_buffer=(uint16_t*)heap_caps_malloc(PUSH_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!g_base_frame || !g_frame || !g_push_buffer){Serial.println("FATAL: PSRAM allocation failed");while(true)delay(1000);}
  memset(g_base_frame,0,FRAME_BYTES);memset(g_frame,0,FRAME_BYTES);
  set_brightness(0);display->fillScreen(0);
  // Panel settling retained; no multi-second logo delay. Startup runs in loop().
  delay(220);set_brightness(DISPLAY_BRIGHTNESS);delay(45);
  can_publish_state(requested,0);can_begin();startup_started=millis();
  Serial.println("OJM SYSTEMS DDE V6.0.5.2 / DYNAMICS / CAN v2");
}
void loop(){
  // Input always precedes model/ACK application and rendering. No CAN driver waits here.
  int16_t delta=consume_encoder_delta();ButtonEvent button=read_encoder_button();
  model_service(); // Drain already-received remote requests; newest local input then wins.
  if(delta || button!=BUTTON_NONE){if(startup_active){startup_active=false;base_invalid=true;}}
  navigation_input(delta,button);
  can_publish_state(requested,effective_faults);render_tick();delay(1);
}
