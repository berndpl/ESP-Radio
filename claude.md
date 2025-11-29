I am using a ESP32S3 
MAX98357A as amplifier

It's a web radio that plays by default
it connects to a Bluetooth device. to allow changing volume

"j" keypress for volume up "g" for volume

bluetooth infos about the device

8Bitdo Zero GamePad:
  Address:	E4:17:D8:01:03:11
  Vendor ID:	0x05A0
  Product ID:	0x3232
  Firmware Version:	1.0.9
  Minor Type:	Keyboard
  RSSI:	-63
  Services:	0x800020 < HID ACL >

Show debug message about the bluetooth connection state.
If the dial isn't connected try to connect automatically

try to compile the project to check for errors after all significatnt changes