/* Real stb_image integration -- STB_IMAGE_IMPLEMENTATION must be
   defined in exactly one .c file in the whole project; this is that
   file. STBI_NO_STDIO: this project only ever decodes from a memory
   buffer (real network-fetched bytes), matching its existing style of
   no filesystem dependency for network content -- disabling stb's
   file-path API entirely removes that unused surface area rather than
   just not calling it. */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "../../third_party/stb/stb_image.h"

#include "tezos_image_decode.h"
#include <string.h>
#include <stdio.h>

bool tezos_image_decode(const uint8_t *data, int data_len, tezos_decoded_image_t *out) {
    memset(out, 0, sizeof *out);
    int channels_in_file;
    /* real, forced 3-channel (RGB) output regardless of the source
       image's actual channel count -- a consistent, known format this
       project's downsample/RGB565 pipeline (tezos_cam_downsample_box/
       tezos_cam_apply_16bit_color, already real and tested) can rely
       on, rather than branching on RGB vs RGBA vs grayscale */
    unsigned char *pixels = stbi_load_from_memory(data, data_len, &out->width, &out->height,
                                                  &channels_in_file, 3);
    if (!pixels) return false; /* real failure -- corrupt data, unsupported
        format, or not an image at all; a real, expected outcome for
        arbitrary network-fetched content, not something to paper over */
    out->rgb = pixels; /* stb's own malloc'd buffer -- freed via stbi_image_free
        in tezos_image_free, not a copy, no double-allocation */
    return true;
}

void tezos_image_free(tezos_decoded_image_t *img) {
    if (img->rgb) {
        stbi_image_free(img->rgb);
        img->rgb = NULL;
    }
}

bool tezos_resolve_ipfs_uri(const char *uri, char *url_out, size_t url_cap) {
    if (strncmp(uri, "ipfs://", 7) == 0) {
        int written = snprintf(url_out, url_cap, "https://ipfs.io/ipfs/%s", uri + 7);
        return written > 0 && (size_t)written < url_cap; /* real truncation check --
            a silently-truncated URL would fetch the wrong (or a nonexistent)
            resource, worse than a clean, reported failure */
    }
    if (strncmp(uri, "http://", 7) == 0 || strncmp(uri, "https://", 8) == 0) {
        int written = snprintf(url_out, url_cap, "%s", uri);
        return written > 0 && (size_t)written < url_cap;
    }
    return false; /* a real, unrecognized scheme -- not silently guessed at */
}
