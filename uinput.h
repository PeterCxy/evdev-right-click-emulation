#ifndef UINPUT_H
#define UINPUT_H
#include <libevdev/libevdev-uinput.h>

struct libevdev_uinput *uinput_initialize();
void uinput_send_right_click(struct libevdev_uinput *uinput);
#endif