#pragma once
static ESP32Encoder encoder;

static int64_t g_encoder_last_raw = 0;
static int64_t g_encoder_substeps = 0;

static int16_t consume_encoder_delta() {
  const int64_t raw_now = encoder.getCount();
  const int64_t raw_delta = raw_now - g_encoder_last_raw;

  g_encoder_last_raw = raw_now;
  g_encoder_substeps += raw_delta;

  // A detent can settle one edge short of its nominal four-count centre.
  // Symmetric hysteresis accepts that detent in either direction. The centre
  // still advances by FOUR counts, so the last edge cannot create a second
  // step and a one-edge rebound cannot reverse an already accepted step.
  // Never clear pending counts on a click or simply reset them on reversal.
  static_assert(ENCODER_TRANSITIONS_PER_STEP>0,"Invalid encoder resolution");
  constexpr int slack=ENCODER_TRANSITIONS_PER_STEP>=4?1:0;
  constexpr int threshold=ENCODER_TRANSITIONS_PER_STEP-slack;
  int64_t count=0;
  if(g_encoder_substeps>=threshold)
    count=(g_encoder_substeps+slack)/ENCODER_TRANSITIONS_PER_STEP;
  else if(g_encoder_substeps<=-threshold)
    count=(g_encoder_substeps-slack)/ENCODER_TRANSITIONS_PER_STEP;
  // Drain large bursts over subsequent polls instead of wrapping int16_t.
  int16_t steps=(int16_t)(count>32767?32767:count<-32767?-32767:count);

  g_encoder_substeps -=
    (int64_t)steps * ENCODER_TRANSITIONS_PER_STEP;

#if ENCODER_REVERSED
  steps = -steps;
#endif

  return steps;
}

// V6.0.6.7: read-only view of the debounced button for the hold ring (pressed and not yet a long press).
static bool g_button_down=false;
static uint32_t g_button_down_at=0;

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
      g_button_down = false;
      return BUTTON_SHORT;
    }
  }

  g_button_down = stable == LOW && !long_sent;
  g_button_down_at = pressed_at;

  if (
    stable == LOW &&
    !long_sent &&
    (now - pressed_at) >= BUTTON_LONG_MS
  ) {
    long_sent = true;
    g_button_down = false;
    return BUTTON_LONG;
  }

  return BUTTON_NONE;
}

