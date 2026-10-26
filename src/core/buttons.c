/* SPDX-License-Identifier: MIT */
#include "uke/boot_core.h"

void uke_buttons_init(UKE_BUTTONS *b, uint64_t now, uint8_t held) {
  unsigned i;
  if (!b) return;
  b->last_ms = now;
  for (i = 0; i < UKE_KEY_COUNT; ++i) {
    UKE_KEY_STATE *s = &b->keys[i];
    s->changed_ms = now; s->pressed_ms = now; s->next_repeat_ms = now;
    s->sampled = s->stable = (uint8_t)(((unsigned)held >> i) & 1u);
    s->armed = !s->stable;
  }
}
void uke_buttons_poll(UKE_BUTTONS *b, uint64_t now, uint8_t held, UKE_KEY_EVENT events[UKE_KEY_COUNT]) {
  unsigned i;
  if (!events) return;
  for (i = 0; i < UKE_KEY_COUNT; ++i) events[i] = UKE_EVENT_NONE;
  if (!b) return;
  /* A backwards clock invalidates pending actions, rather than synthesizing a release. */
  if (now < b->last_ms) { uke_buttons_init(b, now, held); return; }
  b->last_ms = now;
  for (i = 0; i < UKE_KEY_COUNT; ++i) {
    UKE_KEY_STATE *s = &b->keys[i];
    uint8_t down = (uint8_t)(((unsigned)held >> i) & 1u);
    if (down != s->sampled) { s->sampled = down; s->changed_ms = now; }
    if (s->sampled != s->stable && now - s->changed_ms >= 30) {
      s->stable = s->sampled;
      if (s->stable) {
        s->pressed_ms = now;
        s->next_repeat_ms = now > UINT64_MAX - 500 ? UINT64_MAX : now + 500;
        if (s->armed && i != UKE_KEY_POWER)
          events[i] = i == UKE_KEY_UP ? UKE_EVENT_PREVIOUS : UKE_EVENT_NEXT;
      } else {
        if (s->armed && i == UKE_KEY_POWER && now - s->pressed_ms < 800)
          events[i] = UKE_EVENT_SELECT;
        s->armed = 1;
      }
    } else if (s->stable && s->sampled && s->armed && i != UKE_KEY_POWER &&
               now < UINT64_MAX && now >= s->next_repeat_ms) {
      events[i] = i == UKE_KEY_UP ? UKE_EVENT_PREVIOUS : UKE_EVENT_NEXT;
      s->next_repeat_ms = now > UINT64_MAX - 120 ? UINT64_MAX : now + 120;
    }
  }
  /* Chords are reserved, so they never implicitly select an OS. */
  if ((held & 7u) != 0 && ((held & 7u) & ((held & 7u) - 1u)) != 0) {
    for (i = 0; i < UKE_KEY_COUNT; ++i) { events[i] = UKE_EVENT_NONE; b->keys[i].armed = 0; }
  }
}
