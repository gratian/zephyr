USB Desk Analog Gauges
########################################

Overview
********

This project creates a USB composite device which exposes:

  * a CDC_ACM serial console (e.g. /dev/ttyACM0)
  * a HID raw device that can receive values to display on analog
    gagues driven by "DAC" PWM channels

Running
*******

  west build -p always -b adafruit_trinket_m0 projects/desk_gauges_client
  west flash

Host side communication example:
  cd ~/projects/desk_gauges/hid
  sudo ./hid 50 50 50 50
