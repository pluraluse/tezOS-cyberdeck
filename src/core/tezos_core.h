#pragma once
/*
 * tezOS core — how screens, apps, input, and memory actually fit together.
 *
 * Memory model, decided up front rather than discovered as a bug later:
 * NO malloc/free in the input-dispatch or render hot path. Every screen
 * is a static singleton owned by its app (a fixed struct, not heap
 * allocated per push). The navigation stack holds POINTERS to these
 * statics, never copies or heap-allocated instances. This matters
 * specifically because this device may run for weeks between reboots —
 * a heap-churning navigation model (malloc a screen on push, free on
 * pop) would fragment slowly and unpredictably over that timescale,
 * exactly the kind of bug that's invisible in short test sessions and
 * shows up as a field failure. Fixed stack depth means "can't navigate
 * deeper" is a hard, safe failure mode instead of a heap exhaustion.
 */
#include "tezos_gfx.h"
#include <stdbool.h>

#define TEZOS_MAX_STACK_DEPTH 8
#define TEZOS_MAX_APPS 16

typedef enum {
    TEZOS_INPUT_NUM,        /* value = 0-9 */
    TEZOS_INPUT_STAR,
    TEZOS_INPUT_HASH,
    TEZOS_INPUT_UP,
    TEZOS_INPUT_DOWN,
    TEZOS_INPUT_SOFTKEY_L,
    TEZOS_INPUT_SOFTKEY_R,
    TEZOS_INPUT_TOUCH,      /* value = packed (x<<16 | y), rare path, most nav is keys */
} tezos_input_type_t;

typedef struct {
    tezos_input_type_t type;
    int value;
} tezos_input_event_t;

typedef enum {
    TEZOS_ACTION_NONE,   /* handled in place, nothing changes on the stack */
    TEZOS_ACTION_PUSH,   /* open a new screen on top (e.g. drill into a menu item) */
    TEZOS_ACTION_POP,    /* back to previous screen (right softkey convention) */
    TEZOS_ACTION_REPLACE /* swap current screen without growing the stack
                            (e.g. idle -> app home, so "back" from app home
                            correctly returns to idle, not to itself) */
} tezos_action_t;

typedef struct tezos_screen {
    /* Called when this screen needs to (re)draw. Screen decides its own
       dirty rects via the passed tezos_dirty_t — core doesn't guess. */
    void (*render)(struct tezos_screen *self, tezos_fb_t *fb, tezos_dirty_t *d);

    /* Called on every input event while this screen is on top of the
       stack. next_screen is an out-param, only meaningful if the return
       value is PUSH. needs_redraw is an out-param the screen sets to
       true if its own state changed in a way that requires a re-render
       (e.g. moving a menu highlight) even when the stack itself didn't
       change — this is what tells core to re-render after an
       ACTION_NONE, rather than assuming every input event is visible. */
    tezos_action_t (*handle_input)(struct tezos_screen *self, tezos_input_event_t ev,
                                    struct tezos_screen **next_screen, bool *needs_redraw);

    /* Optional: called when this screen becomes top-of-stack again after
       a pop (e.g. refresh a balance after returning from Send). NULL if
       the screen doesn't need this. */
    void (*on_resume)(struct tezos_screen *self);

    void *app_state; /* owned by the app, statically allocated, never freed by core */
    const char *debug_name; /* for logging only, not shown to users */
} tezos_screen_t;

typedef struct {
    int id;
    const char *name;         /* shown on idle screen's app list */
    tezos_screen_t *(*entry)(void); /* returns the app's static home screen */
} tezos_app_t;

typedef struct {
    tezos_screen_t *stack[TEZOS_MAX_STACK_DEPTH];
    int depth;

    tezos_app_t apps[TEZOS_MAX_APPS];
    int app_count;

    tezos_fb_t *fb;
    tezos_dirty_t dirty;
    bool needs_render; /* set on any stack change or explicit screen request */
} tezos_core_t;

void tezos_core_init(tezos_core_t *core, tezos_fb_t *fb);

/* Bounds-checked. Pushing past MAX_STACK_DEPTH is a no-op (logged, not a
   crash) — a real bug (a screen with a runaway push loop) should fail
   loud in dev builds and safe in shipped ones, not overrun memory. */
bool tezos_core_push(tezos_core_t *core, tezos_screen_t *screen);
bool tezos_core_pop(tezos_core_t *core); /* refuses to pop the last (idle) screen */
void tezos_core_replace(tezos_core_t *core, tezos_screen_t *screen);

bool tezos_core_register_app(tezos_core_t *core, tezos_app_t app);

/* One iteration: dispatch input (if any) to top-of-stack screen, apply
   any resulting stack action, render if needs_render is set. Called in
   a loop by the platform shim's main() — core has no idea whether input
   came from evdev (real hardware) or a test harness. */
void tezos_core_tick(tezos_core_t *core, const tezos_input_event_t *ev /* NULL if no input this tick */);
