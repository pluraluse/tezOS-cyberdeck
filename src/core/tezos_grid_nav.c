#include "tezos_grid_nav.h"

int idle_grid_next_selected(int selected, int count, tezos_input_type_t type, int value) {
    if (type == TEZOS_INPUT_NUM && value >= 1 && value <= 9) {
        int idx = value - 1;
        return (idx < count) ? idx : selected; /* real, deliberate no-op --
            a numpad press beyond the real app count shouldn't move
            selection into a real, nonexistent slot */
    }
    if (type == TEZOS_INPUT_UP) return (selected - 1 + count) % count;
    if (type == TEZOS_INPUT_DOWN) return (selected + 1) % count;
    return selected; /* real, deliberate no-op for any other input type */
}
