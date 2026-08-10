#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESP32Encoder.h>
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_heap_caps.h"
#include "SmartDial_Fonts_DINish.h"

// Smart Dial V5.5.2 firmware for the Waveshare ESP32-S3 AMOLED 1.32.
// Arduino_GFX handles the UI. LVGL is only used by logomodre256.c.
// Keep the .ino, SmartDial_Fonts_DINish.h and logomodre256.c together.
#define USE_EXISTING_LVGL_LOGO 1

#if USE_EXISTING_LVGL_LOGO
  #include <lvgl.h>
  LV_IMG_DECLARE(logomodre256);

  #if LV_COLOR_DEPTH != 16
    #error "The existing logomodre256 asset requires LV_COLOR_DEPTH 16."
  #endif
#endif

// ============================================================
// HARDWARE: Waveshare ESP32-S3-Touch-AMOLED-1.32 / CO5300
// ============================================================
#define TFT_RST   8
#define TFT_CS    10
#define TFT_SCLK  11
#define TFT_D0    12
#define TFT_D1    13
#define TFT_D2    14
#define TFT_D3    15
#define TFT_TE    9

#define ENCODER_A       1
#define ENCODER_B       2
#define ENCODER_BUTTON  0

#define ENCODER_REVERSED              0
#define ENCODER_TRANSITIONS_PER_STEP  4

#define CAN_TX_GPIO  43
#define CAN_RX_GPIO  44

#define CAN_STATE_ID      0x201
#define CAN_COMMAND_ID    0x301
#define CAN_TX_PERIOD_MS  200UL
#define CAN_RX_BUDGET     16
#define CAN_TX_TASK_STACK 3072
#define CAN_TX_TASK_PRIORITY 3

#define LCD_W   466
#define LCD_H   466
#define LCD_CX  (LCD_W / 2)
#define LCD_CY  (LCD_H / 2)

#define DISPLAY_BUS_HZ      80000000L
#define DISPLAY_BRIGHTNESS  225

// Leave this at 0 for RGB565 buffers created by this sketch.
// Change only if the bitmap test itself shows swapped colors.
#define GFX_BITMAP_BIG_ENDIAN  0

#define USE_TE_SYNC         1
#define TE_WAIT_TIMEOUT_US  18000UL

#define BUTTON_DEBOUNCE_MS  24UL
#define BUTTON_LONG_MS      700UL

#define PAGE_TRANSITION_MS      92UL
#define PAGE_TRANSITION_FRAMES  6
#define VISUAL_ANIMATION_MS     88UL
#define VISUAL_ANIMATION_FRAMES 5

// ============================================================
// VISUAL SYSTEM
// ============================================================
// Black AMOLED background + white, FR red and electric blue.
// Dim elements are lower luminance of the same three colors.
static constexpr uint16_t COL_BLACK = 0x0000;
static constexpr uint16_t COL_WHITE = 0xFFDF;  // 248, 249, 252
static constexpr uint16_t COL_RED   = 0xF908;  // 255,  35,  69
static constexpr uint16_t COL_BLUE  = 0x247F;  //  37, 141, 255

static constexpr int16_t ARC_OUTER_RADIUS = 211;
static constexpr int16_t ARC_INNER_RADIUS = 197;
static constexpr float ARC_START_DEG = 135.0f;
static constexpr float ARC_SPAN_DEG  = 270.0f;
static constexpr int16_t ARC_MARKER_RADIUS = 204;

static constexpr int16_t CONTENT_X = 48;
static constexpr int16_t CONTENT_Y = 101;
static constexpr int16_t CONTENT_W = 370;
static constexpr int16_t CONTENT_H = 224;

static constexpr int16_t VISUAL_X = 133;
static constexpr int16_t VISUAL_Y = 322;
static constexpr int16_t VISUAL_W = 200;
static constexpr int16_t VISUAL_H = 61;

static constexpr int16_t PAGE_RAIL_X = 176;
static constexpr int16_t PAGE_RAIL_Y = 386;
static constexpr int16_t PAGE_RAIL_W = 114;
static constexpr int16_t PAGE_RAIL_H = 26;
static constexpr int16_t PAGE_RAIL_FIRST_X = 194;
static constexpr int16_t PAGE_RAIL_SPACING = 26;

static constexpr int16_t MARKER_PATCH = 44;

// ============================================================
// DISPLAY
// ============================================================
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  TFT_CS, TFT_SCLK, TFT_D0, TFT_D1, TFT_D2, TFT_D3
);

Arduino_GFX *display = new Arduino_CO5300(
  bus,
  TFT_RST,
  0,
  LCD_W,
  LCD_H,
  9, 0, 0, 0
);

// base: static page background without center content or marker
// frame: currently composed visible frame
// push: contiguous staging buffer for a partial rectangle
static uint16_t *g_base_frame = nullptr;
static uint16_t *g_frame = nullptr;
static uint16_t *g_push_buffer = nullptr;

static constexpr size_t FRAME_PIXELS = (size_t)LCD_W * LCD_H;
static constexpr size_t FRAME_BYTES = FRAME_PIXELS * sizeof(uint16_t);
static constexpr int16_t STARTUP_PUSH_X = 16;
static constexpr int16_t STARTUP_PUSH_Y = 16;
static constexpr int16_t STARTUP_PUSH_W = 434;
static constexpr int16_t STARTUP_PUSH_H = 374;

static constexpr size_t CONTENT_PUSH_PIXELS =
  (size_t)CONTENT_W * CONTENT_H;

static constexpr size_t STARTUP_PUSH_PIXELS =
  (size_t)STARTUP_PUSH_W * STARTUP_PUSH_H;

static constexpr size_t PUSH_PIXELS =
  STARTUP_PUSH_PIXELS > CONTENT_PUSH_PIXELS
    ? STARTUP_PUSH_PIXELS
    : CONTENT_PUSH_PIXELS;

static constexpr size_t PUSH_BYTES =
  PUSH_PIXELS * sizeof(uint16_t);

// ============================================================
// APP STATE
// ============================================================
enum PageId : int8_t {
  PAGE_TEMP = 0,
  PAGE_FAN,
  PAGE_AIRFLOW,
  PAGE_SEAT,
  PAGE_COUNT
};

enum ButtonEvent : uint8_t {
  BUTTON_NONE = 0,
  BUTTON_SHORT,
  BUTTON_LONG
};

static constexpr int8_t FAN_AUTO = -1;

struct ClimateState {
  bool power_on;
  bool ac_on;
  float temperature;
  int8_t fan;
  uint8_t airflow;
  uint8_t seat;
};

// Keep this type with the global declarations. Arduino generates function
// prototypes before compiling the sketch, so the type must already be known.
struct CanCommandBatch {
  bool accepted;
  bool changed;
  bool full_rebuild;
  bool temperature_marker_may_move;
  uint8_t dirty_pages;
  uint8_t accepted_count;
  uint8_t last_command;
  uint8_t last_value;
};

static ClimateState climate = {
  true,
  true,
  22.0f,
  FAN_AUTO,
  0,
  0
};

static int8_t g_page = PAGE_TEMP;
static int8_t g_requested_page = PAGE_TEMP;
static bool g_edit_mode = false;

static bool g_transition_active = false;
static uint32_t g_transition_started = 0;
static uint8_t g_transition_last_frame = 255;
static float g_page_indicator_x = PAGE_RAIL_FIRST_X;
static float g_transition_from_x = PAGE_RAIL_FIRST_X;
static float g_transition_to_x = PAGE_RAIL_FIRST_X;

static bool g_visual_animation_active = false;
static uint32_t g_visual_animation_started = 0;
static uint8_t g_visual_animation_last_frame = 255;
static float g_visual_animation_phase = 1.0f;
static bool g_animation_te_used = false;

static bool g_marker_visible = false;
static int16_t g_marker_x = LCD_CX;
static int16_t g_marker_y = LCD_CY;

static volatile bool g_can_ready = false;
static volatile bool g_can_recovering = false;
static uint8_t g_can_alive_counter = 0;
static volatile uint32_t g_can_last_tx = 0;
static volatile uint32_t g_can_tx_failures = 0;
static TaskHandle_t g_can_tx_task_handle = nullptr;
static portMUX_TYPE g_can_state_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t g_can_state_snapshot[6] = {};

