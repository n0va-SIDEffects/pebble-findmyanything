#include "beeper.h"
#include "sounds.h"
#ifndef BEEPER_HOST_TEST
#include "audio_pump.h"
#endif

#define RATE 16000

static struct {
  bool active;
  uint8_t kind;
  uint16_t pos;
  uint32_t phase, inc;   // 32-bit phase accumulator, top 8 bits = table index
} s_cur;

static struct {
  bool pending;
  uint8_t kind;
  uint32_t inc;
} s_next;

void beeper_trigger(int kind, uint16_t freq_hz) {
  s_next.kind = (uint8_t)kind;
  s_next.inc = (uint32_t)(((uint64_t)freq_hz << 32) / RATE);
  s_next.pending = true;
}

void beeper_render(int16_t *out, uint16_t n) {
  for (uint16_t i = 0; i < n; i++) {
    if (!s_cur.active && s_next.pending) {
      s_cur.active = true;
      s_cur.kind = s_next.kind;
      s_cur.inc = s_next.inc;
      s_cur.phase = 0;
      s_cur.pos = 0;
      s_next.pending = false;
    }
    int32_t v = 0;
    if (s_cur.active) {
      if (s_cur.kind == BEEPER_BEEP) {
        // Sine from the table, linearly interpolated, times the envelope
        uint32_t idx = s_cur.phase >> 24;
        int32_t frac = (int32_t)((s_cur.phase >> 8) & 0xFFFF);
        int32_t a = SINE_TABLE[idx], b = SINE_TABLE[idx + 1];
        int32_t s = a + (((b - a) * frac) >> 16);
        v = (s * (int32_t)BEEP_ENV[s_cur.pos]) >> 15;
        s_cur.phase += s_cur.inc;
        if (++s_cur.pos >= BEEP_ENV_LEN) s_cur.active = false;
      } else {
        v = SOUND_CLICK[s_cur.pos];
        if (++s_cur.pos >= SOUND_CLICK_LEN) s_cur.active = false;
      }
    }
    out[i] = (int16_t)v;
  }
}

#ifndef BEEPER_HOST_TEST
bool beeper_start(uint8_t volume) {
  if (audio_pump_is_running()) return true;
  s_cur.active = false;
  s_next.pending = false;
  return audio_pump_start(beeper_render, volume);
}

void beeper_stop(void) {
  audio_pump_stop();
  s_cur.active = false;
  s_next.pending = false;
}

bool beeper_running(void) { return audio_pump_is_running(); }

void beeper_set_volume(uint8_t volume) { audio_pump_set_volume(volume); }

uint16_t beeper_latency_ms(void) { return audio_pump_lead_ms(); }
#endif
