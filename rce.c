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

// Blacklist of touch devices that this program
// should NOT listen on.
char *TOUCH_DEVICE_BLACKLIST = NULL;

// Whitelist of touch devices that, if present,
// we should ONLY listen on
char *TOUCH_DEVICE_WHITELIST = NULL;

// Determine if a device name is contained
// in a "|"-separated device list
int list_contains(const char *list, const char* name) {
    char *fname = malloc(strlen(name) + 3);
    sprintf(fname, "|%s|", name);
    int ret = list != NULL
        && strstr(list, fname) != NULL;
    free(fname);
    return ret;
}

int find_evdev(struct libevdev **devices) {
    DIR *dev_input_fd;
    if ((dev_input_fd = opendir(DIR_DEV_INPUT)) == NULL) {
        fprintf(stderr, "Unable to open %s\n", DIR_DEV_INPUT);
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
                && !libevdev_has_event_code(evdev, EV_KEY, BTN_RIGHT)) {
            const char *name = libevdev_get_name(evdev);
            printf("Found touch screen at %s: %s\n", dev_path, name);
            if (TOUCH_DEVICE_WHITELIST == NULL) {
                // Blacklist mode
                if (list_contains(TOUCH_DEVICE_BLACKLIST, name)) {
                    printf("Device \"%s\" is blacklisted. skipping.\n", name);
                    // Skip this device. Just jump to the clean-up section
                    // to continue to the next device.
                    goto continue_loop;
                }
            } else {
                // Whitelist mode
                if (!list_contains(TOUCH_DEVICE_WHITELIST, name)) {
                    printf("Device \"%s\" is not whitelisted. skipping.\n", name);
                    goto continue_loop;
                }
            }
            devices[device_num] = evdev;
            device_num++;
            if (device_num == MAX_TOUCHSCREEN_NUM) {
                printf("More than %d touchscreens. Only keeping the first %d.\n",
                    MAX_TOUCHSCREEN_NUM, MAX_TOUCHSCREEN_NUM);
                break;
            }
            // Skip the clean-up because we need this device to be open
            continue;
        }

    continue_loop:
        // Some clean-up work to do before trying the next device
        libevdev_free(evdev);
        close(dev_fd);
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

    if ((env = getenv("TOUCH_DEVICE_BLACKLIST")) != NULL) {
        TOUCH_DEVICE_BLACKLIST = malloc(strlen(env) + 3);
        sprintf(TOUCH_DEVICE_BLACKLIST, "|%s|", env);
    }

    if ((env = getenv("TOUCH_DEVICE_WHITELIST")) != NULL) {
        printf("Note: Whitelist mode is enabled. This overrides the blacklist.\n");
        TOUCH_DEVICE_WHITELIST = malloc(strlen(env) + 3);
        sprintf(TOUCH_DEVICE_WHITELIST, "|%s|", env);
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
    free(TOUCH_DEVICE_BLACKLIST);
    free(TOUCH_DEVICE_WHITELIST);
    return 0;
}