// ============================================================
// SMALL HELPERS
// ============================================================
static int32_t clamp_i32(int32_t value, int32_t low, int32_t high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

static float clamp_float(float value, float low, float high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

static uint8_t mul_alpha(uint8_t a, uint8_t b) {
  return (uint8_t)(((uint16_t)a * b + 127U) / 255U);
}

static void set_brightness(uint8_t value) {
  ((Arduino_CO5300 *)display)->setBrightness(value);
}

// ============================================================
// TE + RGB565 TRANSFER
// ============================================================
static void enable_te_output() {
#if USE_TE_SYNC
  pinMode(TFT_TE, INPUT);

  bus->beginWrite();
  bus->writeC8D8(0x35, 0x00);
  bus->endWrite();
#endif
}

static void wait_for_te_edge() {
#if USE_TE_SYNC
  const uint32_t start = micros();

  while (gpio_get_level((gpio_num_t)TFT_TE) != 0) {
    if ((micros() - start) > TE_WAIT_TIMEOUT_US) return;
  }

  while (gpio_get_level((gpio_num_t)TFT_TE) == 0) {
    if ((micros() - start) > TE_WAIT_TIMEOUT_US) return;
  }
#endif
}

static void push_bitmap(
  int16_t x,
  int16_t y,
  uint16_t *pixels,
  int16_t width,
  int16_t height
) {
#if GFX_BITMAP_BIG_ENDIAN
  display->draw16bitBeRGBBitmap(x, y, pixels, width, height);
#else
  display->draw16bitRGBBitmap(x, y, pixels, width, height);
#endif
}

static void push_full_frame(bool sync_to_te = true) {
  if (sync_to_te) wait_for_te_edge();
  push_bitmap(0, 0, g_frame, LCD_W, LCD_H);
}

static void push_frame_rect(
  int16_t x,
  int16_t y,
  int16_t width,
  int16_t height,
  bool sync_to_te
) {
  if (x < 0) {
    width += x;
    x = 0;
  }
  if (y < 0) {
    height += y;
    y = 0;
  }
  if (x + width > LCD_W) width = LCD_W - x;
  if (y + height > LCD_H) height = LCD_H - y;
  if (width <= 0 || height <= 0) return;

  const size_t pixels = (size_t)width * height;
  if (pixels > PUSH_PIXELS) return;

  for (int16_t row = 0; row < height; ++row) {
    memcpy(
      g_push_buffer + (size_t)row * width,
      g_frame + (size_t)(y + row) * LCD_W + x,
      (size_t)width * sizeof(uint16_t)
    );
  }

  if (sync_to_te) wait_for_te_edge();
  push_bitmap(x, y, g_push_buffer, width, height);
}

// ============================================================
// FRAMEBUFFER PRIMITIVES
// ============================================================
static uint16_t blend_rgb565(
  uint16_t destination,
  uint16_t source,
  uint8_t alpha
) {
  if (alpha == 0) return destination;
  if (alpha == 255) return source;

  const int32_t dr = (destination >> 11) & 0x1F;
  const int32_t dg = (destination >> 5) & 0x3F;
  const int32_t db = destination & 0x1F;

  const int32_t sr = (source >> 11) & 0x1F;
  const int32_t sg = (source >> 5) & 0x3F;
  const int32_t sb = source & 0x1F;

  const int32_t r = dr + ((sr - dr) * alpha + 127) / 255;
  const int32_t g = dg + ((sg - dg) * alpha + 127) / 255;
  const int32_t b = db + ((sb - db) * alpha + 127) / 255;

  return (uint16_t)((r << 11) | (g << 5) | b);
}

static void blend_pixel(
  uint16_t *target,
  int16_t x,
  int16_t y,
  uint16_t color,
  uint8_t alpha
) {
  if ((uint16_t)x >= LCD_W || (uint16_t)y >= LCD_H || alpha == 0) return;

  uint16_t &pixel = target[(size_t)y * LCD_W + x];
  pixel = blend_rgb565(pixel, color, alpha);
}

static void fill_rect(
  uint16_t *target,
  int16_t x,
  int16_t y,
  int16_t width,
  int16_t height,
  uint16_t color
) {
  if (x < 0) {
    width += x;
    x = 0;
  }
  if (y < 0) {
    height += y;
    y = 0;
  }
  if (x + width > LCD_W) width = LCD_W - x;
  if (y + height > LCD_H) height = LCD_H - y;
  if (width <= 0 || height <= 0) return;

  for (int16_t row = 0; row < height; ++row) {
    uint16_t *line = target + (size_t)(y + row) * LCD_W + x;
    for (int16_t column = 0; column < width; ++column) {
      line[column] = color;
    }
  }
}

static void copy_frame_rect(
  uint16_t *destination,
  const uint16_t *source,
  int16_t x,
  int16_t y,
  int16_t width,
  int16_t height
) {
  if (x < 0) {
    width += x;
    x = 0;
  }
  if (y < 0) {
    height += y;
    y = 0;
  }
  if (x + width > LCD_W) width = LCD_W - x;
  if (y + height > LCD_H) height = LCD_H - y;
  if (width <= 0 || height <= 0) return;

  for (int16_t row = 0; row < height; ++row) {
    memcpy(
      destination + (size_t)(y + row) * LCD_W + x,
      source + (size_t)(y + row) * LCD_W + x,
      (size_t)width * sizeof(uint16_t)
    );
  }
}

static void fill_circle_aa(
  uint16_t *target,
  float center_x,
  float center_y,
  float radius,
  uint16_t color,
  uint8_t opacity = 255
) {
  const int16_t x0 = (int16_t)floorf(center_x - radius - 1.0f);
  const int16_t x1 = (int16_t)ceilf(center_x + radius + 1.0f);
  const int16_t y0 = (int16_t)floorf(center_y - radius - 1.0f);
  const int16_t y1 = (int16_t)ceilf(center_y + radius + 1.0f);

  for (int16_t y = y0; y <= y1; ++y) {
    for (int16_t x = x0; x <= x1; ++x) {
      const float dx = (x + 0.5f) - center_x;
      const float dy = (y + 0.5f) - center_y;
      const float distance = sqrtf(dx * dx + dy * dy);
      const float coverage = clamp_float(radius + 0.5f - distance, 0.0f, 1.0f);
      const uint8_t alpha = mul_alpha(
        opacity,
        (uint8_t)lroundf(coverage * 255.0f)
      );
      blend_pixel(target, x, y, color, alpha);
    }
  }
}

static void draw_ring_aa(
  uint16_t *target,
  float center_x,
  float center_y,
  float outer_radius,
  float inner_radius,
  uint16_t color,
  uint8_t opacity
) {
  const int16_t x0 = (int16_t)floorf(center_x - outer_radius - 1.0f);
  const int16_t x1 = (int16_t)ceilf(center_x + outer_radius + 1.0f);
  const int16_t y0 = (int16_t)floorf(center_y - outer_radius - 1.0f);
  const int16_t y1 = (int16_t)ceilf(center_y + outer_radius + 1.0f);

  for (int16_t y = y0; y <= y1; ++y) {
    for (int16_t x = x0; x <= x1; ++x) {
      const float dx = (x + 0.5f) - center_x;
      const float dy = (y + 0.5f) - center_y;
      const float radius = sqrtf(dx * dx + dy * dy);

      const float outer_coverage =
        clamp_float(outer_radius + 0.5f - radius, 0.0f, 1.0f);
      const float inner_coverage =
        clamp_float(radius - inner_radius + 0.5f, 0.0f, 1.0f);
      const float coverage = fminf(outer_coverage, inner_coverage);

      const uint8_t alpha = mul_alpha(
        opacity,
        (uint8_t)lroundf(coverage * 255.0f)
      );
      blend_pixel(target, x, y, color, alpha);
    }
  }
}

// ============================================================
// EMBEDDED ANTIALIASED FONT RENDERER
// ============================================================
static bool find_glyph(
  const SDFont &font,
  uint8_t code,
  SDGlyph &result
) {
  for (uint16_t index = 0; index < font.glyph_count; ++index) {
    SDGlyph glyph;
    memcpy_P(&glyph, font.glyphs + index, sizeof(SDGlyph));

    if (glyph.code == code) {
      result = glyph;
      return true;
    }
  }

  return false;
}

static int16_t measure_text(
  const SDFont &font,
  const char *text,
  int8_t letter_spacing = 0
) {
  int16_t width = 0;
  uint16_t count = 0;

  while (*text) {
    SDGlyph glyph;
    if (find_glyph(font, (uint8_t)*text, glyph)) {
      width += glyph.advance;
      count++;
    }
    text++;
  }

  if (count > 1) width += (count - 1) * letter_spacing;
  return width;
}

static void draw_glyph(
  uint16_t *target,
  const SDFont &font,
  const SDGlyph &glyph,
  int16_t cursor_x,
  int16_t baseline_y,
  uint16_t color,
  uint8_t opacity
) {
  if (glyph.width == 0 || glyph.height == 0 || glyph.data_length == 0) return;

  uint32_t pixel_index = 0;
  const uint32_t pixel_count = (uint32_t)glyph.width * glyph.height;

  for (uint16_t index = 0; index < glyph.data_length; ++index) {
    const uint8_t packed = pgm_read_byte(
      font.data + glyph.data_offset + index
    );
    const uint8_t run = (packed >> 4) + 1;
    const uint8_t alpha4 = packed & 0x0F;
    const uint8_t glyph_alpha = alpha4 * 17;
    const uint8_t final_alpha = mul_alpha(glyph_alpha, opacity);

    for (uint8_t step = 0; step < run && pixel_index < pixel_count; ++step) {
      if (final_alpha != 0) {
        const int16_t local_x = pixel_index % glyph.width;
        const int16_t local_y = pixel_index / glyph.width;

        blend_pixel(
          target,
          cursor_x + glyph.x_offset + local_x,
          baseline_y + glyph.y_offset + local_y,
          color,
          final_alpha
        );
      }

      pixel_index++;
    }
  }
}

static void draw_text(
  uint16_t *target,
  const SDFont &font,
  const char *text,
  int16_t x,
  int16_t baseline_y,
  uint16_t color,
  uint8_t opacity = 255,
  int8_t letter_spacing = 0
) {
  int16_t cursor_x = x;

  while (*text) {
    SDGlyph glyph;
    if (find_glyph(font, (uint8_t)*text, glyph)) {
      draw_glyph(
        target,
        font,
        glyph,
        cursor_x,
        baseline_y,
        color,
        opacity
      );
      cursor_x += glyph.advance + letter_spacing;
    }
    text++;
  }
}

static int16_t draw_text_centered(
  uint16_t *target,
  const SDFont &font,
  const char *text,
  int16_t center_x,
  int16_t baseline_y,
  uint16_t color,
  uint8_t opacity = 255,
  int8_t letter_spacing = 0,
  int16_t x_offset = 0
) {
  const int16_t width = measure_text(font, text, letter_spacing);
  const int16_t x = center_x - width / 2 + x_offset;

  draw_text(
    target,
    font,
    text,
    x,
    baseline_y,
    color,
    opacity,
    letter_spacing
  );

  return x + width;
}

// ============================================================
// STATIC ARC DESIGN
// ============================================================
static float normalized_angle(float degrees) {
  while (degrees < 0.0f) degrees += 360.0f;
  while (degrees >= 360.0f) degrees -= 360.0f;
  return degrees;
}

static bool inside_segment(float value, float start, float end) {
  return value >= start && value <= end;
}

static void arc_style_at(
  float relative_angle,
  uint16_t &color,
  uint8_t &opacity
) {
  color = COL_WHITE;
  opacity = climate.power_on ? 42 : 34;

  if (!climate.power_on) return;

  // The rail stays the same on every page to avoid large QSPI updates.
  if (inside_segment(relative_angle, 0.0f, 111.0f)) {
    color = COL_BLUE;
    opacity = 255;
  } else if (inside_segment(relative_angle, 116.0f, 154.0f)) {
    color = COL_WHITE;
    opacity = 255;
  } else if (inside_segment(relative_angle, 159.0f, 270.0f)) {
    color = COL_RED;
    opacity = 255;
  }
}

static void render_arc_base(uint16_t *target) {
  const int16_t margin = ARC_OUTER_RADIUS + 2;

  const float min_radius = ARC_INNER_RADIUS - 0.5f;
  const float max_radius = ARC_OUTER_RADIUS + 0.5f;
  const float min_radius_sq = min_radius * min_radius;
  const float max_radius_sq = max_radius * max_radius;

  for (int16_t y = LCD_CY - margin; y <= LCD_CY + margin; ++y) {
    for (int16_t x = LCD_CX - margin; x <= LCD_CX + margin; ++x) {
      if ((uint16_t)x >= LCD_W || (uint16_t)y >= LCD_H) continue;

      const float dx = (x + 0.5f) - LCD_CX;
      const float dy = (y + 0.5f) - LCD_CY;
      const float radius_sq = dx * dx + dy * dy;

      if (radius_sq < min_radius_sq || radius_sq > max_radius_sq) {
      continue;
      }

      const float radius = sqrtf(radius_sq);

      const float outer_coverage = clamp_float(
        ARC_OUTER_RADIUS + 0.5f - radius,
        0.0f,
        1.0f
      );
      const float inner_coverage = clamp_float(
        radius - ARC_INNER_RADIUS + 0.5f,
        0.0f,
        1.0f
      );
      const float radial_coverage = fminf(
        outer_coverage,
        inner_coverage
      );
      if (radial_coverage <= 0.0f) continue;

      const float angle = normalized_angle(atan2f(dy, dx) * RAD_TO_DEG);
      const float relative = normalized_angle(angle - ARC_START_DEG);
      if (relative > ARC_SPAN_DEG) continue;

      uint16_t color;
      uint8_t opacity;
      arc_style_at(relative, color, opacity);

      const uint8_t alpha = mul_alpha(
        opacity,
        (uint8_t)lroundf(radial_coverage * 255.0f)
      );
      blend_pixel(target, x, y, color, alpha);
    }
  }

  // Same rounded ends as the startup sweep.
  const float middle_radius =
    (ARC_OUTER_RADIUS + ARC_INNER_RADIUS) *
    0.5f;

  const float cap_radius =
    (ARC_OUTER_RADIUS - ARC_INNER_RADIUS) *
    0.5f;

  for (int8_t endpoint = 0; endpoint < 2; ++endpoint) {
    const float relative_angle =
      endpoint == 0 ? 0.0f : ARC_SPAN_DEG;

    const float angle =
      (ARC_START_DEG + relative_angle) *
      DEG_TO_RAD;

    uint16_t color;
    uint8_t opacity;
    arc_style_at(relative_angle, color, opacity);

    fill_circle_aa(
      target,
      (int16_t)lroundf(
        LCD_CX + cosf(angle) * middle_radius
      ),
      (int16_t)lroundf(
        LCD_CY + sinf(angle) * middle_radius
      ),
      cap_radius,
      color,
      opacity
    );
  }
}

// ============================================================
// PAGE CONTENT
// ============================================================
static void draw_page_visual(
  uint16_t *target,
  int16_t x_offset,
  uint8_t opacity,
  float animation_phase
);

static void draw_page_rail_base(uint16_t *target);
static void draw_page_indicator(uint16_t *target, float center_x);

struct PageText {
  const char *title;
  char value[24];
  const char *status;
  const SDFont *value_font;
  bool degree;
};

static void get_page_text(PageText &text) {
  text.value[0] = '\0';
  text.value_font = &SD_FONT_HERO;
  text.degree = false;

  switch (g_page) {
    case PAGE_TEMP:    text.title = "TEMPERATURE"; break;
    case PAGE_FAN:     text.title = "FAN SPEED"; break;
    case PAGE_AIRFLOW: text.title = "AIRFLOW"; break;
    case PAGE_SEAT:    text.title = "SEAT HEAT"; break;
  }

  if (!climate.power_on) {
    snprintf(text.value, sizeof(text.value), "OFF");
    text.status = "HOLD TO START";
    return;
  }

  switch (g_page) {
    case PAGE_TEMP:
      snprintf(text.value, sizeof(text.value), "%.1f", climate.temperature);
      text.status = climate.ac_on ? "A/C ON" : "A/C OFF";
      text.degree = true;
      break;

    case PAGE_FAN:
      if (climate.fan == FAN_AUTO) {
        snprintf(text.value, sizeof(text.value), "AUTO");
        text.status = "AUTOMATIC";
      } else {
        snprintf(text.value, sizeof(text.value), "%d", climate.fan);
        text.status = climate.fan == 0 ? "FAN OFF" : "MANUAL";
      }
      break;

    case PAGE_AIRFLOW: {
      static const char *values[] = {
        "AUTO", "FACE", "FEET", "SCREEN"
      };
      static const char *statuses[] = {
        "AUTOMATIC", "UPPER VENTS", "FOOTWELL", "WINDSCREEN"
      };

      snprintf(
        text.value,
        sizeof(text.value),
        "%s",
        values[climate.airflow]
      );
      text.status = statuses[climate.airflow];
      if (climate.airflow == 3) text.value_font = &SD_FONT_COMPACT;
      break;
    }

    case PAGE_SEAT:
      if (climate.seat == 0) {
        snprintf(text.value, sizeof(text.value), "OFF");
        text.status = "DRIVER SEAT";
      } else {
        snprintf(text.value, sizeof(text.value), "%u", climate.seat);
        text.status = "HEATING";
      }
      break;
  }
}

static void draw_page_content(
  uint16_t *target,
  int16_t x_offset,
  uint8_t opacity
) {
  PageText text{};
  get_page_text(text);

  const uint16_t title_color = g_edit_mode ? COL_RED : COL_WHITE;
  const uint8_t title_opacity = mul_alpha(
    opacity,
    g_edit_mode ? 255 : 220
  );

  draw_text_centered(
    target,
    SD_FONT_LABEL,
    text.title,
    LCD_CX,
    127,
    title_color,
    title_opacity,
    4,
    x_offset
  );

  static constexpr int16_t VALUE_BASELINE = 276;

  if (text.degree) {
    const int16_t text_width = measure_text(*text.value_font, text.value);
    const int16_t degree_width = measure_text(SD_FONT_DEGREE, "\xB0");
    static constexpr int16_t DEGREE_GAP = 6;
    const int16_t start_x =
      LCD_CX -
      (text_width + DEGREE_GAP + degree_width) / 2 +
      x_offset;

    draw_text(
      target,
      *text.value_font,
      text.value,
      start_x,
      VALUE_BASELINE,
      COL_WHITE,
      opacity
    );

    draw_text(
      target,
      SD_FONT_DEGREE,
      "\xB0",
      start_x + text_width + DEGREE_GAP,
      VALUE_BASELINE - 62,
      COL_WHITE,
      opacity
    );
  } else {
    draw_text_centered(
      target,
      *text.value_font,
      text.value,
      LCD_CX,
      VALUE_BASELINE,
      COL_WHITE,
      opacity,
      0,
      x_offset
    );
  }

  draw_text_centered(
    target,
    SD_FONT_LABEL,
    text.status,
    LCD_CX,
    311,
    COL_WHITE,
    mul_alpha(opacity, 165),
    3,
    x_offset
  );
}

static float page_indicator_target_x(int8_t page) {
  return PAGE_RAIL_FIRST_X + page * PAGE_RAIL_SPACING;
}

static float indicator_fraction() {
  float fraction = 0.5f;

  switch (g_page) {
    case PAGE_TEMP:
      fraction = 0.02f + 0.96f * clamp_float(
        (climate.temperature - 16.0f) / 14.0f,
        0.0f,
        1.0f
      );
      break;

    case PAGE_FAN: {
      // AUTO, 0, 1, ... 8.
      const int32_t index = climate.fan + 1;
      fraction = ((float)index + 0.5f) / 10.0f;
      break;
    }

    case PAGE_AIRFLOW:
      fraction = ((float)climate.airflow + 0.5f) / 4.0f;
      break;

    case PAGE_SEAT:
      fraction = ((float)climate.seat + 0.5f) / 4.0f;
      break;
  }

  return clamp_float(fraction, 0.0f, 1.0f);
}

static void calculate_marker_position(int16_t &x, int16_t &y) {
  const float angle =
    (ARC_START_DEG + ARC_SPAN_DEG * indicator_fraction()) * DEG_TO_RAD;

  x = (int16_t)lroundf(LCD_CX + cosf(angle) * ARC_MARKER_RADIUS);
  y = (int16_t)lroundf(LCD_CY + sinf(angle) * ARC_MARKER_RADIUS);
}

static void draw_oriented_capsule_aa(
  uint16_t *target,
  int16_t center_x,
  int16_t center_y,
  float direction_x,
  float direction_y,
  float half_segment,
  float radius,
  uint16_t color,
  uint8_t opacity
) {
  const int16_t margin =
    (int16_t)ceilf(half_segment + radius + 1.0f);

  for (
    int16_t y = center_y - margin;
    y <= center_y + margin;
    ++y
  ) {
    for (
      int16_t x = center_x - margin;
      x <= center_x + margin;
      ++x
    ) {
      if ((uint16_t)x >= LCD_W || (uint16_t)y >= LCD_H) continue;

      const float dx = (x + 0.5f) - center_x;
      const float dy = (y + 0.5f) - center_y;

      const float projection = clamp_float(
        dx * direction_x + dy * direction_y,
        -half_segment,
        half_segment
      );

      const float nearest_x = projection * direction_x;
      const float nearest_y = projection * direction_y;
      const float distance_x = dx - nearest_x;
      const float distance_y = dy - nearest_y;
      const float distance = sqrtf(
        distance_x * distance_x +
        distance_y * distance_y
      );

      const float coverage = clamp_float(
        radius + 0.5f - distance,
        0.0f,
        1.0f
      );

      if (coverage <= 0.0f) continue;

      blend_pixel(
        target,
        x,
        y,
        color,
        mul_alpha(
          opacity,
          (uint8_t)lroundf(coverage * 255.0f)
        )
      );
    }
  }
}

static void draw_line_segment_aa(
  uint16_t *target,
  float x0,
  float y0,
  float x1,
  float y1,
  float radius,
  uint16_t color,
  uint8_t opacity
) {
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float length = sqrtf(dx * dx + dy * dy);

  if (length < 0.01f) {
    fill_circle_aa(target, x0, y0, radius, color, opacity);
    return;
  }

  draw_oriented_capsule_aa(
    target,
    (int16_t)lroundf((x0 + x1) * 0.5f),
    (int16_t)lroundf((y0 + y1) * 0.5f),
    dx / length,
    dy / length,
    length * 0.5f,
    radius,
    color,
    opacity
  );
}

static void draw_arrow_aa(
  uint16_t *target,
  float x0,
  float y0,
  float x1,
  float y1,
  uint16_t color,
  uint8_t opacity
) {
  draw_line_segment_aa(
    target, x0, y0, x1, y1, 1.35f, color, opacity
  );

  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float length = sqrtf(dx * dx + dy * dy);
  if (length < 0.01f) return;

  const float ux = dx / length;
  const float uy = dy / length;
  const float px = -uy;
  const float py = ux;
  static constexpr float HEAD_BACK = 6.2f;
  static constexpr float HEAD_SIDE = 3.8f;

  draw_line_segment_aa(
    target,
    x1,
    y1,
    x1 - ux * HEAD_BACK + px * HEAD_SIDE,
    y1 - uy * HEAD_BACK + py * HEAD_SIDE,
    1.35f,
    color,
    opacity
  );

  draw_line_segment_aa(
    target,
    x1,
    y1,
    x1 - ux * HEAD_BACK - px * HEAD_SIDE,
    y1 - uy * HEAD_BACK - py * HEAD_SIDE,
    1.35f,
    color,
    opacity
  );
}

static void draw_fan_visual(
  uint16_t *target,
  int16_t center_x,
  int16_t center_y,
  uint8_t opacity,
  float phase
) {
  const bool automatic = climate.fan == FAN_AUTO;
  const bool stopped = climate.fan == 0;
  const uint16_t color =
    g_edit_mode ? COL_RED : (automatic ? COL_BLUE : COL_WHITE);
  const uint8_t visual_opacity = mul_alpha(
    opacity,
    stopped ? 62 : 232
  );

  const float settled_rotation =
    (automatic ? 0.18f : climate.fan * 0.16f);
  const float rotation =
    settled_rotation - (1.0f - phase) * 0.72f;

  draw_ring_aa(
    target,
    center_x,
    center_y,
    21.0f,
    19.5f,
    color,
    mul_alpha(opacity, stopped ? 26 : 58)
  );

  for (int8_t blade = 0; blade < 3; ++blade) {
    const float angle =
      rotation + blade * (2.0f * PI / 3.0f);
    const float radial_x = cosf(angle);
    const float radial_y = sinf(angle);
    const float tangent_x = -radial_y;
    const float tangent_y = radial_x;

    draw_oriented_capsule_aa(
      target,
      (int16_t)lroundf(center_x + radial_x * 13.0f),
      (int16_t)lroundf(center_y + radial_y * 13.0f),
      tangent_x,
      tangent_y,
      6.5f,
      2.5f,
      color,
      visual_opacity
    );
  }

  fill_circle_aa(
    target,
    center_x,
    center_y,
    3.5f,
    color,
    visual_opacity
  );
}

static void draw_airflow_visual(
  uint16_t *target,
  int16_t center_x,
  int16_t center_y,
  uint8_t opacity,
  float phase
) {
  const bool automatic = climate.airflow == 0;
  const uint16_t color =
    g_edit_mode ? COL_RED : (automatic ? COL_BLUE : COL_WHITE);
  const uint8_t arrow_opacity = mul_alpha(
    opacity,
    (uint8_t)lroundf(80.0f + 165.0f * phase)
  );
  const float shift = -6.0f * (1.0f - phase);

  for (int8_t line = 0; line < 3; ++line) {
    const float y_offset = (line - 1) * 7.0f;
    float rise = 0.0f;

    if (climate.airflow == 2) {
      rise = 12.0f;
    } else if (climate.airflow == 3) {
      rise = -12.0f;
    } else if (automatic) {
      rise = (line - 1) * 8.0f;
    }

    draw_arrow_aa(
      target,
      center_x - 25.0f + shift,
      center_y + y_offset,
      center_x + 23.0f + shift,
      center_y + y_offset + rise,
      color,
      arrow_opacity
    );
  }
}

static void draw_heat_wave(
  uint16_t *target,
  float center_x,
  float top_y,
  float bottom_y,
  uint16_t color,
  uint8_t opacity
) {
  float previous_x = center_x;
  float previous_y = bottom_y;

  for (int8_t point = 1; point <= 7; ++point) {
    const float t = point / 7.0f;
    const float x = center_x + sinf(t * 2.0f * PI) * 2.4f;
    const float y = bottom_y + (top_y - bottom_y) * t;

    draw_line_segment_aa(
      target,
      previous_x,
      previous_y,
      x,
      y,
      1.15f,
      color,
      opacity
    );

    previous_x = x;
    previous_y = y;
  }
}

static void draw_seat_visual(
  uint16_t *target,
  int16_t center_x,
  int16_t center_y,
  uint8_t opacity,
  float phase
) {
  const uint8_t outline_opacity = mul_alpha(opacity, 185);

  // Simple side profile with the heater waves above the cushion.
  draw_line_segment_aa(
    target,
    center_x - 28.0f,
    center_y - 16.0f,
    center_x - 21.0f,
    center_y + 8.0f,
    1.65f,
    COL_WHITE,
    outline_opacity
  );

  draw_line_segment_aa(
    target,
    center_x - 20.0f,
    center_y + 9.0f,
    center_x + 27.0f,
    center_y + 9.0f,
    1.65f,
    COL_WHITE,
    outline_opacity
  );

  draw_line_segment_aa(
    target,
    center_x - 14.0f,
    center_y + 11.0f,
    center_x - 17.0f,
    center_y + 20.0f,
    1.45f,
    COL_WHITE,
    outline_opacity
  );

  draw_line_segment_aa(
    target,
    center_x + 22.0f,
    center_y + 11.0f,
    center_x + 26.0f,
    center_y + 20.0f,
    1.45f,
    COL_WHITE,
    outline_opacity
  );

  for (int8_t wave = 0; wave < 3; ++wave) {
    const bool active = wave < climate.seat;
    const float local_phase = clamp_float(
      phase * 1.35f - wave * 0.18f,
      0.0f,
      1.0f
    );
    const uint8_t wave_opacity = mul_alpha(
      opacity,
      active
        ? (uint8_t)lroundf(72.0f + 183.0f * local_phase)
        : 28
    );

    draw_heat_wave(
      target,
      center_x - 8.0f + wave * 12.0f,
      center_y - 13.0f,
      center_y + 4.0f,
      active ? COL_RED : COL_WHITE,
      wave_opacity
    );
  }
}

static void draw_page_visual(
  uint16_t *target,
  int16_t x_offset,
  uint8_t opacity,
  float animation_phase
) {
  const float phase = clamp_float(animation_phase, 0.0f, 1.0f);
  const int16_t center_x = LCD_CX + x_offset;
  static constexpr int16_t center_y = 352;

  // Only the temperature page uses the extra focus blade.
  if (g_edit_mode && g_page == PAGE_TEMP) {
    draw_oriented_capsule_aa(
      target,
      center_x,
      326,
      1.0f,
      0.0f,
      24.0f,
      1.25f,
      COL_RED,
      mul_alpha(opacity, 235)
    );
  }

  switch (g_page) {
    case PAGE_TEMP:
      break;

    case PAGE_FAN:
      draw_fan_visual(target, center_x, center_y, opacity, phase);
      break;

    case PAGE_AIRFLOW:
      draw_airflow_visual(target, center_x, center_y, opacity, phase);
      break;

    case PAGE_SEAT:
      draw_seat_visual(target, center_x, center_y, opacity, phase);
      break;
  }
}

static void draw_page_rail_base(uint16_t *target) {
  for (int8_t index = 0; index < PAGE_COUNT; ++index) {
    draw_oriented_capsule_aa(
      target,
      PAGE_RAIL_FIRST_X + index * PAGE_RAIL_SPACING,
      399,
      1.0f,
      0.0f,
      5.5f,
      1.15f,
      COL_WHITE,
      46
    );
  }
}

static void draw_page_indicator(uint16_t *target, float center_x) {
  draw_oriented_capsule_aa(
    target,
    (int16_t)lroundf(center_x),
    399,
    1.0f,
    0.0f,
    9.5f,
    1.8f,
    g_edit_mode ? COL_RED : COL_WHITE,
    255
  );
}

static void draw_marker(uint16_t *target, int16_t x, int16_t y) {
  const uint16_t color = g_edit_mode ? COL_RED : COL_WHITE;
  const float radial_x = (float)(x - LCD_CX);
  const float radial_y = (float)(y - LCD_CY);
  const float length = sqrtf(
    radial_x * radial_x + radial_y * radial_y
  );

  if (length < 0.001f) return;

  const float direction_x = radial_x / length;
  const float direction_y = radial_y / length;

  // The black slot keeps the index readable over the arc.
  draw_oriented_capsule_aa(
    target,
    x,
    y,
    direction_x,
    direction_y,
    11.0f,
    4.8f,
    COL_BLACK,
    255
  );

  draw_oriented_capsule_aa(
    target,
    x,
    y,
    direction_x,
    direction_y,
    7.0f,
    1.8f,
    color,
    255
  );
}

static void render_base_frame() {
  fill_rect(g_base_frame, 0, 0, LCD_W, LCD_H, COL_BLACK);
  render_arc_base(g_base_frame);
  draw_page_rail_base(g_base_frame);
}

static void compose_full_frame(
  int16_t content_offset,
  uint8_t content_opacity
) {
  memcpy(g_frame, g_base_frame, FRAME_BYTES);
  draw_page_content(g_frame, content_offset, content_opacity);
  draw_page_visual(
    g_frame,
    content_offset,
    content_opacity,
    g_visual_animation_phase
  );

  copy_frame_rect(
    g_frame,
    g_base_frame,
    PAGE_RAIL_X,
    PAGE_RAIL_Y,
    PAGE_RAIL_W,
    PAGE_RAIL_H
  );
  draw_page_indicator(g_frame, g_page_indicator_x);

  if (climate.power_on && g_page == PAGE_TEMP) {
    calculate_marker_position(g_marker_x, g_marker_y);
    draw_marker(g_frame, g_marker_x, g_marker_y);
    g_marker_visible = true;
  } else {
    g_marker_visible = false;
  }
}

// ============================================================
// PARTIAL, TEAR-FREE UPDATES
// ============================================================
static void restore_marker_patch(int16_t x, int16_t y) {
  copy_frame_rect(
    g_frame,
    g_base_frame,
    x - MARKER_PATCH / 2,
    y - MARKER_PATCH / 2,
    MARKER_PATCH,
    MARKER_PATCH
  );
}

static void refresh_dynamic_ui(
  bool marker_may_move,
  bool rail_may_change
) {
  const bool old_marker_visible = g_marker_visible;
  const int16_t old_marker_x = g_marker_x;
  const int16_t old_marker_y = g_marker_y;

  copy_frame_rect(
    g_frame,
    g_base_frame,
    CONTENT_X,
    CONTENT_Y,
    CONTENT_W,
    CONTENT_H
  );
  draw_page_content(g_frame, 0, 255);

  copy_frame_rect(
    g_frame,
    g_base_frame,
    VISUAL_X,
    VISUAL_Y,
    VISUAL_W,
    VISUAL_H
  );
  draw_page_visual(
    g_frame,
    0,
    255,
    g_visual_animation_phase
  );

  if (old_marker_visible) {
    restore_marker_patch(old_marker_x, old_marker_y);
  }

  if (climate.power_on && g_page == PAGE_TEMP) {
    calculate_marker_position(g_marker_x, g_marker_y);
    draw_marker(g_frame, g_marker_x, g_marker_y);
    g_marker_visible = true;
  } else {
    g_marker_visible = false;
  }

  if (rail_may_change) {
    copy_frame_rect(
      g_frame,
      g_base_frame,
      PAGE_RAIL_X,
      PAGE_RAIL_Y,
      PAGE_RAIL_W,
      PAGE_RAIL_H
    );
    draw_page_indicator(g_frame, g_page_indicator_x);
  }

  // One TE wait for all related patches.
  wait_for_te_edge();
  push_frame_rect(CONTENT_X, CONTENT_Y, CONTENT_W, CONTENT_H, false);
  push_frame_rect(VISUAL_X, VISUAL_Y, VISUAL_W, VISUAL_H, false);

  if (marker_may_move || old_marker_visible != g_marker_visible) {
    if (old_marker_visible) {
      push_frame_rect(
        old_marker_x - MARKER_PATCH / 2,
        old_marker_y - MARKER_PATCH / 2,
        MARKER_PATCH,
        MARKER_PATCH,
        false
      );
    }

    if (g_marker_visible) {
      push_frame_rect(
        g_marker_x - MARKER_PATCH / 2,
        g_marker_y - MARKER_PATCH / 2,
        MARKER_PATCH,
        MARKER_PATCH,
        false
      );
    }
  }

  if (rail_may_change) {
    push_frame_rect(
      PAGE_RAIL_X,
      PAGE_RAIL_Y,
      PAGE_RAIL_W,
      PAGE_RAIL_H,
      false
    );
  }
}

// ============================================================
// NON-BLOCKING PAGE TRANSITION
// ============================================================
static float ease_out_cubic(float value) {
  value = clamp_float(value, 0.0f, 1.0f);
  const float inverse = 1.0f - value;
  return 1.0f - inverse * inverse * inverse;
}

static void begin_animation_transfer_group() {
  if (g_animation_te_used) return;
  wait_for_te_edge();
  g_animation_te_used = true;
}

static void start_page_transition_if_needed() {
  if (g_requested_page == g_page) return;

  g_page = g_requested_page;
  g_edit_mode = false;

  // Content changes first; only the small page rail keeps moving.
  g_visual_animation_phase = g_page == PAGE_TEMP ? 1.0f : 0.0f;
  g_visual_animation_active = g_page != PAGE_TEMP;
  g_visual_animation_last_frame = 0;
  refresh_dynamic_ui(true, false);
  g_visual_animation_started = millis();

  g_transition_from_x = g_page_indicator_x;
  g_transition_to_x = page_indicator_target_x(g_page);
  g_transition_started = millis();
  g_transition_active =
    fabsf(g_transition_to_x - g_transition_from_x) > 0.01f;
  g_transition_last_frame = 0;
}

static void update_page_transition() {
  if (!g_transition_active) return;

  const uint32_t elapsed = millis() - g_transition_started;
  const float linear = clamp_float(
    (float)elapsed / (float)PAGE_TRANSITION_MS,
    0.0f,
    1.0f
  );
  const uint8_t frame_index = (uint8_t)clamp_i32(
    (int32_t)floorf(linear * PAGE_TRANSITION_FRAMES),
    0,
    PAGE_TRANSITION_FRAMES
  );

  if (frame_index != g_transition_last_frame || linear >= 1.0f) {
    g_transition_last_frame = frame_index;

    const float eased = ease_out_cubic(linear);
    g_page_indicator_x =
      g_transition_from_x +
      (g_transition_to_x - g_transition_from_x) * eased;

    copy_frame_rect(
      g_frame,
      g_base_frame,
      PAGE_RAIL_X,
      PAGE_RAIL_Y,
      PAGE_RAIL_W,
      PAGE_RAIL_H
    );
    draw_page_indicator(g_frame, g_page_indicator_x);
    begin_animation_transfer_group();
    push_frame_rect(
      PAGE_RAIL_X,
      PAGE_RAIL_Y,
      PAGE_RAIL_W,
      PAGE_RAIL_H,
      false
    );
  }

  if (linear >= 1.0f) {
    g_page_indicator_x = g_transition_to_x;
    g_transition_active = false;
  }
}

static void update_visual_animation() {
  if (!g_visual_animation_active) return;

  const uint32_t elapsed = millis() - g_visual_animation_started;
  const float linear = clamp_float(
    (float)elapsed / (float)VISUAL_ANIMATION_MS,
    0.0f,
    1.0f
  );
  const uint8_t frame_index = (uint8_t)clamp_i32(
    (int32_t)floorf(linear * VISUAL_ANIMATION_FRAMES),
    0,
    VISUAL_ANIMATION_FRAMES
  );

  if (
    frame_index != g_visual_animation_last_frame ||
    linear >= 1.0f
  ) {
    g_visual_animation_last_frame = frame_index;
    g_visual_animation_phase = ease_out_cubic(linear);

    copy_frame_rect(
      g_frame,
      g_base_frame,
      VISUAL_X,
      VISUAL_Y,
      VISUAL_W,
      VISUAL_H
    );
    draw_page_visual(
      g_frame,
      0,
      255,
      g_visual_animation_phase
    );
    begin_animation_transfer_group();
    push_frame_rect(
      VISUAL_X,
      VISUAL_Y,
      VISUAL_W,
      VISUAL_H,
      false
    );
  }

  if (linear >= 1.0f) {
    g_visual_animation_phase = 1.0f;
    g_visual_animation_active = false;
  }
}

static ESP32Encoder encoder;

static int64_t g_encoder_last_raw = 0;
static int32_t g_encoder_substeps = 0;

static int16_t consume_encoder_delta() {
  const int64_t raw_now = encoder.getCount();
  const int64_t raw_delta = raw_now - g_encoder_last_raw;

  g_encoder_last_raw = raw_now;
  g_encoder_substeps += (int32_t)raw_delta;

  int16_t steps =
    (int16_t)(g_encoder_substeps / ENCODER_TRANSITIONS_PER_STEP);

  g_encoder_substeps -=
    (int32_t)steps * ENCODER_TRANSITIONS_PER_STEP;

#if ENCODER_REVERSED
  steps = -steps;
#endif

  return steps;
}

static ButtonEvent read_encoder_button() {
  static bool previous_raw = HIGH;
  static bool stable = HIGH;
  static bool long_sent = false;
  static uint32_t raw_changed_at = 0;
  static uint32_t pressed_at = 0;

  const bool raw = digitalRead(ENCODER_BUTTON);
  const uint32_t now = millis();

  if (raw != previous_raw) {
    previous_raw = raw;
    raw_changed_at = now;
  }

  if ((now - raw_changed_at) >= BUTTON_DEBOUNCE_MS && raw != stable) {
    stable = raw;

    if (stable == LOW) {
      pressed_at = now;
      long_sent = false;
    } else if (!long_sent) {
      return BUTTON_SHORT;
    }
  }

  if (
    stable == LOW &&
    !long_sent &&
    (now - pressed_at) >= BUTTON_LONG_MS
  ) {
    long_sent = true;
    return BUTTON_LONG;
  }

  return BUTTON_NONE;
}

// ============================================================
// VALUE EDITING + INPUT
// ============================================================
static void rebuild_and_push_current_page() {
  g_visual_animation_phase = 1.0f;
  g_visual_animation_active = false;
  g_page_indicator_x = page_indicator_target_x(g_page);
  render_base_frame();
  compose_full_frame(0, 255);
  push_full_frame(true);
}

static void adjust_current_value(int16_t delta) {
  if (delta == 0) return;

  const bool was_powered = climate.power_on;
  climate.power_on = true;

  switch (g_page) {
    case PAGE_TEMP:
      climate.temperature = clamp_float(
        climate.temperature + 0.5f * delta,
        16.0f,
        30.0f
      );
      break;

    case PAGE_FAN:
      climate.fan = (int8_t)clamp_i32(
        (int32_t)climate.fan + delta,
        FAN_AUTO,
        8
      );
      break;

    case PAGE_AIRFLOW: {
      int32_t next = climate.airflow + delta;
      while (next < 0) next += 4;
      while (next > 3) next -= 4;
      climate.airflow = (uint8_t)next;
      break;
    }

    case PAGE_SEAT:
      climate.seat = (uint8_t)clamp_i32(
        (int32_t)climate.seat + delta,
        0,
        3
      );
      break;
  }

  if (!was_powered) {
    rebuild_and_push_current_page();
  } else {
    if (g_page != PAGE_TEMP) {
      g_visual_animation_phase = 0.0f;
      g_visual_animation_active = true;
      g_visual_animation_last_frame = 0;
    }
    refresh_dynamic_ui(true, false);
    if (g_visual_animation_active) {
      g_visual_animation_started = millis();
    }
  }
}

static void handle_encoder_rotation(int16_t delta) {
  if (delta == 0) return;

  if (g_edit_mode) {
    adjust_current_value(delta);
    return;
  }

  g_requested_page = (int8_t)clamp_i32(
    (int32_t)g_requested_page + delta,
    0,
    PAGE_COUNT - 1
  );

  start_page_transition_if_needed();
}

static void handle_button(ButtonEvent event) {
  if (event == BUTTON_SHORT) {
    g_edit_mode = !g_edit_mode;
    refresh_dynamic_ui(true, true);
    return;
  }

  if (event == BUTTON_LONG) {
    climate.power_on = !climate.power_on;
    g_edit_mode = false;
    g_transition_active = false;
    g_visual_animation_active = false;
    g_visual_animation_phase = 1.0f;
    g_requested_page = g_page;
    rebuild_and_push_current_page();
  }
}

// ============================================================
// CAN BUS
// ============================================================
static constexpr uint8_t CAN_CMD_SET_TEMPERATURE = 0x01;
static constexpr uint8_t CAN_CMD_SET_FAN         = 0x02;
static constexpr uint8_t CAN_CMD_SET_AIRFLOW     = 0x03;
static constexpr uint8_t CAN_CMD_SET_SEAT        = 0x04;
static constexpr uint8_t CAN_CMD_SET_AC          = 0x05;
static constexpr uint8_t CAN_CMD_SET_POWER       = 0x06;

static uint8_t can_checksum(const uint8_t *data, uint8_t length) {
  uint8_t checksum = 0;
  for (uint8_t index = 0; index < length; ++index) {
    checksum ^= data[index];
  }
  return checksum;
}

static void can_send_state();
static void can_tx_task(void *parameter);

static void can_publish_state_snapshot() {
  uint8_t snapshot[6];
  snapshot[0] =
    (climate.power_on ? 0x01 : 0x00) |
    (climate.ac_on ? 0x02 : 0x00);
  snapshot[1] = (uint8_t)lroundf(climate.temperature * 2.0f);
  snapshot[2] = climate.fan == FAN_AUTO
    ? 0xFF
    : (uint8_t)climate.fan;
  snapshot[3] = climate.airflow;
  snapshot[4] = climate.seat;
  snapshot[5] = (uint8_t)g_page;

  portENTER_CRITICAL(&g_can_state_mux);
  memcpy(g_can_state_snapshot, snapshot, sizeof(snapshot));
  portEXIT_CRITICAL(&g_can_state_mux);
}

static bool can_begin() {
  twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
    (gpio_num_t)CAN_TX_GPIO,
    (gpio_num_t)CAN_RX_GPIO,
    TWAI_MODE_NORMAL
  );
  twai_timing_config_t timing = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  general.tx_queue_len = 8;
  general.rx_queue_len = 16;
  general.alerts_enabled =
    TWAI_ALERT_TX_FAILED |
    TWAI_ALERT_BUS_OFF |
    TWAI_ALERT_BUS_RECOVERED;

  if (twai_driver_install(&general, &timing, &filter) != ESP_OK) {
    return false;
  }

  if (twai_start() != ESP_OK) {
    twai_driver_uninstall();
    return false;
  }

  g_can_ready = true;
  g_can_recovering = false;
  g_can_last_tx = millis();

  can_publish_state_snapshot();
  const BaseType_t task_result = xTaskCreatePinnedToCore(
    can_tx_task,
    "can-state-tx",
    CAN_TX_TASK_STACK,
    nullptr,
    CAN_TX_TASK_PRIORITY,
    &g_can_tx_task_handle,
    0
  );

  if (task_result != pdPASS) {
    g_can_tx_task_handle = nullptr;
    Serial.println("CAN: state task unavailable, using loop fallback");
  }

  return true;
}

static void can_send_state() {
  if (!g_can_ready || g_can_recovering) return;

  twai_message_t message = {};
  message.identifier = CAN_STATE_ID;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 8;

  portENTER_CRITICAL(&g_can_state_mux);
  memcpy(message.data, g_can_state_snapshot, sizeof(g_can_state_snapshot));
  portEXIT_CRITICAL(&g_can_state_mux);

  message.data[6] = g_can_alive_counter & 0x0F;
  message.data[7] = can_checksum(message.data, 7);

  if (twai_transmit(&message, 0) == ESP_OK) {
    g_can_alive_counter = (g_can_alive_counter + 1) & 0x0F;
  } else {
    ++g_can_tx_failures;
  }
}

static void can_tx_task(void *parameter) {
  (void)parameter;

  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(CAN_TX_PERIOD_MS));
    g_can_last_tx = millis();
    can_send_state();
  }
}

