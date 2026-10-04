#pragma once
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

