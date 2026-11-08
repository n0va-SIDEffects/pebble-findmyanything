// Search sound: one PCM stream stays open for the whole search, the beeps are
// synthesised into it (no restart of the audio path per beep, which crackled).
#pragma once
#include <pebble.h>

enum { BEEPER_BEEP, BEEPER_CLICK };

//! Opens the stream. false if the speaker is unavailable (then use the fallback).
bool beeper_start(uint8_t volume);
void beeper_stop(void);
bool beeper_running(void);
void beeper_set_volume(uint8_t volume);

//! Queues one sound; it starts as soon as the previous one has finished, so a
//! sound is never cut off mid-wave. freq_hz is ignored for the click.
void beeper_trigger(int kind, uint16_t freq_hz);

//! How long until a sound triggered now is heard (for syncing the vibration).
uint16_t beeper_latency_ms(void);

//! Render one block (exposed for the host test in tools/beeper_test.c).
void beeper_render(int16_t *out, uint16_t n);