static void can_request_state_tx() {
  can_publish_state_snapshot();

  if (g_can_tx_task_handle != nullptr) {
    xTaskNotifyGive(g_can_tx_task_handle);
    return;
  }

  g_can_last_tx = millis();
  can_send_state();
}

static void can_refresh_value_page(PageId page, bool marker_may_move) {
  if (!climate.power_on || g_page != page) return;

  if (page != PAGE_TEMP) {
    g_visual_animation_phase = 0.0f;
    g_visual_animation_active = true;
    g_visual_animation_last_frame = 0;
  }

  refresh_dynamic_ui(marker_may_move, false);

  if (g_visual_animation_active) {
    g_visual_animation_started = millis();
  }
}

static void can_mark_page_dirty(CanCommandBatch &batch, PageId page) {
  batch.changed = true;
  batch.dirty_pages |= (uint8_t)(1U << (uint8_t)page);
}

static bool can_apply_command(
  const twai_message_t &message,
  CanCommandBatch &batch
) {
  if (message.data_length_code < 2) return false;

  const uint8_t command = message.data[0];
  const uint8_t value = message.data[1];

  switch (command) {
    case CAN_CMD_SET_TEMPERATURE: {
      if (value < 32 || value > 60) return false;
      const float next = value * 0.5f;
      if (fabsf(next - climate.temperature) >= 0.01f) {
        climate.temperature = next;
        batch.temperature_marker_may_move = true;
        can_mark_page_dirty(batch, PAGE_TEMP);
      }
      break;
    }

    case CAN_CMD_SET_FAN: {
      if (value != 0xFF && value > 8) return false;
      const int8_t next = value == 0xFF ? FAN_AUTO : (int8_t)value;
      if (next != climate.fan) {
        climate.fan = next;
        can_mark_page_dirty(batch, PAGE_FAN);
      }
      break;
    }

    case CAN_CMD_SET_AIRFLOW: {
      if (value > 3) return false;
      if (value != climate.airflow) {
        climate.airflow = value;
        can_mark_page_dirty(batch, PAGE_AIRFLOW);
      }
      break;
    }

    case CAN_CMD_SET_SEAT: {
      if (value > 3) return false;
      if (value != climate.seat) {
        climate.seat = value;
        can_mark_page_dirty(batch, PAGE_SEAT);
      }
      break;
    }

    case CAN_CMD_SET_AC: {
      const bool next = value != 0;
      if (next != climate.ac_on) {
        climate.ac_on = next;
        can_mark_page_dirty(batch, PAGE_TEMP);
      }
      break;
    }

    case CAN_CMD_SET_POWER: {
      const bool next = value != 0;
      if (next != climate.power_on) {
        climate.power_on = next;
        g_edit_mode = false;
        g_transition_active = false;
        g_visual_animation_active = false;
        g_visual_animation_phase = 1.0f;
        g_requested_page = g_page;
        batch.changed = true;
        batch.full_rebuild = true;
      }
      break;
    }

    default:
      return false;
  }

  // Valid duplicate commands are accepted too. Sending a fresh state frame
  // lets the ECU confirm a retry without forcing another display redraw.
  batch.accepted = true;
  ++batch.accepted_count;
  batch.last_command = command;
  batch.last_value = value;
  return true;
}

