#pragma once
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

