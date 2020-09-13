#!/usr/bin/env python3

# Minimal python3 exequtable for indi_arduino_cap
# First parameter is USB COM path usually /dev/ttyUSB0 on ubuntu
# Only task is to set digital (servo pwm) pin X to rotation degrees Y (0,180)
# This will be done interpolated from current position Z (0,180)
# Like ./arduino_servo_pyfirmata.py COM X Y Z
# If Z is equal to Y servo will be set to rotation Y without interpolation

import sys
from time import sleep
from pyfirmata import Arduino
from pyfirmata import SERVO

fail = 0

if len(sys.argv) != 5:
    fail = 1
elif not str.isdigit(sys.argv[2]):
    fail = 2
elif not str.isdigit(sys.argv[3]):
    fail = 3
elif not str.isdigit(sys.argv[4]):
    fail = 4

if fail != 0:
    print("\n###############\nThe", sys.argv[0], "script needs parameters\n* USB COM path, like /dev/ttyUSB0 on ubuntu\n* servo pin int\n* desired servo rotation int\n* followed by current servorotation int\n\n###############\n", sys.argv[0], "/dev/ttyUSB0 3 60 120\n* moves arduino on /dev/ttyUSB0\n* servo number 3\n* from 120 degrees\n* to 60 degrees rotation\n* in 1 degree increments\n\n###############\n", sys.argv[0], "/dev/ttyUSB0 3 90 90\n* moves arduino on /dev/ttyUSB0\n* servo number 3\n* to 90 degrees\n* as fast as the servo moves\n")
    exit(fail)

port = sys.argv[1]
pin = int(sys.argv[2])
angle = int(sys.argv[3])
position = int(sys.argv[4])

board = Arduino(port)
sleep(1)

board.digital[pin].mode = SERVO

if angle == position:
    board.digital[pin].write(angle)
    sleep(1)
    exit(0)

if position < angle:
    for i in range(position, angle):
        board.digital[pin].write(i)
        sleep(0.02)
else:
    for i in range(position, angle + 1, -1):
        board.digital[pin].write(i)
        sleep(0.02)

board.exit()
exit(0)
