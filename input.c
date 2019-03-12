#include "input.h"
#include "uinput.h"
#include "rce.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/timerfd.h>
#include <unistd.h>

struct input_state_t {
    int pressed_device_id;
    int pos_x, pos_y;
    int pressed_pos_x, pressed_pos_y;
    int fd_timer; // The timer used to execute right click on timeout
    struct libevdev_uinput *uinput;
};

void free_evdev(struct libevdev *evdev) {
    int fd = libevdev_get_fd(evdev);
    libevdev_free(evdev);
    close(fd);
}

// Build the fd set used by `select`
// return: nfds to be used in select()
int build_fd_set(fd_set *fds, int fd_timer,
        int num, struct libevdev **evdev) {
    FD_ZERO(fds);
    FD_SET(fd_timer, fds);
    int fd = -1, max_fd = fd_timer;
    for (unsigned int i = 0; i < num; i++) {
        fd = libevdev_get_fd(evdev[i]);
        FD_SET(fd, fds);
        if (fd > max_fd)
            max_fd = fd;
    }
    return max_fd + 1;
}

void arm_delayed_rclick(struct input_state_t *state, int dev_id) {
    // Record the position where the touch event began
    state->pressed_pos_x = state->pos_x;
    state->pressed_pos_y = state->pos_y;
    state->pressed_device_id = dev_id;
    // Start the timer; once timeout reached,
    // fire the right click event
    timerfd_settime(state->fd_timer, 0, &(struct itimerspec) {
        .it_value = LONG_CLICK_INTERVAL,
    }, NULL);
}

void unarm_delayed_rclick(struct input_state_t *state) {
    state->pressed_device_id = -1;
    // Force cancel the timer if finger released
    timerfd_settime(state->fd_timer, 0, &(struct itimerspec) {
        .it_value = {
            .tv_sec = 0,
            .tv_nsec = 0,
        },
    }, NULL);
}

void on_input_event(struct input_state_t *state,
        struct input_event *ev, int dev_id) {
    // If we are during a long-press event,
    // do not interruput with another device
    if (state->pressed_device_id != -1 && dev_id != state->pressed_device_id)
        return;

    if (ev->type == EV_ABS) {
        // Absolute position event
        // Record the current position
        if (ev->code == ABS_X || ev->code == ABS_MT_POSITION_X) {
            state->pos_x = ev->value;
        } else if (ev->code == ABS_Y || ev->code == ABS_MT_POSITION_Y) {
            state->pos_y = ev->value;
        }
    } else if (ev->type == EV_KEY && ev->code == BTN_TOUCH) {
        // Touch event
        if (ev->value == 1) {
            // Schedule a delayed right click event
            // so that if anything happens before the long-press timeout,
            // it can be canceled
            arm_delayed_rclick(state, dev_id);
        } else {
            // Finger released. It is no longer considered a long-press
            unarm_delayed_rclick(state);
        }
    }
}

void on_timer_expire(struct input_state_t *state) {
    unsigned int dx = abs(state->pos_x - state->pressed_pos_x);
    unsigned int dy = abs(state->pos_y - state->pressed_pos_y);
    // Only consider movement within a range to be "still"
    // i.e. if movement is within this value during timeout
    //      , then it is a long click
    if (dx <= LONG_CLICK_FUZZ && dy <= LONG_CLICK_FUZZ)
        uinput_send_right_click(state->uinput);
    // In Linux implementation of timerfd, the fd becomes always "readable"
    // after the timeout. So we have to unarm it after we receive the event.
    unarm_delayed_rclick(state);
}

void process_evdev_input(int num, struct libevdev **evdev) {
    // Initialize everything to -1
    struct input_state_t state = {
        .pressed_device_id = -1,
        .pos_x = -1, .pos_y = -1,
        .pressed_pos_x = -1, .pressed_pos_y = -1,
        .fd_timer = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK),
        .uinput = uinput_initialize(),
    };

    // Check if uinput device was created
    if (state.uinput == NULL) {
        fprintf(stderr, "Failed to create uinput device");
        exit(-1);
        return;
    }

    // Initialize the set of fds
    fd_set fds;
    int nfds = build_fd_set(&fds, state.fd_timer, num, evdev);

    struct input_event ev;
    // Detect events from the fds
    while (select(nfds, &fds, NULL, NULL, NULL) > 0) {
        // The timer fd
        if (FD_ISSET(state.fd_timer, &fds))
            on_timer_expire(&state);

        for (unsigned int i = 0; i < num; i++) {
            // Can we read an event?
            if (!FD_ISSET(libevdev_get_fd(evdev[i]), &fds))
                continue;
            
            // Read all the available events
            while (libevdev_next_event(evdev[i],
                    LIBEVDEV_READ_FLAG_NORMAL, &ev) == 0) {
                on_input_event(&state, &ev, i);
            }
        }

        // Reset the fd_set for next iteration
        nfds = build_fd_set(&fds, state.fd_timer, num, evdev);
    }

    // Cleanup
    libevdev_uinput_destroy(state.uinput);
    for (int i = 0; i < num; i++) {
        free_evdev(evdev[i]);
    }
}