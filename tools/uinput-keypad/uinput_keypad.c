/* Minimal synthetic keyboard via /dev/uinput -- creates a virtual input
 * device registering exactly the keys linux_shim.c's translate_key()
 * understands, then reads key codes (one per line) from stdin and sends
 * a press+release for each. Used to exercise real navigation on real
 * hardware without a keypad wired yet. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/uinput.h>

static void emit(int fd, int type, int code, int val) {
    struct input_event ie = {0};
    ie.type = type; ie.code = code; ie.value = val;
    write(fd, &ie, sizeof(ie));
}

int main(void) {
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) { perror("open /dev/uinput"); return 1; }

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    int keys[] = {KEY_UP, KEY_DOWN, KEY_ENTER, KEY_ESC, KEY_KPASTERISK, KEY_BACKSLASH,
                  KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9};
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++)
        ioctl(fd, UI_SET_KEYBIT, keys[i]);

    struct uinput_setup usetup = {0};
    usetup.id.bustype = BUS_USB;
    usetup.id.vendor = 0x1234;
    usetup.id.product = 0x5678;
    strcpy(usetup.name, "tca8418-keypad-sim");
    ioctl(fd, UI_DEV_SETUP, &usetup);
    ioctl(fd, UI_DEV_CREATE);

    fprintf(stderr, "device created, settling...\n");
    sleep(1);
    fprintf(stderr, "ready, waiting for key codes on stdin\n");

    char line[64];
    while (fgets(line, sizeof(line), stdin)) {
        int code = atoi(line);
        if (code <= 0) continue;
        fprintf(stderr, "sending key %d\n", code);
        emit(fd, EV_KEY, code, 1);
        emit(fd, EV_SYN, SYN_REPORT, 0);
        usleep(50000);
        emit(fd, EV_KEY, code, 0);
        emit(fd, EV_SYN, SYN_REPORT, 0);
        usleep(50000);
    }

    ioctl(fd, UI_DEV_DESTROY);
    close(fd);
    return 0;
}
