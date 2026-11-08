// Host test: renders beeps at several pitches the way the watch does and writes
// a WAV for listening/analysis. Build: see tools/beeper_test.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/c/beeper.h"

#define N (16000 * 3)
static int16_t buf[N];

int main(void) {
  // Far -> near: 900..1800 Hz, intervals like the watch (2 s .. 140 ms), plus
  // two triggers in a row to check that a beep is never cut off mid-wave.
  int pos = 0, kind = 0;
  const uint16_t freqs[] = {900, 1100, 1350, 1600, 1800, 1800};
  const int gaps[] = {4000, 3200, 2400, 1600, 960, 960};
  for (int k = 0; k < 6 && pos < N; k++) {
    beeper_trigger(kind, freqs[k]);
    if (k == 4) beeper_trigger(kind, 1700);  // second trigger while one is pending
    int n = gaps[k];
    for (int i = 0; i < n && pos < N; i += 256) {
      int m = (n - i) < 256 ? (n - i) : 256;
      if (pos + m > N) m = N - pos;
      beeper_render(buf + pos, (uint16_t)m);
      pos += m;
    }
  }
  beeper_trigger(1, 0);
  while (pos < N) { int m = N - pos < 256 ? N - pos : 256; beeper_render(buf + pos, (uint16_t)m); pos += m; }

  FILE *f = fopen(getenv("OUT") ? getenv("OUT") : "beeps.wav", "wb");
  uint32_t data = N * 2, riff = 36 + data, rate = 16000, br = 32000, sixteen = 16;
  uint16_t pcm = 1, ch = 1, align = 2, bits = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
  fwrite(&sixteen, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f);
  fwrite(&br, 4, 1, f); fwrite(&align, 2, 1, f); fwrite(&bits, 2, 1, f);
  fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f); fwrite(buf, 2, N, f); fclose(f);
  return 0;
}