static void can_receive_commands() {
  if (!g_can_ready || g_can_recovering) return;

  CanCommandBatch batch = {};

  for (uint8_t count = 0; count < CAN_RX_BUDGET; ++count) {
    twai_message_t message;
    if (twai_receive(&message, 0) != ESP_OK) break;

    if (
      !message.extd &&
      !message.rtr &&
      message.identifier == CAN_COMMAND_ID
    ) {
      can_apply_command(message, batch);
    }
  }

  if (!batch.accepted) return;

  // Publish only the final state from this batch. Drawing is intentionally
  // done once, after the RX queue has been drained.
  can_request_state_tx();

  if (batch.full_rebuild) {
    rebuild_and_push_current_page();
  } else if (
    batch.changed &&
    g_page >= PAGE_TEMP &&
    g_page < PAGE_COUNT &&
    (batch.dirty_pages & (uint8_t)(1U << (uint8_t)g_page)) != 0
  ) {
    can_refresh_value_page(
      (PageId)g_page,
      g_page == PAGE_TEMP && batch.temperature_marker_may_move
    );
  }

  Serial.printf(
    "CAN RX: %u accepted, latest %02X %02X\n",
    batch.accepted_count,
    batch.last_command,
    batch.last_value
  );
}

static void can_handle_alerts() {
  if (!g_can_ready) return;

  uint32_t alerts = 0;
  if (twai_read_alerts(&alerts, 0) != ESP_OK) return;

  if (alerts & TWAI_ALERT_TX_FAILED) {
    ++g_can_tx_failures;
  }

  if (alerts & TWAI_ALERT_BUS_OFF) {
    Serial.println("CAN: bus off, starting recovery");
    g_can_recovering = twai_initiate_recovery() == ESP_OK;
  }

  if (alerts & TWAI_ALERT_BUS_RECOVERED) {
    if (twai_start() == ESP_OK) {
      g_can_recovering = false;
      g_can_last_tx = millis();
      Serial.println("CAN: recovered");
    }
  }
}

