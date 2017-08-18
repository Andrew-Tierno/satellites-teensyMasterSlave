#!/usr/bin/python
import RPi.GPIO as GPIO
import time
import subprocess

program_master = 2
program_slave = 3
reset_master = 4

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup(reset_master,GPIO.OUT)
GPIO.setup(program_master,GPIO.OUT)
GPIO.setup(program_slave,GPIO.OUT)
GPIO.output(reset_master, GPIO.LOW) # No power to FET - antagonist
GPIO.output(program_slave, GPIO.HIGH) # Not program mode
GPIO.output(program_master, GPIO.HIGH) # Not program mode

def cycle(pin):
    GPIO.output(pin, GPIO.HIGH)
    GPIO.output(pin, GPIO.LOW)
    time.sleep(0.5)
    GPIO.output(pin, GPIO.HIGH)
    time.sleep(1)

def showSerial():
    print('-------------')
    print("Serials available:")
    subprocess.call("ls /dev/ttyACM*", shell=True)
    print('-------------')

def program(platformio_base, mcu):
    time.sleep(1)
    command = "cd " + platformio_base + " && platformio run && cd ../ && ./teensy_loader_cli --mcu=TEENSY" + mcu + " " + platformio_base + ".pioenvs/teensy" + mcu + "/firmware.hex"
    command = "./teensy_loader_cli --mcu=TEENSY" + mcu + " " + platformio_base + ".pioenvs/teensy" + mcu + "/firmware.hex"
    print(command)
    subprocess.call(command, shell=True)
    time.sleep(1)
    #subprocess.call("g++ spitest.cpp -std=c++11 -lwiringPi -o spitest", shell=True) # compile master mode


cycle(program_slave)
print("Slave serial:")
subprocess.call("ls /dev/ttyACM*", shell=True)
program("stateMachine/", "36")
GPIO.output(reset_master, GPIO.HIGH)
time.sleep(1)
print("Both serial:")
subprocess.call("ls /dev/ttyACM*", shell=True)
cycle(program_master)
program("antagonist/", "31")
showSerial()
subprocess.call("./serial");
#subprocess.call("g++ master.cpp -std=c++11 -lwiringPi -o master && ./master", shell=True) # compile master mode
