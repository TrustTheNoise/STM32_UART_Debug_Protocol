# todo implement and document

import serial

import hople_com_dbg_protocol as cdp

# Import definitions for console coloring
from helper_scripts import console_colors
bcolors = console_colors.bcolors

serial_port:serial.Serial = None


import csv
import time
import os.path

def main():
    try:
        global serial_port
        serial_port = serial.Serial(
            port = "COM18", #! Change comport here 
            baudrate = 500000, #! Change baud rate here
            bytesize = serial.EIGHTBITS,
            parity = serial.PARITY_NONE,
            stopbits = serial.STOPBITS_ONE,
            timeout = 0.1,
        )
        print(f'Connected to port {bcolors.OKBLUE}{serial_port.port}{bcolors.ENDC}')
    except:
        print(f'{bcolors.FAIL}Failed to open serial port!{bcolors.ENDC}')
        serial_port = None
        exit()

    # make sure that connection is established before doing anything else
    device_connected = cdp.establish_connection(serial_port)
    if(device_connected == False):
        print("Connection failed!")
        return

    cdp.save_streaming_data(serial_port, 0)

    cdp.close_connection(serial_port)


if __name__ == "__main__":
    main()
        