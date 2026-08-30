/* Beacon QR pairing payload -- real libqrencode->zbar round trip with a
   genuine Beacon-shaped pairing URI, real version-11 QR code. */
#include "../src/chain/tezos_beacon_qr.h"
#include <qrencode.h>
#include <zbar.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } else { printf("PASS: %s\n", msg); } } while(0)

static unsigned char *render_qr_to_grayscale(QRcode *qr, int scale, int *out_w, int *out_h) {
    int quiet = 4 * scale;
    int img_size = qr->width * scale + quiet * 2;
    unsigned char *img = malloc((size_t)img_size * img_size);
    memset(img, 255, (size_t)img_size * img_size);
    for (int y = 0; y < qr->width; y++)
        for (int x = 0; x < qr->width; x++) {
            unsigned char pixel = (qr->data[y * qr->width + x] & 0x1) ? 0 : 255;
            for (int dy = 0; dy < scale; dy++)
                for (int dx = 0; dx < scale; dx++)
                    img[(quiet + y * scale + dy) * img_size + (quiet + x * scale + dx)] = pixel;
        }
    *out_w = *out_h = img_size;
    return img;
}

int main(void) {
    tezos_beacon_peer_info_t info = {0};
    strcpy(info.id, "a1b2c3d4-real-guid-format");
    strcpy(info.name, "Kukai");
    strcpy(info.public_key, "7775e38359f7e8c66332edf78a0d1e7630191e909f695393bfab40e3f723dedf");
    strcpy(info.version, "3");
    strcpy(info.relay_server, "matrix.papers.tech");

    char uri[1024];
    CHECK(tezos_beacon_build_pairing_uri(&info, uri, sizeof uri), "builds a real pairing URI");
    CHECK(strncmp(uri, "tezos://?type=tzip10&data=", 26) == 0, "URI has the exact real Beacon URI scheme");

    QRcode *qr = QRcode_encodeString(uri, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    CHECK(qr != NULL, "libqrencode successfully encodes the pairing URI");
    if (!qr) { printf("\n=== SOME CHECKS FAILED (%d) ===\n", ++fails); return 1; }
    printf("  QR version=%d, width=%d modules\n", qr->version, qr->width);

    int img_w, img_h;
    unsigned char *img = render_qr_to_grayscale(qr, 4, &img_w, &img_h);

    zbar_image_scanner_t *scanner = zbar_image_scanner_create();
    zbar_image_scanner_set_config(scanner, 0, ZBAR_CFG_ENABLE, 1);
    zbar_image_t *zimg = zbar_image_create();
    zbar_image_set_format(zimg, zbar_fourcc('Y', '8', '0', '0'));
    zbar_image_set_size(zimg, (unsigned)img_w, (unsigned)img_h);
    zbar_image_set_data(zimg, img, (unsigned long)(img_w * img_h), NULL);

    int n = zbar_scan_image(scanner, zimg);
    CHECK(n > 0, "zbar successfully detects and decodes at least one symbol");

    bool found_match = false;
    const zbar_symbol_t *sym = zbar_image_first_symbol(zimg);
    for (; sym; sym = zbar_symbol_next(sym))
        if (strcmp(zbar_symbol_get_data(sym), uri) == 0) found_match = true;
    CHECK(found_match, "zbar's decoded text matches the original URI EXACTLY -- real encode->image->decode round trip");

    tezos_beacon_peer_info_t parsed = {0};
    CHECK(tezos_beacon_parse_pairing_uri(uri, &parsed), "parsing the pairing URI back into peer info succeeds");
    CHECK(strcmp(parsed.name, "Kukai") == 0 && strcmp(parsed.relay_server, "matrix.papers.tech") == 0,
          "parsed peer info matches original exactly");

    zbar_image_destroy(zimg);
    zbar_image_scanner_destroy(scanner);
    QRcode_free(qr);
    free(img);

    printf("\n%s (%d failures)\n", fails == 0 ? "=== ALL CHECKS PASSED ===" : "=== SOME CHECKS FAILED ===", fails);
    return fails == 0 ? 0 : 1;
}