static void service_can() {
  if (!g_can_ready) return;

  can_handle_alerts();
  can_receive_commands();

  if (g_can_tx_task_handle == nullptr) {
    const uint32_t now = millis();
    if ((uint32_t)(now - g_can_last_tx) >= CAN_TX_PERIOD_MS) {
      g_can_last_tx = now;
      can_send_state();
    }
  }
}

// ============================================================
// STARTUP ARC LOADER
// ============================================================
#define STARTUP_SETTLE_MS       220
#define STARTUP_SWEEP_MS        620
#define STARTUP_RING_OUTER      ARC_OUTER_RADIUS
#define STARTUP_RING_INNER      ARC_INNER_RADIUS
#define STARTUP_LOGO_HOLD_MS    2000

// Target 60 FPS. The dirty renderer sends only two small areas.
#define STARTUP_FRAME_US        16667UL

static float g_startup_previous_progress = 0.0f;

static float startup_ease(float value) {
  value = clamp_float(value, 0.0f, 1.0f);

  return value * value * value *
    (value * (value * 6.0f - 15.0f) + 10.0f);
}

static void prepare_startup_sweep_lut() {
  memset(g_base_frame, 0, FRAME_BYTES);

  const float maximum_half_span =
    ARC_SPAN_DEG * 0.5f; // 135 deg per side

  const float inner_limit =
    STARTUP_RING_INNER - 0.5f;

  const float outer_limit =
    STARTUP_RING_OUTER + 0.5f;

  const float inner_limit_sq =
    inner_limit * inner_limit;

  const float outer_limit_sq =
    outer_limit * outer_limit;

  const int16_t margin =
    STARTUP_RING_OUTER + 1;

  for (
    int16_t y = LCD_CY - margin;
    y <= LCD_CY + margin;
    ++y
  ) {
    for (
      int16_t x = LCD_CX - margin;
      x <= LCD_CX + margin;
      ++x
    ) {
      if ((uint16_t)x >= LCD_W || (uint16_t)y >= LCD_H) {
        continue;
      }

      const float dx = (x + 0.5f) - LCD_CX;
      const float dy = (y + 0.5f) - LCD_CY;

      const float radius_sq =
        dx * dx + dy * dy;

      if (
        radius_sq < inner_limit_sq ||
        radius_sq > outer_limit_sq
      ) {
        continue;
      }

      const float angle =
        normalized_angle(atan2f(dy, dx) * RAD_TO_DEG);

      float distance_from_top =
        fabsf(angle - 270.0f);

      if (distance_from_top > 180.0f) {
        distance_from_top =
          360.0f - distance_from_top;
      }

      if (distance_from_top > maximum_half_span) {
        continue;
      }

      const float radius = sqrtf(radius_sq);

      const float outer_coverage = clamp_float(
        STARTUP_RING_OUTER + 0.5f - radius,
        0.0f,
        1.0f
      );

      const float inner_coverage = clamp_float(
        radius - STARTUP_RING_INNER + 0.5f,
        0.0f,
        1.0f
      );

      const float coverage = fminf(
        outer_coverage,
        inner_coverage
      );

      const uint8_t threshold = (uint8_t)clamp_i32(
        1 + (int32_t)lroundf(
          distance_from_top /
          maximum_half_span *
          254.0f
        ),
        1,
        255
      );

      const uint8_t alpha = (uint8_t)clamp_i32(
        (int32_t)lroundf(coverage * 255.0f),
        0,
        255
      );

      g_base_frame[(size_t)y * LCD_W + x] =
        ((uint16_t)threshold << 8) | alpha;
    }
  }
}

