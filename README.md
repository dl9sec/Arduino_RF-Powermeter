# Arduino RF-Powermeter for Mini-Circuits ZX47-55 frontend
A "quick and dirty" implementation of a RF-Powermeter with a Mini-Circuits ZX47-55 frontend.

RF-Powermeter with for a Mini-Circuits ZX47-55-S+ frontend, 128x64 OLED display and pushbuttons.
Using Arduino Uno (5V/16MHz) and Waveshare 0.96" 128x64 OLED SPI/I2C with SSD1306.

Copyright (C) 2019 by Thorsten Godau (dl9sec) and licensed under
GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

![alt RF-Powermeter display](https://github.com/dl9sec/Arduino_RF-Powermeter/raw/master/images/Arduino_RF-Powermeter_1.png)

![alt RF-Powermeter display](https://github.com/dl9sec/Arduino_RF-Powermeter/raw/master/images/Arduino_RF-Powermeter_2.png)

# Connections

Arduino             Signal
-------             ------

(4) --------------- Upper  pushbutton to GND
(3) --------------- Middle pushbutton to GND
(2) --------------- Lower  pushbutton to GND

(A0) -------------- ZX47 TEMP
(A1) -------------- ZX47 DC OUT
(5V) -------------- ZX47 +5V
(GND) ------------- ZX47 GND

(5V) -------------- OLED VCC
(GND) ------------- OLED GND
(11) -------------- OLED DIN
(13) -------------- OLED CLK
(10) -------------- OLED CS
(9) --------------- OLED D/C
(8) --------------- OLED RES

(AREF) --- 10k ---- 3V3

# Usage

The ZX47 linear measurement range at about 2,4GHz is between -55dBm and +5dBm.
For more than +5dBm input power, an suitable attenuator must be connected ahead.
The ATT value is just added to the measured power value.
The value of the ATT can be adjusted with upper/middle pushbutton:

- Press upper button  -> Increase ATT
- Press middle button -> Decrease ATT

The ATT range is 0..49dB, so RF power up to +54dBm/251W can be displayed.

Pressing all three pushbuttons at the same time will reset the device
and displays the splash screen.

Remove comment at "#define DEBUGON" to get debug information as measured voltages and
pushbutton states.