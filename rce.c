#include <libevdev/libevdev.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "rce.h"
#include "input.h"

#define DIR_DEV_INPUT "/dev/input"
#define EVDEV_PREFIX "event"

struct timespec LONG_CLICK_INTERVAL = {
    .tv_sec = 1,
    .tv_nsec = 0,
};

// Allowed "fuzziness" of a long-press action
// Finger is considered still if the movement is within this value
int LONG_CLICK_FUZZ = 100;

int find_evdev(struct libevdev **devices) {
    DIR *dev_input_fd;
    if ((dev_input_fd = opendir(DIR_DEV_INPUT)) == NULL) {
        fprintf(stderr, "Unable to open %s", DIR_DEV_INPUT);
        return -1;
    }

    char dev_path[32];
    int dev_fd = -1;
    int rc = -1;
    struct libevdev *evdev = NULL;
    struct dirent *dev_input_entry = NULL;
    int device_num = 0;
    // Loop over /dev/input to find touchscreen
    // There can be multiple touchscreen devices.
    // e.g. a touch screen, two stylus for that screen (like on Surface)
    // TODO: Support user-defined blacklist (or whitelist)
    while ((dev_input_entry = readdir(dev_input_fd)) != NULL) {
        if (strncmp(EVDEV_PREFIX,
                dev_input_entry->d_name, strlen(EVDEV_PREFIX)) != 0) {
            // Not a /dev/input/event*
            continue;
        }
        sprintf(dev_path, "%s/%.20s", DIR_DEV_INPUT, dev_input_entry->d_name);
        dev_fd = open(dev_path, O_RDONLY | O_NONBLOCK);
        if (dev_fd < 0) {
            fprintf(stderr, "Open %s failed: %s\n", dev_path, strerror(-dev_fd));
            continue;
        }
        rc = libevdev_new_from_fd(dev_fd, &evdev);
        if (rc < 0) {
            fprintf(stderr, "Open %s failed: %s\n", dev_path, strerror(-rc));
            continue;
        }

        // A touchscreen is absolute input, with TOUCH events
        if (libevdev_has_event_type(evdev, EV_ABS)
                && !libevdev_has_event_type(evdev, EV_REL)
                && libevdev_has_event_code(evdev, EV_KEY, BTN_TOUCH)
                && !libevdev_has_event_code(evdev, EV_KEY, BTN_RIGHT)) {
            const char *name = libevdev_get_name(evdev);
            printf("Found touch screen at %s: %s\n", dev_path, name);
            devices[device_num] = evdev;
            device_num++;
            if (device_num == MAX_TOUCHSCREEN_NUM) {
                printf("More than %d touchscreens. Only keeping the first %d.",
                    MAX_TOUCHSCREEN_NUM, MAX_TOUCHSCREEN_NUM);
                break;
            }
        } else {
            libevdev_free(evdev);
            close(dev_fd);
        }
    }

    closedir(dev_input_fd);
    return device_num;
}

int main() {
    // Try to read some configurable options from env
    char *env = NULL;
    if ((env = getenv("LONG_CLICK_INTERVAL")) != NULL) {
        int ms = atoi(env);
        int sec = 0;
        if (ms >= 1000) {
            sec = ms / 1000;
            ms = ms % 1000;
        }
        LONG_CLICK_INTERVAL.tv_sec = sec;
        LONG_CLICK_INTERVAL.tv_nsec = ((long) ms) * 1000 * 1000;
    }

    if ((env = getenv("LONG_CLICK_FUZZ")) != NULL) {
        LONG_CLICK_FUZZ = atoi(env);
    }

    struct libevdev *devices[MAX_TOUCHSCREEN_NUM];
    int device_num;
    if ((device_num = find_evdev(devices)) < 0) {
        return 1;
    }
    if (device_num == 0) {
        fprintf(stderr, "No touchscreen is found\n");
        return 1;
    }

    process_evdev_input(device_num, devices);
    return 0;
}