static void get_startup_dirty_rect(
  float previous_progress,
  float current_progress,
  int8_t side,
  int16_t *out_x,
  int16_t *out_y,
  int16_t *out_w,
  int16_t *out_h
) {
  const float middle_radius =
    (STARTUP_RING_OUTER + STARTUP_RING_INNER) *
    0.5f;

  const float middle_progress =
    (previous_progress + current_progress) *
    0.5f;

  const float middle_angle =
    (
      270.0f +
      side *
      middle_progress *
      ARC_SPAN_DEG *
      0.5f
    ) * DEG_TO_RAD;

  const int16_t center_x =
    (int16_t)lroundf(
      LCD_CX +
      cosf(middle_angle) * middle_radius
    );

  const int16_t center_y =
    (int16_t)lroundf(
      LCD_CY +
      sinf(middle_angle) * middle_radius
    );

  // Covers both endpoint positions and their AA edge.
  static constexpr int16_t DIRTY_SIZE = 72;

  int32_t x0 =
    center_x - DIRTY_SIZE / 2;

  int32_t y0 =
    center_y - DIRTY_SIZE / 2;

  x0 = clamp_i32(
    x0,
    0,
    LCD_W - DIRTY_SIZE
  );

  y0 = clamp_i32(
    y0,
    0,
    LCD_H - DIRTY_SIZE
  );

  // Keep X and width even for RGB565 transfers.
  x0 &= ~1L;

  *out_x = (int16_t)x0;
  *out_y = (int16_t)y0;
  *out_w = DIRTY_SIZE;
  *out_h = DIRTY_SIZE;
}

