# GHOST_SHELL — hacker animation for Arduino UNO + 2.4" TFT

Green-on-black hacker animation. No touch needed.

## What you get

- **Rotating 3D wireframe box**: outer + counter-rotating inner cube with
  linked vertices. Rotation speed, size and screen position drift to new
  random targets every few seconds, with occasional wild spin bursts.
- **Radar sweep**, matrix rain, random hex speckles, glitch slices.
- **Fake terminal**: types random linux commands (`nmap`, `ssh`, `hydra`,
  `tcpdump`, ...) char-by-char at random speed with pauses and bursts,
  prints fake output, random session IDs.
- **Non-deterministic loop**: seeded from floating-analog-pin noise and
  re-stirred periodically, so it never repeats the same way twice.

## Hardware

- Arduino UNO + 2.4" TFT LCD shield (MCUFRIEND style) plugged on top.
- USB cable to PC.

## Libraries (one-time)

```
arduino-cli lib install MCUFRIEND_kbv "Adafruit GFX Library"
# or via Arduino IDE Library Manager: MCUFRIEND_kbv + Adafruit GFX
```

## Upload

```
arduino-cli compile --fqbn arduino:avr:uno hacker_box
arduino-cli upload --fqbn arduino:avr:uno --port /dev/ttyUSB0 hacker_box
```

If the port says "Permission denied", add yourself to `dialout` once
and log out / back in:

```
sudo usermod -aG dialout $USER
```
