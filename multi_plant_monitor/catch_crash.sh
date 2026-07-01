#!/bin/bash
while true; do
  if [ -c /dev/cu.usbserial-5B1F0101911 ]; then
    stty -f /dev/cu.usbserial-5B1F0101911 115200 raw -clocal -echo
    cat /dev/cu.usbserial-5B1F0101911 >> serial_crash.log
  fi
  sleep 0.1
done
