# autosnake

Self-playing Snake for an **Arduino Uno + 2.4" TFT LCD shield** (MCUFRIEND style).
No buttons, no menus: it starts playing instantly, always wins, shows
`YOU WIN!` for 5 seconds, then restarts automatically.

## Hardware

- Arduino Uno (or compatible)
- 2.4" TFT LCD shield (ILI9341 and friends) — just plug it onto the Uno,
  no extra wiring.

## Libraries

Install via Arduino IDE → Sketch → Include Library → Manage Libraries:

- `Adafruit GFX Library` (Adafruit)
- `MCUFRIEND_kbv` (David Prentice)

Then pick Board `Arduino Uno`, the right port, and Upload.

## Flash from the command line

`arduino-cli` is already installed at `~/.local/bin/arduino-cli`, with the
`arduino:avr` core and all required libraries. Plug in the Uno and run:

```sh
cd /home/miha/dev/qardit/autosnake

# 1. Find the port (the Uno; often /dev/ttyUSB0 for clones, /dev/ttyACM0 for genuine)
arduino-cli board list

# 2. Compile
arduino-cli compile --fqbn arduino:avr:uno .

# 3. Upload (use the port from step 1)
arduino-cli upload --fqbn arduino:avr:uno --port /dev/ttyUSB0 .
```

If you ever set this up on a fresh machine:

```sh
arduino-cli core install arduino:avr
arduino-cli lib install "Adafruit GFX Library" MCUFRIEND_kbv "Adafruit BusIO"
```

Notes:

- `board list` may show the Uno as `Unknown` (normal for CH340 clones) —
  just use the `/dev/ttyUSB*` / `/dev/ttyACM*` port that appears when you
  plug it in.
- The game starts by itself on power-up; there is nothing to press and no
  serial output to watch (it only prints if the AI ever died, which it can't).

## How it always wins

The 16×12 grid is covered by a fixed **Hamiltonian cycle**: one loop that
visits every cell exactly once per lap (`buildCycle()` in
[`snake_core.h`](/home/miha/dev/qardit/autosnake/snake_core.h)).

The head advances exactly one cycle cell per tick, so the body is always
one contiguous cycle segment and the next cell is always free — the snake
mathematically cannot collide. Food spawns on a free cell, which is always
ahead of the head, so it is always reached within one lap. The snake grows
until it fills the whole 192-cell board, the game shows `YOU WIN!` for
5 seconds, and restarts.

Tweak: game speed is the `stepMs` map at the top of `loop()` in
[`autosnake.ino`](/home/miha/dev/qardit/autosnake/autosnake.ino) — it sprints
while food is far away and slows down for each catch (~2 min per game).
Speed never affects the AI, which cannot die either way.