static void render_startup_dirty_rect(
  float progress,
  int16_t x,
  int16_t y,
  int16_t width,
  int16_t height
) {
  const bool arc_visible =
    progress > 0.001f;

  float endpoint_x[2] = {0.0f, 0.0f};
  float endpoint_y[2] = {0.0f, 0.0f};
  uint16_t endpoint_color[2] = {COL_WHITE, COL_WHITE};
  uint8_t endpoint_opacity[2] = {255, 255};

  const float cap_radius =
    (STARTUP_RING_OUTER - STARTUP_RING_INNER) *
    0.5f;

  if (arc_visible) {
    const float current_half_span =
      progress * ARC_SPAN_DEG * 0.5f;

    const float middle_radius =
      (STARTUP_RING_OUTER + STARTUP_RING_INNER) *
      0.5f;

    for (int8_t endpoint = 0; endpoint < 2; ++endpoint) {
      const int8_t side =
        endpoint == 0 ? -1 : 1;

      const float endpoint_angle_degrees =
        270.0f +
        side * current_half_span;

      const float endpoint_angle =
        endpoint_angle_degrees * DEG_TO_RAD;

      endpoint_x[endpoint] =
        lroundf(
          LCD_CX +
          cosf(endpoint_angle) * middle_radius
        );

      endpoint_y[endpoint] =
        lroundf(
          LCD_CY +
          sinf(endpoint_angle) * middle_radius
        );

      const float relative_angle =
        normalized_angle(
          endpoint_angle_degrees -
          ARC_START_DEG
        );

      arc_style_at(
        relative_angle,
        endpoint_color[endpoint],
        endpoint_opacity[endpoint]
      );
    }
  }

  const uint8_t visible_threshold =
    arc_visible
      ? (uint8_t)clamp_i32(
          1 + (int32_t)lroundf(progress * 254.0f),
          1,
          255
        )
      : 0;

  for (
    int16_t pixel_y = y;
    pixel_y < y + height;
    ++pixel_y
  ) {
    for (
      int16_t pixel_x = x;
      pixel_x < x + width;
      ++pixel_x
    ) {
      const size_t index =
        (size_t)pixel_y * LCD_W + pixel_x;

      uint16_t color = COL_BLACK;

      if (arc_visible) {
        const uint16_t packed =
          g_base_frame[index];

        if (
          packed != 0 &&
          (uint8_t)(packed >> 8) <= visible_threshold
        ) {
          const float dx =
            (pixel_x + 0.5f) - LCD_CX;

          const float dy =
            (pixel_y + 0.5f) - LCD_CY;

          const float angle =
            normalized_angle(
              atan2f(dy, dx) * RAD_TO_DEG
            );

          const float relative_angle =
            normalized_angle(
              angle - ARC_START_DEG
            );

          uint16_t arc_color;
          uint8_t arc_opacity;

          arc_style_at(
            relative_angle,
            arc_color,
            arc_opacity
          );

          color = blend_rgb565(
            COL_BLACK,
            arc_color,
            mul_alpha(
              arc_opacity,
              (uint8_t)(packed & 0xFF)
            )
          );
        }
      }

      if (arc_visible) {
        for (
          int8_t endpoint = 0;
          endpoint < 2;
          ++endpoint
        ) {
          const float dx =
            (pixel_x + 0.5f) -
            endpoint_x[endpoint];

          const float dy =
            (pixel_y + 0.5f) -
            endpoint_y[endpoint];

          const float distance_sq =
            dx * dx + dy * dy;

          const float cap_limit =
            cap_radius + 0.5f;

          if (
            distance_sq <
            cap_limit * cap_limit
          ) {
            const float coverage =
              clamp_float(
                cap_limit -
                sqrtf(distance_sq),
                0.0f,
                1.0f
              );

            color = blend_rgb565(
              color,
              endpoint_color[endpoint],
              mul_alpha(
                endpoint_opacity[endpoint],
                (uint8_t)lroundf(
                  coverage * 255.0f
                )
              )
            );
          }
        }
      }

      g_frame[index] = color;
    }
  }

  // Small enough to share the current TE window.
  push_frame_rect(
    x,
    y,
    width,
    height,
    false
  );
}

static void draw_startup_sweep_frame(float progress) {
  progress = clamp_float(progress, 0.0f, 1.0f);

  if (
    fabsf(progress - g_startup_previous_progress) <
    0.00005f
  ) {
    return;
  }

  for (int8_t side = -1; side <= 1; side += 2) {
    int16_t dirty_x;
    int16_t dirty_y;
    int16_t dirty_width;
    int16_t dirty_height;

    get_startup_dirty_rect(
      g_startup_previous_progress,
      progress,
      side,
      &dirty_x,
      &dirty_y,
      &dirty_width,
      &dirty_height
    );

    render_startup_dirty_rect(
      progress,
      dirty_x,
      dirty_y,
      dirty_width,
      dirty_height
    );
  }

  g_startup_previous_progress = progress;
}

static void run_startup_sweep_phase(
  float from,
  float to,
  uint16_t duration_ms
) {
  const uint32_t started_ms = millis();
  uint32_t next_frame_us = micros();

  while (true) {
    const uint32_t elapsed_ms =
      millis() - started_ms;

    const float linear = clamp_float(
      (float)elapsed_ms / duration_ms,
      0.0f,
      1.0f
    );

    const float progress =
      from +
      (to - from) * startup_ease(linear);

    draw_startup_sweep_frame(progress);

    if (elapsed_ms >= duration_ms) {
      break;
    }

    next_frame_us += STARTUP_FRAME_US;

    const int32_t wait_us =
      (int32_t)(next_frame_us - micros());

    if (wait_us > 0) {
      delayMicroseconds((uint32_t)wait_us);
    } else {
      next_frame_us = micros();
    }
  }
}

static void run_runtime_arc_load() {
  fill_rect(
    g_frame,
    0,
    0,
    LCD_W,
    LCD_H,
    COL_BLACK
  );

  g_startup_previous_progress = 0.0f;

  prepare_startup_sweep_lut();

  run_startup_sweep_phase(
    0.0f,
    1.0f,
    STARTUP_SWEEP_MS
  );

  // The sweep LUT borrows g_base_frame, so rebuild it before marker updates.
  render_base_frame();
}

static void reveal_startup_content() {
  // Restore the static frame; the finished arc is already on the panel.
  memcpy(g_frame, g_base_frame, FRAME_BYTES);
  g_marker_visible = false;

  static constexpr uint8_t REVEAL_FRAMES = 6;

  for (
    uint8_t frame = 1;
    frame <= REVEAL_FRAMES;
    ++frame
  ) {
    const float linear =
      (float)frame / REVEAL_FRAMES;

    const uint8_t opacity =
      (uint8_t)lroundf(
        startup_ease(linear) * 255.0f
      );

    copy_frame_rect(
      g_frame,
      g_base_frame,
      CONTENT_X,
      CONTENT_Y,
      CONTENT_W,
      CONTENT_H
    );

    draw_page_content(g_frame, 0, opacity);
    copy_frame_rect(
      g_frame,
      g_base_frame,
      VISUAL_X,
      VISUAL_Y,
      VISUAL_W,
      VISUAL_H
    );
    draw_page_visual(g_frame, 0, opacity, 1.0f);

    // Keep the transfer seperate from the animation timing.
    wait_for_te_edge();
    push_frame_rect(
      CONTENT_X,
      CONTENT_Y,
      CONTENT_W,
      CONTENT_H,
      false
    );
    push_frame_rect(
      VISUAL_X,
      VISUAL_Y,
      VISUAL_W,
      VISUAL_H,
      false
    );
  }

  // Rail and temperature marker arrive last.
  copy_frame_rect(
    g_frame,
    g_base_frame,
    PAGE_RAIL_X,
    PAGE_RAIL_Y,
    PAGE_RAIL_W,
    PAGE_RAIL_H
  );
  g_page_indicator_x = page_indicator_target_x(g_page);
  draw_page_indicator(g_frame, g_page_indicator_x);

  if (climate.power_on && g_page == PAGE_TEMP) {
    calculate_marker_position(
      g_marker_x,
      g_marker_y
    );

    draw_marker(
      g_frame,
      g_marker_x,
      g_marker_y
    );

    g_marker_visible = true;
  }

  wait_for_te_edge();
  push_frame_rect(
    PAGE_RAIL_X,
    PAGE_RAIL_Y,
    PAGE_RAIL_W,
    PAGE_RAIL_H,
    false
  );

  if (g_marker_visible) {
    push_frame_rect(
      g_marker_x - MARKER_PATCH / 2,
      g_marker_y - MARKER_PATCH / 2,
      MARKER_PATCH,
      MARKER_PATCH,
      false
    );
  }
}

// ============================================================
// EXISTING STARTUP LOGO -> GFX FRAMEBUFFER
// ============================================================
#if USE_EXISTING_LVGL_LOGO
static uint16_t lvgl_logo_pixel_to_rgb565(const uint8_t *source) {
  lv_color_t color;
  memcpy(&color, source, sizeof(lv_color_t));
  uint16_t value = color.full;

  #if LV_COLOR_16_SWAP
    value = (uint16_t)((value << 8) | (value >> 8));
  #endif

  return value;
}

static bool render_existing_logo(uint16_t *target) {
  const lv_img_dsc_t &logo = logomodre256;
  const uint16_t width = logo.header.w;
  const uint16_t height = logo.header.h;
  const int16_t origin_x = (LCD_W - width) / 2;
  const int16_t origin_y = (LCD_H - height) / 2;

  const bool has_alpha = logo.header.cf == LV_IMG_CF_TRUE_COLOR_ALPHA;
  const bool chroma_keyed =
    logo.header.cf == LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED;
  const bool plain = logo.header.cf == LV_IMG_CF_TRUE_COLOR;
  const lv_color_t chroma_key = LV_COLOR_CHROMA_KEY;

  if (!has_alpha && !chroma_keyed && !plain) return false;

  const uint8_t bytes_per_pixel = has_alpha ? 3 : 2;

  for (uint16_t y = 0; y < height; ++y) {
    for (uint16_t x = 0; x < width; ++x) {
      const uint8_t *source =
        logo.data + ((size_t)y * width + x) * bytes_per_pixel;
      const uint16_t color = lvgl_logo_pixel_to_rgb565(source);
      const uint8_t alpha = has_alpha ? source[2] : 255;

      if (chroma_keyed && color == chroma_key.full) continue;
      blend_pixel(target, origin_x + x, origin_y + y, color, alpha);
    }
  }

  return true;
}
#endif

static void render_fallback_logo(uint16_t *target) {
  draw_text_centered(
    target,
    SD_FONT_LABEL,
    "OJM SYSTEMS",
    LCD_CX,
    242,
    COL_WHITE,
    255,
    2
  );

  fill_rect(
    target,
    LCD_CX - 74,
    259,
    148,
    4,
    COL_BLUE
  );
}

static void render_startup_logo_frame() {
  fill_rect(g_frame, 0, 0, LCD_W, LCD_H, COL_BLACK);

  bool rendered = false;
#if USE_EXISTING_LVGL_LOGO
  rendered = render_existing_logo(g_frame);
#endif

  if (!rendered) {
    render_fallback_logo(g_frame);
  }
}

