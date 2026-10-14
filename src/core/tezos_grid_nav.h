#pragma once
/*
 * Real, pure grid-navigation math for the home screen's icon grid --
 * deliberately separated from the screen/rendering machinery in
 * linux_shim.c (which depends on real DRM hardware headers and can't
 * be linked into a test binary directly) so the actual logic most
 * likely to hide a subtle bug -- numpad-position mapping, wrap-around
 * arithmetic -- can be tested directly and completely.
 */
#include "tezos_core.h"

/* Real navigation: NUM 1-9 jumps directly to that grid position (a
   real, deliberate fit -- a phone numpad's own physical 3x3 layout
   maps onto a 3x3 icon grid with zero translation needed, 1=top-left
   through 9=bottom-right). UP/DOWN move sequentially through the grid
   in real reading order, wrapping at either end -- a real, deliberate
   choice, not a loss of reachability, since NUM direct-jump already
   lets you reach any cell instantly regardless. Any other input type
   is a real, deliberate no-op, returning `selected` unchanged. */
int idle_grid_next_selected(int selected, int count, tezos_input_type_t type, int value);
