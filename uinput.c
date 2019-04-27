#include "uinput.h"
#include <stddef.h>

struct libevdev_uinput *uinput_initialize() {
    // Create a evdev first to describe the features
    struct libevdev *evdev = libevdev_new();
    libevdev_set_name(evdev, "Simulated Right Button");
    libevdev_enable_event_type(evdev, EV_KEY);
    libevdev_enable_event_code(evdev, EV_KEY, BTN_RIGHT, NULL);
    libevdev_set_name(evdev, "Simulated Left Button");
    libevdev_enable_event_type(evdev, EV_KEY);
    libevdev_enable_event_code(evdev, EV_KEY, BTN_LEFT, NULL);
    // Initialize uinput device from the evdev descriptor
    struct libevdev_uinput *uinput = NULL;
    if (libevdev_uinput_create_from_device(evdev,
            LIBEVDEV_UINPUT_OPEN_MANAGED, &uinput) != 0) {
        uinput = NULL;
    }
    // We don't need the fake evdev anymore.
    libevdev_free(evdev);
    return uinput;
}

void uinput_send_right_click(struct libevdev_uinput *uinput) {
    libevdev_uinput_write_event(uinput, EV_KEY, BTN_RIGHT, 1);
    libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
    libevdev_uinput_write_event(uinput, EV_KEY, BTN_RIGHT, 0);
    libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
}

void uinput_send_left_click(struct libevdev_uinput *uinput) {
    libevdev_uinput_write_event(uinput, EV_KEY, BTN_LEFT, 1);
    libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
    libevdev_uinput_write_event(uinput, EV_KEY, BTN_LEFT, 0);
    libevdev_uinput_write_event(uinput, EV_SYN, SYN_REPORT, 0);
}
