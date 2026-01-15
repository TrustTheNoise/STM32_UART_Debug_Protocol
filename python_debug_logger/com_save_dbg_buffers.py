import serial

import hople_com_dbg_protocol as cdp

# Import definitions for console coloring
from helper_scripts import console_colors
bcolors = console_colors.bcolors

serial_port:serial.Serial = None


def main():
    """Saves all registered debug buffers into a .csv file inside logs/ folder.
    
    Prerequisites:
     1. logs/ folder must exist inside the folder with this .py file;
     2. MCU with support for com debug protocol must be connected to the target PC with USB-UART converter;
     3. MCU must be powered up;
     4. COM port inside the main function must be changed to correspond to USB-UART converter;
     5. Baud rate must be selected to be the same as inside the connected MCU;
     6. At least one debug buffer should be registered inside the MCU firmware. There will not be an error without it, but nothing
        will be saved if no buffer is registered inside the firmware
     TODO maybe move COM port and baud rate into global constants ?

    How to use:
     Simply run this python file when all prerequisites are fulfilled. If everything is ok, log will be saved inside the logs/ folder
     with name buffers_log_*numbers*.csv. Numbers are taking from current epoch time value, and therefore files can be easily sorted and 
     all logs will always have unique names. If there are any issues during saving, corresponding errors will be written to console.
     if you cannot handle issues yourself, ask Andrei Dounar for help.
    """
    try:
        global serial_port
        serial_port = serial.Serial(
            port = "COM18", #! Change comport here 
            baudrate = 500000, #! Change baud rate here
            bytesize = serial.EIGHTBITS,
            parity = serial.PARITY_NONE,
            stopbits = serial.STOPBITS_ONE,
            timeout = 0.5,
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

    cdp.save_all_buffers(serial_port)

    cdp.close_connection(serial_port)

if __name__ == "__main__":
    main()
        