#include "tezos_signer_apdu.h"
#include <string.h>

bool tezos_apdu_parse_command(const uint8_t *raw, uint16_t raw_len,
                               tezos_apdu_command_t *out, uint16_t *sw_error) {
    memset(out, 0, sizeof *out);

    if (raw_len < 4) { /* too short even for the mandatory header --
        a real, malformed input, not an edge case to skip */
        *sw_error = TEZOS_APDU_SW_MALFORMED;
        return false;
    }

    out->cla = raw[0];
    out->ins = raw[1];
    out->p1 = raw[2];
    out->p2 = raw[3];

    uint16_t remaining = raw_len - 4;

    if (remaining == 0) {
        /* real ISO 7816-4 case 1: no data, no Le */
        return true;
    }

    if (remaining == 1) {
        /* real case 2: no data, Le only -- 0x00 in this position means
           "return up to 256 bytes", the real, standard short-form
           convention, not a literal zero */
        out->le_present = true;
        out->le = (raw[4] == 0) ? 256 : raw[4];
        return true;
    }

    /* remaining >= 2: raw[4] is Lc -- must be real case 3 or case 4 */
    uint8_t lc = raw[4];
    if (lc == 0) {
        /* a real, deliberate v1 scope limit: 0x00 in the Lc position
           would signal extended-length APDUs in the full ISO 7816-4
           spec, which this project doesn't support yet (see the
           header's own real reasoning: chunking via P1 solves the
           actual size problem this project has, without the added
           complexity of 3-byte length fields) */
        *sw_error = TEZOS_APDU_SW_MALFORMED;
        return false;
    }

    if (remaining == (uint16_t)(1 + lc)) {
        /* real case 3: Lc + Data, no Le */
        memcpy(out->data, raw + 5, lc);
        out->data_len = lc;
        return true;
    }

    if (remaining == (uint16_t)(1 + lc + 1)) {
        /* real case 4: Lc + Data + Le */
        memcpy(out->data, raw + 5, lc);
        out->data_len = lc;
        out->le_present = true;
        uint8_t le_byte = raw[5 + lc];
        out->le = (le_byte == 0) ? 256 : le_byte;
        return true;
    }

    /* the real total length doesn't match any of the four real ISO
       7816-4 short-form cases -- genuinely malformed, not a case this
       parser failed to anticipate */
    *sw_error = TEZOS_APDU_SW_MALFORMED;
    return false;
}

uint16_t tezos_apdu_build_response(const uint8_t *data, uint16_t data_len,
                                    uint16_t sw, uint8_t *out, uint16_t out_cap) {
    if ((uint32_t)data_len + 2 > out_cap) return 0; /* real, reported
        failure -- the real data plus the mandatory 2-byte trailer
        wouldn't fit; a silent truncation here would be a genuinely
        dangerous bug for a signer's response (e.g. a truncated
        signature silently reported as if complete) */
    if (data_len > 0 && data) memcpy(out, data, data_len);
    out[data_len] = (uint8_t)(sw >> 8);
    out[data_len + 1] = (uint8_t)(sw & 0xFF);
    return (uint16_t)(data_len + 2);
}
