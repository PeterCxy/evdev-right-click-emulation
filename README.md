# evdev-right-click-emulation
## Planet Computers - Cosmo communicator [Linux V3]
---

*WIP:* Add support for right click on long press for the Cosmo Communicator Frankeinstein of a SmartLaptop by Planet Computers.

Current state:
- Working mostly
- Cannot right click twice, you must click somewhere else before long press again
- Cannot right click on taskbar

## Build
```
sudo apt install build-essential libevdev-dev git -y
git clone git@github.com:m600x/evdev-right-click-emulation.git
cd evdev-right-click-emulation
make all
```

## Run
```
out/evdev-rce
```

## Install as service
```
sudo cp out/evdev-rce /usr/local/bin/evdev-rce
sudo vi /etc/systemd/system/rightclick.service
```

Content of `/etc/systemd/system/rightclick.service`:
```
[Service]
Type=oneshot
RemainAfterExit=true
StandardOutput=journal
ExecStart=/usr/local/bin/evdev-rce
[Install]
WantedBy=default.target
```
Reboot using `sudo reboot now`

---

Original readme below

---

A program that solves the problem of right clicks on Linux devices with a touchscreen. It implements the long-press-to-right-click gesture on Linux touchscreen devices while having no specific requirements on the desktop environment, display server or the distribution, which greatly improves the touch experience for users of Linux distributions.

It directly reads from `evdev` touchscreen input devices, parses the events, and triggers a right click with `uinput` when it detects a long press from a touchscreen. Since `evdev` is lower-level than `libinput` or any other user-space input driver, this program can work regardless of your choice of Xorg or Wayland.

This program started as a Python script that I published on [Gist](https://gist.github.com/PeterCxy/b4e256b6b4a133c93c012b9738c557ca). However, the Python binding of `libevdev` had some mysterious bugs, and I thought it was not the best idea to run a Python interpreter to read every input event just to implement a simple feature. Thus I rewrote it in C, which is what is contained in this repository.

Dependencies
---

- libevdev
- libevdev headers (for development)

And of course `glibc` and your Linux kernel.

Building
---

```bash
make all
```

The compiled program lies in `out/evdev-rce`.

Usage
---

Simply run

```bash
out/evdev-rce
```

Or you may put this executable anywhere on your file system.

Note: this program needs to read from and write to `/dev/input`. Make sure the current user has the permission to do so, or you may use `root`.

There are some tunable options accessible through the following environment variables:

```bash
# Required interval in ms for a touch event to be considered a long press
# default = 1000ms
LONG_CLICK_INTERVAL=1000
# Allowed margin of error (in px) for a long press event
# i.e. the finger is considered still if its movement is within this range
# Note: the px here is not physical pixels on the screen. It's the physical
# pixels of the touch devices, which may have a different resolution.
# default = 100px
LONG_CLICK_FUZZ=100
# Uncomment to set a blacklist of devices that `evdev-rce` will not
# listen on for touch events. The names can be retrived by using `xinput`
# or simply by reading the output of `evdev-rce`
# (it will print out the names of all touch devices when starting,
#  e.g. "Found touch screen at <some_path>: <device_name>)
#TOUCH_DEVICE_BLACKLIST="device1 name|device2 name|..."
# Uncomment to set a whitelist of devices that `evdev-rce` will ONLY
# listen on. This overrides the blacklist - when whitelist is present,
# any device not in this list will be ignored.
#TOUCH_DEVICE_WHITELIST="device1 name|device2 name|..."
```

So you can run the program like

```
LONG_CLICK_INTERVAL=500 LONG_CLICK_FUZZ=50 evdev-rce
```

To customize those options.
