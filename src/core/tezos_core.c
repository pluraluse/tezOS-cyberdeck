#include "tezos_core.h"
#include <stdio.h>

void tezos_core_init(tezos_core_t *core, tezos_fb_t *fb) {
    core->depth = 0;
    core->app_count = 0;
    core->fb = fb;
    core->needs_render = false;
    tezos_dirty_reset(&core->dirty);
}

bool tezos_core_push(tezos_core_t *core, tezos_screen_t *screen) {
    if (core->depth >= TEZOS_MAX_STACK_DEPTH) {
        /* Safe failure: refuse the push, stay where we are. A screen
           that hits this in practice has a navigation bug worth fixing,
           but the device stays usable instead of corrupting memory. */
        fprintf(stderr, "[tezos_core] push refused: stack full (depth=%d)\n", core->depth);
        return false;
    }
    core->stack[core->depth++] = screen;
    core->needs_render = true;
    return true;
}

bool tezos_core_pop(tezos_core_t *core) {
    if (core->depth <= 1) {
        /* Never pop the last screen (idle) — there must always be
           something on top of the stack for input to go to. */
        return false;
    }
    core->depth--;
    tezos_screen_t *resumed = core->stack[core->depth - 1];
    if (resumed->on_resume) resumed->on_resume(resumed);
    core->needs_render = true;
    return true;
}

void tezos_core_replace(tezos_core_t *core, tezos_screen_t *screen) {
    if (core->depth == 0) {
        core->stack[core->depth++] = screen;
    } else {
        core->stack[core->depth - 1] = screen;
    }
    core->needs_render = true;
}

bool tezos_core_register_app(tezos_core_t *core, tezos_app_t app) {
    if (core->app_count >= TEZOS_MAX_APPS) return false;
    core->apps[core->app_count++] = app;
    return true;
}

void tezos_core_tick(tezos_core_t *core, const tezos_input_event_t *ev) {
    if (ev && core->depth > 0) {
        tezos_screen_t *top = core->stack[core->depth - 1];
        tezos_screen_t *next = NULL;
        bool needs_redraw = false;
        tezos_action_t action = top->handle_input(top, *ev, &next, &needs_redraw);
        if (needs_redraw) core->needs_render = true;
        switch (action) {
            case TEZOS_ACTION_PUSH:
                if (next) tezos_core_push(core, next);
                break;
            case TEZOS_ACTION_POP:
                tezos_core_pop(core);
                break;
            case TEZOS_ACTION_REPLACE:
                if (next) tezos_core_replace(core, next);
                break;
            case TEZOS_ACTION_NONE:
            default:
                break;
        }
    }

    if (core->needs_render && core->depth > 0) {
        tezos_dirty_reset(&core->dirty);
        tezos_screen_t *top = core->stack[core->depth - 1];
        top->render(top, core->fb, &core->dirty);
        core->needs_render = false;
        /* Platform shim (real hardware) would now flip core->dirty's
           rects to the DRM buffer here. Dev harness just leaves the
           result in core->fb for the caller to dump/inspect. */
    }
}
