#include "../apps/wallet/wallet.h"
#include "tezos_core.h"
#include "tezos_gfx.h"
#include <stdio.h>

int main(void) {
    tezos_fb_t *fb = tezos_fb_create(320, 480);
    tezos_core_t core;
    tezos_core_init(&core, fb);

    tezos_core_push(&core, wallet_entry_screen());
    core.needs_render = true;
    tezos_core_tick(&core, NULL);
    tezos_fb_dump_ppm(fb, "wallet_01_balances.ppm");
    printf("wallet_01_balances.ppm — Balances page (real address, mock balance)\n");

    /* flip through all 6 pages via Down */
    const char *page_names[] = {"balances","transactions","contacts","domain","staking","tzkt_info"};
    tezos_input_event_t down = {TEZOS_INPUT_DOWN, 0};
    for (int i = 1; i < 6; i++) {
        tezos_core_tick(&core, &down);
        char fname[64];
        snprintf(fname, sizeof fname, "wallet_%02d_%s.ppm", i + 1, page_names[i]);
        tezos_fb_dump_ppm(fb, fname);
        printf("%s\n", fname);
    }

    /* go back to Staking page (index 4) and press SELECT to actually
       forge+sign a real delegation via the mock signer */
    tezos_input_event_t up = {TEZOS_INPUT_UP, 0};
    tezos_core_tick(&core, &up); /* tzkt_info -> staking */
    tezos_input_event_t select = {TEZOS_INPUT_SOFTKEY_L, 0};
    tezos_core_tick(&core, &select);
    tezos_fb_dump_ppm(fb, "wallet_07_staking_signed.ppm");
    printf("wallet_07_staking_signed.ppm — after pressing DELEGATE (real forge+sign+self-verify)\n");

    /* go back to Balances and press SELECT (SEND) to push the Send confirm screen */
    for (int i = 0; i < 4; i++) tezos_core_tick(&core, &up); /* staking -> ... -> balances */
    tezos_core_tick(&core, &select);
    tezos_fb_dump_ppm(fb, "wallet_08_send_confirm.ppm");
    printf("wallet_08_send_confirm.ppm — Send confirm screen pushed\n");

    tezos_core_tick(&core, &select); /* SIGN */
    tezos_fb_dump_ppm(fb, "wallet_09_send_signed.ppm");
    printf("wallet_09_send_signed.ppm — after pressing SIGN (real forge+sign+self-verify)\n");

    tezos_fb_destroy(fb);
    return 0;
}
