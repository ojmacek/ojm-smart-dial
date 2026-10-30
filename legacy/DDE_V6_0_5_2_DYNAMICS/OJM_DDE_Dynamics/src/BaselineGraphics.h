#pragma once
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

