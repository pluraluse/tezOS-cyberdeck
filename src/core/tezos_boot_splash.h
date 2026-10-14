#pragma once
/*
 * Boot splash — an original animation in the spirit of the classic
 * 16-bit console boot logo (a bright gleam sweeping across a wordmark
 * on a dark screen) built for tezOS's own name and palette, not a
 * reproduction of anyone else's specific artwork. Shown once, first,
 * before the real home screen — the very first thing a person sees
 * when the device boots.
 *
 * Auto-advances to the home screen after the animation completes, and
 * a real key press skips it immediately too -- but neither exit path
 * is driven by this screen's own handle_input return value. The splash
 * is pushed as the very first (and, briefly, only) screen on the
 * stack, so returning TEZOS_ACTION_POP would be a silent no-op
 * (tezos_core_pop() correctly refuses to ever pop the last screen);
 * and this core module has no business hardcoding a dependency on
 * whatever the platform shim's specific idle/home screen happens to
 * be, so it can't return TEZOS_ACTION_REPLACE either. Instead, both
 * "the animation finished" and "the user pressed a key" just set
 * internal state, and tezos_boot_splash_is_finished() reports true for
 * either — the calling shim's main loop is what actually performs the
 * real transition to its own home screen, checked after every
 * on_tick/input dispatch, the same one real mechanism for both exit
 * paths rather than two different ones.
 */
#include "tezos_core.h"

tezos_screen_t *tezos_boot_splash_screen_get(void);

/* True once the splash should be dismissed -- either its own animation
   duration has fully elapsed, or the user pressed any key to skip it.
   Safe to call at any time; false if the passed screen isn't actually
   the splash screen (a real caller-safety check, not just
   documentation, since a stale pointer here would otherwise silently
   never report true). */
bool tezos_boot_splash_is_finished(tezos_screen_t *splash_screen);
