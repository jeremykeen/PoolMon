# uses the curses library to make a terminal screen that allows
# the user to communicate with Atlas Scientific boards

import curses, curses.ascii # needed to allow seperate terminal displays
import serial # required for communication with boards
import time
import os
import glob
import subprocess

from time import strftime # used for timestamps

os.system('modprobe w1-gpio')
os.system('modprobe w1-therm')

base_dir = '/sys/bus/w1/devices/'
device_folder = glob.glob(base_dir + '28*')[0]
device_file = device_folder + '/w1_slave'

# Import Adafruit IO REST client.
from Adafruit_IO import Client

# Set to your Adafruit IO key.
ADAFRUIT_IO_KEY = 'f23a7ccc954b857af7c88a3f1229f72b0dcd9c58'

# Create an instance of the REST client.
aio = Client(ADAFRUIT_IO_KEY)

def read_temp_raw():
	catdata = subprocess.Popen(['cat',device_file], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	out,err = catdata.communicate()
	out_decode = out.decode('utf-8')
	lines = out_decode.split('\n')
	return lines

def read_temp():
    lines = read_temp_raw()
    while lines[0].strip()[-3:] != 'YES':
        time.sleep(0.2)
        lines = read_temp_raw()
    equals_pos = lines[1].find('t=')
    if equals_pos != -1:
        temp_string = lines[1][equals_pos+2:]
        temp_c = float(temp_string) / 1000.0
        temp_f = temp_c * 9.0 / 5.0 + 32.0
        return temp_f
def main():

	#USB parameters
	usbport = '/dev/ttyAMA0'
	ser = serial.Serial(usbport, 9600, timeout = 0) # sets the serial port to the specified port, with a 9600 baud rate
	# Timeout = 0 tells the serial port to not wait for input if there is non

	#temp compensation
	ser.write("T,15.0\r")

	while True:

		data = ser.read() # get serial data
		#if(data == "\r"): # if its terminated by a newline

		print("sensor data: " + data + "\n")
		#aio.send('pH', data)
		#else:
		#	line = line + data