static void run_logo_formation() {
  static constexpr uint8_t COLLISION_FRAMES = 26;
  static constexpr uint8_t IMPACT_FRAMES = 8;
  static constexpr int16_t DOT_MARGIN = 10;

  const float arc_radius =
    (STARTUP_RING_OUTER + STARTUP_RING_INNER) * 0.5f;
  const float left_angle = ARC_START_DEG * DEG_TO_RAD;
  const float right_angle =
    (ARC_START_DEG + ARC_SPAN_DEG) * DEG_TO_RAD;

  const int16_t left_start_x =
    (int16_t)lroundf(LCD_CX + cosf(left_angle) * arc_radius);
  const int16_t right_start_x =
    (int16_t)lroundf(LCD_CX + cosf(right_angle) * arc_radius);
  const int16_t start_y =
    (int16_t)lroundf(LCD_CY + sinf(left_angle) * arc_radius);

  const int16_t collision_x =
    (left_start_x - DOT_MARGIN) & ~1;
  const int16_t collision_y =
    (LCD_CY - DOT_MARGIN) & ~1;
  const int16_t collision_right =
    right_start_x + DOT_MARGIN;
  const int16_t collision_bottom =
    start_y + DOT_MARGIN;
  const int16_t collision_width =
    ((collision_right - collision_x + 2) / 2) * 2;
  const int16_t collision_height =
    ((collision_bottom - collision_y + 2) / 2) * 2;

  fill_rect(g_frame, 0, 0, LCD_W, LCD_H, COL_BLACK);
  display->fillScreen(COL_BLACK);

  uint32_t next_frame_us = micros();

  for (
    uint8_t frame = 0;
    frame <= COLLISION_FRAMES;
    ++frame
  ) {
    const float linear =
      (float)frame / COLLISION_FRAMES;
    const float eased = startup_ease(linear);
    const int16_t new_left_x =
      (int16_t)lroundf(
        left_start_x + (LCD_CX - left_start_x) * eased
      );
    const int16_t new_right_x =
      (int16_t)lroundf(
        right_start_x + (LCD_CX - right_start_x) * eased
      );
    const int16_t new_y =
      (int16_t)lroundf(
        start_y + (LCD_CY - start_y) * eased
      );

    // Fixed window, just like the clean vertical travel animation.
    fill_rect(
      g_frame,
      collision_x,
      collision_y,
      collision_width,
      collision_height,
      COL_BLACK
    );

    if (frame == COLLISION_FRAMES) {
      fill_circle_aa(
        g_frame,
        LCD_CX,
        LCD_CY,
        5.0f,
        COL_WHITE,
        255
      );
    } else {
      const float mix_start = clamp_float(
        (eased - 0.72f) / 0.28f,
        0.0f,
        1.0f
      );
      const uint8_t mix_alpha =
        (uint8_t)lroundf(mix_start * 255.0f);

      fill_circle_aa(
        g_frame,
        new_left_x,
        new_y,
        5.0f,
        blend_rgb565(COL_BLUE, COL_WHITE, mix_alpha),
        255
      );

      fill_circle_aa(
        g_frame,
        new_right_x,
        new_y,
        5.0f,
        blend_rgb565(COL_RED, COL_WHITE, mix_alpha),
        255
      );
    }

    push_frame_rect(
      collision_x,
      collision_y,
      collision_width,
      collision_height,
      false
    );

    next_frame_us += STARTUP_FRAME_US;
    const int32_t wait_us =
      (int32_t)(next_frame_us - micros());

    if (wait_us > 0) {
      delayMicroseconds((uint32_t)wait_us);
    } else {
      next_frame_us = micros();
    }
  }

  next_frame_us = micros();
  for (uint8_t frame = 1; frame <= IMPACT_FRAMES; ++frame) {
    const float linear = (float)frame / IMPACT_FRAMES;
    const float radius =
      5.0f + sinf(linear * PI) * 3.2f;

    fill_rect(
      g_frame,
      LCD_CX - DOT_MARGIN,
      LCD_CY - DOT_MARGIN,
      DOT_MARGIN * 2 + 1,
      DOT_MARGIN * 2 + 1,
      COL_BLACK
    );

    fill_circle_aa(
      g_frame,
      LCD_CX,
      LCD_CY,
      radius,
      COL_WHITE,
      255
    );

    push_frame_rect(
      LCD_CX - DOT_MARGIN,
      LCD_CY - DOT_MARGIN,
      DOT_MARGIN * 2 + 1,
      DOT_MARGIN * 2 + 1,
      false
    );

    next_frame_us += STARTUP_FRAME_US;
    const int32_t wait_us =
      (int32_t)(next_frame_us - micros());

    if (wait_us > 0) {
      delayMicroseconds((uint32_t)wait_us);
    } else {
      next_frame_us = micros();
    }
  }

  render_startup_logo_frame();

  static constexpr int16_t REVEAL_Y = LCD_CY - 144;
  static constexpr int16_t REVEAL_H = 288;
  static constexpr int16_t STRIP_W = 8;
  static constexpr int16_t REVEAL_SEED_HALF = 10;
  static constexpr uint8_t REVEAL_STEPS = 17;

  wait_for_te_edge();
  push_frame_rect(
    LCD_CX - REVEAL_SEED_HALF,
    REVEAL_Y,
    REVEAL_SEED_HALF * 2,
    REVEAL_H,
    false
  );
  next_frame_us = micros();

  for (
    uint8_t step = 0;
    step < REVEAL_STEPS;
    ++step
  ) {
    const int16_t left_x =
      (LCD_CX - REVEAL_SEED_HALF) -
      (step + 1) * STRIP_W;

    const int16_t right_x =
      (LCD_CX + REVEAL_SEED_HALF) +
      step * STRIP_W;

    push_frame_rect(
      left_x,
      REVEAL_Y,
      STRIP_W,
      REVEAL_H,
      false
    );

    push_frame_rect(
      right_x,
      REVEAL_Y,
      STRIP_W,
      REVEAL_H,
      false
    );

    next_frame_us += STARTUP_FRAME_US;

    const int32_t wait_us =
      (int32_t)(next_frame_us - micros());

    if (wait_us > 0) {
      delayMicroseconds((uint32_t)wait_us);
    } else {
      next_frame_us = micros();
    }
  }

  // Two seconds is long enough to read the mark.
  const uint32_t hold_started = millis();
  const uint32_t hold_elapsed =
    millis() - hold_started;

  if (hold_elapsed < STARTUP_LOGO_HOLD_MS) {
    delay(STARTUP_LOGO_HOLD_MS - hold_elapsed);
  }

  // Close the same strips in reverse.
  static constexpr uint8_t COLLAPSE_STEPS = 16;
  next_frame_us = micros();
  wait_for_te_edge();

  for (
    uint8_t step = 0;
    step < COLLAPSE_STEPS;
    ++step
  ) {
    const int16_t left_x =
      (LCD_CX - 1) -
      (COLLAPSE_STEPS - step) * STRIP_W;

    const int16_t right_x =
      (LCD_CX + 1) +
      (COLLAPSE_STEPS - 1 - step) * STRIP_W;

    fill_rect(
      g_frame,
      left_x,
      REVEAL_Y,
      STRIP_W,
      REVEAL_H,
      COL_BLACK
    );

    fill_rect(
      g_frame,
      right_x,
      REVEAL_Y,
      STRIP_W,
      REVEAL_H,
      COL_BLACK
    );

    push_frame_rect(
      left_x,
      REVEAL_Y,
      STRIP_W,
      REVEAL_H,
      false
    );

    push_frame_rect(
      right_x,
      REVEAL_Y,
      STRIP_W,
      REVEAL_H,
      false
    );

    next_frame_us += STARTUP_FRAME_US;
    const int32_t wait_us =
      (int32_t)(next_frame_us - micros());

    if (wait_us > 0) {
      delayMicroseconds((uint32_t)wait_us);
    } else {
      next_frame_us = micros();
    }
  }

  // Clear the last logo slice and leave one blue dot in the middle.
  fill_rect(
    g_frame,
    LCD_CX - REVEAL_SEED_HALF,
    REVEAL_Y,
    REVEAL_SEED_HALF * 2,
    REVEAL_H,
    COL_BLACK
  );

  fill_circle_aa(
    g_frame,
    LCD_CX,
    LCD_CY,
    5.0f,
    COL_BLUE,
    255
  );

  wait_for_te_edge();
  push_frame_rect(
    LCD_CX - REVEAL_SEED_HALF,
    REVEAL_Y,
    REVEAL_SEED_HALF * 2,
    REVEAL_H,
    false
  );
  push_frame_rect(
    LCD_CX - DOT_MARGIN,
    LCD_CY - DOT_MARGIN,
    DOT_MARGIN * 2 + 1,
    DOT_MARGIN * 2 + 1,
    false
  );

  // Clear the full lane every frame so AA pixels cannot leave a trail.
  static constexpr uint8_t TRAVEL_FRAMES = 16;
  const int16_t arc_seed_y =
    LCD_CY -
    (STARTUP_RING_OUTER + STARTUP_RING_INNER) / 2;
  static constexpr int16_t TRAVEL_HALF_WIDTH = 10;
  const int16_t travel_y = arc_seed_y - TRAVEL_HALF_WIDTH;
  const int16_t travel_height =
    LCD_CY - arc_seed_y + TRAVEL_HALF_WIDTH * 2 + 1;
  next_frame_us = micros();

  for (
    uint8_t frame = 1;
    frame <= TRAVEL_FRAMES;
    ++frame
  ) {
    const float linear =
      (float)frame / TRAVEL_FRAMES;
    const float eased = startup_ease(linear);
    const int16_t new_y =
      (int16_t)lroundf(
        LCD_CY +
        (arc_seed_y - LCD_CY) * eased
      );

    fill_rect(
      g_frame,
      LCD_CX - TRAVEL_HALF_WIDTH,
      travel_y,
      TRAVEL_HALF_WIDTH * 2 + 1,
      travel_height,
      COL_BLACK
    );

    const uint16_t point_color = blend_rgb565(
      COL_BLUE,
      COL_WHITE,
      (uint8_t)lroundf(eased * 255.0f)
    );

    fill_circle_aa(
      g_frame,
      LCD_CX,
      new_y,
      4.8f,
      point_color,
      255
    );

    push_frame_rect(
      LCD_CX - TRAVEL_HALF_WIDTH,
      travel_y,
      TRAVEL_HALF_WIDTH * 2 + 1,
      travel_height,
      false
    );

    next_frame_us += STARTUP_FRAME_US;
    const int32_t wait_us =
      (int32_t)(next_frame_us - micros());

    if (wait_us > 0) {
      delayMicroseconds((uint32_t)wait_us);
    } else {
      next_frame_us = micros();
    }
  }
}

// ============================================================
// MEMORY + SETUP / LOOP
// ============================================================
static void allocate_buffers() {
  g_base_frame = (uint16_t *)heap_caps_malloc(
    FRAME_BYTES,
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
  );
  g_frame = (uint16_t *)heap_caps_malloc(
    FRAME_BYTES,
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
  );
  g_push_buffer = (uint16_t *)heap_caps_malloc(
    PUSH_BYTES,
    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
  );
  if (
    !g_base_frame ||
    !g_frame ||
    !g_push_buffer
  ) {
    Serial.println("FATAL: PSRAM framebuffer allocation failed");
    while (true) delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println("BOOT: Smart Dial PREMIUM SNAPPY V5.5 CAN");

  pinMode(ENCODER_BUTTON, INPUT_PULLUP);

ESP32Encoder::useInternalWeakPullResistors = puType::up;

encoder.attachFullQuad(ENCODER_A, ENCODER_B);
encoder.setFilter(1023);
encoder.clearCount();

g_encoder_last_raw = 0;
g_encoder_substeps = 0;

  if (!display->begin(DISPLAY_BUS_HZ)) {
    Serial.println("FATAL: display begin failed");
    while (true) delay(1000);
  }

  enable_te_output();
  allocate_buffers();

  set_brightness(0);
  display->fillScreen(COL_BLACK);
  delay(STARTUP_SETTLE_MS);
  set_brightness(DISPLAY_BRIGHTNESS);
  delay(45);

  run_logo_formation();

  run_runtime_arc_load();
  reveal_startup_content();

  Serial.printf(
    "BOOT: ready | QSPI %ld MHz | PSRAM %s | buffers %.1f kB\n",
    DISPLAY_BUS_HZ / 1000000L,
    psramFound() ? "OK" : "MISSING",
    (
      FRAME_BYTES * 2 +
      PUSH_BYTES
    ) / 1024.0f
  );

  if (can_begin()) {
    Serial.println("CAN: ready | 500 kbit/s | TX 0x201 | RX 0x301");
  } else {
    Serial.println("CAN: initialization failed, UI remains available");
  }
}

void loop() {
  const int16_t encoder_delta = consume_encoder_delta();
  if (encoder_delta != 0) {
    handle_encoder_rotation(encoder_delta);
  }

  const ButtonEvent button = read_encoder_button();
  if (button != BUTTON_NONE) {
    handle_button(button);
  }

  g_animation_te_used = false;
  update_page_transition();
  update_visual_animation();
  start_page_transition_if_needed();

  can_publish_state_snapshot();
  service_can();

  delay(1);
}
