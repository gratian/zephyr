USB Desk NIXIE Clock
####################

Overview
********

This project creates a USB composite device which exposes:

  * a CDC_ACM serial console (e.g. /dev/ttyACM0)
  * a HID raw device that can be used to interact with the NIXIE clock

Running
*******

  west build -p always -b seeeduino_xiao projects/nixie_clock
  west flash

---
@todo re-write host-side ...
Host side communication example:
  cd ~/projects/desk_gauges/hid
  sudo ./hid 50 50 50 50
