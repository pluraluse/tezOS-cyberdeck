/* Real unit tests for wallet_tez_to_mutez -- a wrong conversion here
   means sending the wrong amount of real money, so this gets direct
   verification, not just trust via a UI flow. */
#include "../src/apps/wallet/wallet.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static bool convert_and_check(const char *tez, const char *expected_mutez) {
    char out[24];
    if (!wallet_tez_to_mutez(tez, out, sizeof out)) return false;
    return strcmp(out, expected_mutez) == 0;
}

int main(void) {
    CHECK(convert_and_check("1", "1000000"), "whole number '1' -> 1000000 mutez");
    CHECK(convert_and_check("1.5", "1500000"), "'1.5' -> 1500000 mutez");
    CHECK(convert_and_check("0.25", "250000"), "'0.25' -> 250000 mutez");
    CHECK(convert_and_check("1.123456", "1123456"), "max precision (6 places) '1.123456' -> 1123456 mutez");
    CHECK(convert_and_check("0.000001", "1"), "smallest possible amount '0.000001' -> 1 mutez");
    CHECK(convert_and_check("0", "0"), "zero '0' -> 0 mutez (still a well-formed conversion, real caller decides if 0 is a valid send)");
    CHECK(convert_and_check("100", "100000000"), "larger whole number '100' -> 100000000 mutez");
    CHECK(convert_and_check("10.1", "10100000"), "'10.1' -> 10100000 mutez (short fractional part correctly right-padded)");

    /* real failure cases -- must be rejected cleanly, not silently rounded/truncated */
    char out[24];
    CHECK(!wallet_tez_to_mutez("1.1234567", out, sizeof out), "7 decimal places (more precision than tez has) is correctly rejected");
    CHECK(!wallet_tez_to_mutez("", out, sizeof out), "empty string is correctly rejected");
    CHECK(!wallet_tez_to_mutez("1.2.3", out, sizeof out), "multiple decimal points correctly rejected");
    CHECK(!wallet_tez_to_mutez("abc", out, sizeof out), "non-numeric input correctly rejected");
    CHECK(!wallet_tez_to_mutez("1.5a", out, sizeof out), "trailing non-numeric character correctly rejected");
    CHECK(!wallet_tez_to_mutez(".5", out, sizeof out), "missing integer part (bare '.5') correctly rejected");

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
