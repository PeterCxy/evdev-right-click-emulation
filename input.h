#ifndef INPUT_H
#define INPUT_H
#include <libevdev/libevdev.h>

void process_evdev_input(int num, struct libevdev **devices);
#endif