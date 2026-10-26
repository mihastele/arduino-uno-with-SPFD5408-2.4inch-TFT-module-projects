    sudo usermod -aG dialout $USER
    arduino-cli upload --fqbn arduino:avr:uno --port /dev/ttyUSB0 hacker_box

