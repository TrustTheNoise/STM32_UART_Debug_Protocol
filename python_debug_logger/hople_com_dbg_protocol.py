# This file contains all the functions we need for our debug protocol
# todo: Run through all error states and make sure that all of them work. Maybe write unit tests or something like that

import serial
import time
import struct
import csv
import os.path
import io
import json

from enum import Enum

# Import definitions for console coloring
from helper_scripts import console_colors
bcolors = console_colors.bcolors

################################################################################
# Message definitions

message_prefix = [0xAA, 0x55]
message_ack = bytearray(message_prefix + [0xAA])
message_nack = bytearray(message_prefix + [0x55])

message_establish_connection = bytearray(message_prefix + [0x01])
message_close_connection = bytearray(message_prefix + [0x02])
message_keep_alive = bytearray(message_prefix + [0x03])

message_read_error_log = bytearray(message_prefix + [0x08])
message_read_buffers_properties = bytearray(message_prefix + [0x10])
message_read_debug_buffer = bytearray(message_prefix + [0x00])

message_start_streaming = bytearray(message_prefix + [0x31])
message_stream_message_start = bytearray(message_prefix + [0x32])
message_stop_streaming = bytearray(message_prefix + [0x33])

message_generic_request = bytearray(message_prefix + [0x40])



class BUFFER_TYPE(Enum):
    NO_BUFFER = 0
    F32_BUFFER = 1
    I32_BUFFER = 2
    U32_BUFFER = 3
    I16_BUFFER = 4
    U16_BUFFER = 5
    U8_BUFFER = 6


################################################################################
# Global variables
device_connection_is_established = False
cyccnt_overflow_ticks = (1 << 32) - 1

################################################################################

def establish_connection(serial_port: serial.Serial):
    """Used to communicate to target MCU that PC client wants to establish connection
    
    Must be used as first communication function because without connection device may not respond to other requests.
    
    @TODO Maybe remove the whole concept of connection establishing. The initial idea for it was to show that PC client is attached so that
    The device can initialize communication by itself. But with updated concept all communication is handled with requests from PC
    and therefore we don't need to communicate to device that connection is established. I will leave it as is for now, as I'm not sure how
    things will be updated in the future. But when things will become clearer this approach can be changed
    """
    global device_connection_is_established

    serial_port.write(message_establish_connection)
    device_reply = serial_port.read(3)

    if(len(device_reply) != 3):
        device_connection_is_established = False
        print(f"{bcolors.FAIL}Device is not responding!{bcolors.ENDC} Make sure that connected device supports debug protocol")
        return device_connection_is_established

    if (device_reply == message_ack):
        device_connection_is_established = True
        print(f"Debug connection is established!")
    else:
        device_connection_is_established = False
        print(f"{bcolors.FAIL}Device response is incorrect!{bcolors.ENDC} Make sure that device runs proper version of debug protocol!")
    return device_connection_is_established

########################################

def close_connection(serial_port: serial.Serial):
    """Used to communicate to target MCU that PC client wants to close already established connection
    Must be used as the last communication function in the end of the program to close existing connection, so that MCU 
    will not think that PC still expects data.
    """
    global device_connection_is_established

    if(device_connection_is_established == False):
        return

    serial_port.write(message_close_connection)
    device_reply = serial_port.read(3)
    # we don't need to handle return, even if it is incorrect, we can't do much about it

    device_connection_is_established = False

########################################

def save_all_buffers(serial_port: serial.Serial):
    """Saves all registered debug files from device into a .csv file inside logs/ folder. Doesn't save anything if no buffers are registered inside the MCU

    @NOTE logs/ folder must exist inside the folder with this .py file
    """
    global device_connection_is_established

    print("Saving all debug buffers.")

    if (device_connection_is_established == False):
        print(f"{bcolors.FAIL}Device connection is not established for save_all_buffers!{bcolors.ENDC}")
        return device_connection_is_established

    serial_port.write(message_read_buffers_properties)
    device_reply = serial_port.read(6)

    if(len(device_reply) != 6):
        print(f"{bcolors.FAIL}Wrong response to read buffers properties request!{bcolors.ENDC}. Wrong answer length")
        device_connection_is_established = False
        return device_connection_is_established
    
    number_of_registered_buffers = 0
    buffers_length = 0
    if(device_reply[0] != message_prefix[0] or device_reply[1] != message_prefix[1] or device_reply[2] != message_read_buffers_properties[2]):
        print(f"{bcolors.FAIL}Wrong response to read buffers properties request!{bcolors.ENDC}. Wrong answer content")
        device_connection_is_established = False
        return device_connection_is_established
    
    number_of_registered_buffers = device_reply[3]
    buffers_length = struct.unpack("H", device_reply[4:6])[0]
    print(f"Buffers count: {bcolors.OKBLUE}{number_of_registered_buffers}{bcolors.ENDC}, ", end='')
    print(f"Buffers size: {bcolors.OKBLUE}{buffers_length}{bcolors.ENDC} records")

    if (number_of_registered_buffers == 0):
        print(f"{bcolors.OKBLUE}No active buffers are registered. Logging aborted{bcolors.ENDC}")
        return device_connection_is_established

    logging_data = []
    for i in range(number_of_registered_buffers):
        message_read_debug_buffer[2] = 0x10 + (i + 1)
        serial_port.write(message_read_debug_buffer)
        device_reply = serial_port.read(3)
        if(len(device_reply) != 3):
            print(f"{bcolors.FAIL}Wrong response to buffer read request!{bcolors.ENDC}. Wrong answer length")
            device_connection_is_established = False
            return device_connection_is_established

        if(device_reply != message_ack):
            print(f"{bcolors.FAIL}Target declined read request for buffer {i}!{bcolors.ENDC}")
            device_connection_is_established = False
            return device_connection_is_established

        device_reply = serial_port.read(4)
        if(len(device_reply) != 4):
            print(f"{bcolors.FAIL}Wrong response to buffer read request!{bcolors.ENDC}. Wrong answer length for second message")
            device_connection_is_established = False
            return device_connection_is_established

        if(device_reply[0] != message_prefix[0] or device_reply[1] != message_prefix[1] or device_reply[2] != message_read_debug_buffer[2]):
            print(f"{bcolors.FAIL}Wrong response to buffer read request!{bcolors.ENDC}. Wrong answer content")
            device_connection_is_established = False
            return device_connection_is_established
        
        record_type = device_reply[3]
        record_size = 1
        unpack_type = ""
        match record_type:
            case BUFFER_TYPE.F32_BUFFER.value:
                record_size = 4
                unpack_type = "f"
            case BUFFER_TYPE.U32_BUFFER.value:
                record_size = 4
                unpack_type = "I"
            case BUFFER_TYPE.I32_BUFFER.value:
                record_size = 4
                unpack_type = "i"
            case BUFFER_TYPE.U16_BUFFER.value:
                record_size = 2
                unpack_type = "H"
            case BUFFER_TYPE.I16_BUFFER.value:
                record_size = 2
                unpack_type = "h"
            case BUFFER_TYPE.U8_BUFFER.value:
                record_size = 1
                unpack_type = "B"

        print(f"Buffer {i + 1} data. Record type: {bcolors.OKBLUE}{record_type}{bcolors.ENDC}, ", end="")
        print(f"size: {bcolors.OKBLUE}{record_size}{bcolors.ENDC}, unpack type: {bcolors.OKBLUE}{unpack_type}{bcolors.ENDC}")

        actual_data = serial_port.read( record_size * buffers_length )

        if(len(actual_data) != record_size * buffers_length ):
            print(f"{bcolors.FAIL}Wrong byte count read for buffer {i}!{bcolors.ENDC}")
            print(len(actual_data))
            device_connection_is_established = False
            return device_connection_is_established

        logging_data.append([])

        for index in range(buffers_length):
            logging_data[i].append(struct.unpack(unpack_type, actual_data[record_size*index:record_size*(index+1)])[0])

    # Add data to csv file
    save_path = "logs/"
    file_name = "buffers_log_" + str(int(time.time()))+".csv"
    complete_name = os.path.join(save_path, file_name)
    file = open(complete_name, "w", newline='')
    csv_writer = csv.writer(file)

    for index_1 in range(buffers_length):
        row = []
        for index_2 in range(number_of_registered_buffers):
            row.append(logging_data[index_2][index_1])
        csv_writer.writerow(row)

    file.close()
    print(f"All buffers were saved into {bcolors.OKGREEN}{file_name}{bcolors.ENDC}")


    return device_connection_is_established

########################################

def save_streaming_data(serial_port: serial.Serial, duration_s: float):
    """Saves streamed MCU data into a .csv file inside logs/ folder
    
    @NOTE logs/ folder must exist inside the folder with this .py file
    @TODO implement
    """
    global device_connection_is_established

    print("Saving data from active stream")

    if (device_connection_is_established == False):
        print(f"{bcolors.FAIL}Device connection is not established for save_streaming_data!{bcolors.ENDC}")
        return device_connection_is_established

    serial_port.write(message_start_streaming)
    device_reply = serial_port.read(3)

    if(len(device_reply) != 3):
        print(f"{bcolors.FAIL}Wrong response to start streaming request!{bcolors.ENDC}. Wrong answer length")
        device_connection_is_established = False
        return device_connection_is_established

    if(device_reply == message_nack):
        print(f"{bcolors.FAIL}Target declined start streaming request!{bcolors.ENDC}")
        device_connection_is_established = False
        return device_connection_is_established

    if(device_reply != message_ack):
        print(f"{bcolors.FAIL}Wrong response to start streaming request!{bcolors.ENDC}. Wrong answer content")
        device_connection_is_established = False
        return device_connection_is_established

    # read message description
    device_reply = serial_port.read(13)
    if(len(device_reply) != 13):
        print(f"{bcolors.FAIL}Wrong response to start streaming request!{bcolors.ENDC}. Wrong answer length for second message")
        device_connection_is_established = False
        return device_connection_is_established

    if(device_reply[0] != message_prefix[0] or device_reply[1] != message_prefix[1] or device_reply[2] != message_start_streaming[2]):
        print(f"{bcolors.FAIL}Wrong response to start streaming request!{bcolors.ENDC}. Wrong answer content for second message")
        device_connection_is_established = False
        return device_connection_is_established

    stream_id = struct.unpack('B', device_reply[3:4])[0]
    stream_number_of_active_fields = struct.unpack('B', device_reply[4:5])[0]
    stream_entries_per_message = struct.unpack('H', device_reply[5:7])[0]
    stream_timeout_ms = struct.unpack('I', device_reply[7:11])[0]
    stream_bytes_per_message = struct.unpack('H', device_reply[11:13])[0]

    if(stream_id == 0):
        print(f"{bcolors.OKBLUE}No active stream is registered on device. Logging aborted{bcolors.ENDC}")
        return device_connection_is_established
 
    # Here we are handling the situation, when UART TX DMA cannot send messages shorted than 3 bytes
    stream_field_types_read_count = stream_number_of_active_fields
    if(stream_field_types_read_count < 3):
        stream_field_types_read_count = 3

    device_reply = serial_port.read(stream_field_types_read_count)
    if(len(device_reply) != stream_field_types_read_count):
        print(f"{bcolors.FAIL}Wrong response to start streaming request!{bcolors.ENDC}. Wrong fields description reply")
        device_connection_is_established = False
        return device_connection_is_established

    total_expected_entry_length = 0
    entry_description = []

    for i in range(stream_number_of_active_fields):
        record_type = device_reply[i]
        match record_type:
            case BUFFER_TYPE.F32_BUFFER.value:
                entry_description.append([4, "f"])
                total_expected_entry_length += 4
            case BUFFER_TYPE.U32_BUFFER.value:
                entry_description.append([4, "I"])
                total_expected_entry_length += 4
            case BUFFER_TYPE.I32_BUFFER.value:
                entry_description.append([4, "i"])
                total_expected_entry_length += 4
            case BUFFER_TYPE.U16_BUFFER.value:
                entry_description.append([2, "H"])
                total_expected_entry_length += 2
            case BUFFER_TYPE.I16_BUFFER.value:
                entry_description.append([2, "h"])
                total_expected_entry_length += 2
            case BUFFER_TYPE.U8_BUFFER.value:
                entry_description.append([1, "B"])
                total_expected_entry_length += 1


    print(f"Stream id: {bcolors.OKBLUE}{stream_id}{bcolors.ENDC}, ", end='')
    print(f"Fields count: {bcolors.OKBLUE}{stream_number_of_active_fields}{bcolors.ENDC}, ", end='')
    print(f"Entries per message: {bcolors.OKBLUE}{stream_entries_per_message}{bcolors.ENDC}, ", end="")
    print(f"Effective entry length: {bcolors.OKBLUE}{total_expected_entry_length}{bcolors.ENDC} bytes, ", end="")
    print(f"Bytes per message: {bcolors.OKBLUE}{stream_bytes_per_message}{bcolors.ENDC} ", end = "")
    print(f"Timeout: {bcolors.OKBLUE}{stream_timeout_ms}{bcolors.ENDC}ms")

    print(f"Entry structure [size, unpack type]: ", end = "")
    print(entry_description)

    if(stream_bytes_per_message % stream_entries_per_message != 0):
        print(f"{bcolors.FAIL}Wrong stream data alignment! Message length is not proportional to number of entries{bcolors.ENDC}. Aborting logging!")
        device_connection_is_established = False
        return device_connection_is_established
    
    stream_actual_bytes_per_entry = int(stream_bytes_per_message / stream_entries_per_message)
    if(stream_actual_bytes_per_entry - total_expected_entry_length > 3):
        print(f"{bcolors.FAIL}Wrong stream properties! Data struct is likely not properly aligned. There is more than 3 extra dummy bytes per entry{bcolors.ENDC}. Aborting logging!")
        device_connection_is_established = False
        return device_connection_is_established

    if( total_expected_entry_length > stream_actual_bytes_per_entry):
        print(f"{bcolors.FAIL}Wrong stream properties! Expected entry length is higher then number of bytes available per entry!{bcolors.ENDC}. Aborting logging!")
        device_connection_is_established = False
        return device_connection_is_established


    # prepare file to store logs. "logs/" folder must exist in the folder with the pythong file calling this function
    save_path = "logs/"
    file_name = "stream_id_" + str(stream_id) + "_log_" + str(int(time.time()))+".csv"
    complete_name = os.path.join(save_path, file_name)

    file = open(complete_name, "w", newline='')
    csv_writer = csv.writer(file)

    print(f"Started stream logging into {bcolors.OKGREEN}{file_name}{bcolors.ENDC}")

    timeout_seconds = stream_timeout_ms / 1000
    current_iteration_start_time = time.time()
    number_of_saved_points = 0
    print(f"Points saved: {bcolors.OKBLUE}{number_of_saved_points}{bcolors.ENDC}")

    expected_message_len = stream_bytes_per_message + 3

    logging_start_time = time.time()
    try:
        while(time.time() - current_iteration_start_time < timeout_seconds 
              and (duration_s == 0 or (time.time() - logging_start_time) < duration_s)):
            # TODO make sure that if data is started to be received as a single message, it will be
            # fully received as expected by the Windows API, and that situation when even though the
            # whole message was sent only part of it was received is possible
            device_reply = serial_port.read(expected_message_len)
            if (len(device_reply) == 0):
                continue

            if(len(device_reply) != expected_message_len):
                print(len(device_reply))
                print(device_reply)
                print(f"{bcolors.FAIL}Wrong stream message length!{bcolors.ENDC}. Stopping the stream and aborting")

                file.close()
                serial_port.write(message_stop_streaming)
                device_reply = serial_port.read(3)

                device_connection_is_established = False
                return device_connection_is_established
            
            if(device_reply[0] != message_prefix[0] or device_reply[1] != message_prefix[1] or device_reply[2] != message_stream_message_start[2]):
                print(f"{bcolors.FAIL}Wrong prefix in stream message!{bcolors.ENDC}. Stopping the stream and aborting")

                file.close()
                serial_port.write(message_stop_streaming)
                device_reply = serial_port.read(3)    

                device_connection_is_established = False
                return device_connection_is_established

            # At this point we are sure that we received proper data and therefore we need to add it to the log
            current_iteration_start_time = time.time()
    
            for entry_id in range(stream_entries_per_message):

                line = []
                read_bytes = 0

                for i in range(stream_number_of_active_fields):
                    current_field_type = entry_description[i][1]
                    current_field_size = entry_description[i][0]

                    read_start_idx = 3 + read_bytes + entry_id*stream_actual_bytes_per_entry
                    read_end_idx = read_start_idx + current_field_size
                    line.append(struct.unpack(current_field_type, device_reply[read_start_idx:read_end_idx])[0])
                    read_bytes += current_field_size

                csv_writer.writerow(line)
            number_of_saved_points += stream_entries_per_message
            print("\033[A                                          \033[A") # Clean last console line
            print(f"Points saved: {bcolors.OKBLUE}{number_of_saved_points}{bcolors.ENDC}")


        if(duration_s != 0 and (time.time() - logging_start_time) > duration_s):
            print(f"{bcolors.OKGREEN}Save duration has elapsed{bcolors.ENDC}. Closing the stream.")
        else:
            print(f"{bcolors.WARNING}Stream timeout has elapsed{bcolors.ENDC}. Closing the stream.")
    
        file.close()
        serial_port.write(message_stop_streaming)
        device_reply = serial_port.read(3)
        print(f"Saved a total of {bcolors.OKBLUE}{number_of_saved_points}{bcolors.ENDC} points")
        return

    except:
        print("loop if forcefully finished")
        time.sleep(0.01) #sleep in case PC was sending a command during exception
        
        file.close()
        serial_port.write(message_stop_streaming)
        device_reply = serial_port.read(3)    
        print("Unsubscribed from device stream!")


########################################

def send_generic_request(serial_port: serial.Serial, request_number: int):
    global device_connection_is_established

    if(request_number == 0 or request_number > 16):
        print(f"{bcolors.FAIL}Wrong request number: {request_number}{bcolors.ENDC}. Only numbers from 1 to 16 are allowed! Aborting request")
        return device_connection_is_established

    print(f"Sending generic request {request_number} to the target")

    if (device_connection_is_established == False):
        print(f"{bcolors.FAIL}Device connection is not established for send_generic_request!{bcolors.ENDC}")
        return device_connection_is_established

    # switch magic number into a proper defined constant
    global message_generic_request
    message_generic_request[2] = 0x40 + request_number - 1

    serial_port.write(message_generic_request)
    device_reply = serial_port.read(3)

    if(len(device_reply) != 3):
        print(f"{bcolors.FAIL}Wrong answer length for the response send_generic_request!{bcolors.ENDC}")
        device_connection_is_established = False
        return device_connection_is_established

    if(device_reply == message_nack):
        print(f"{bcolors.FAIL}Target declined generic request {request_number}!{bcolors.ENDC}")
        device_connection_is_established = False
        return device_connection_is_established

    if(device_reply != message_ack):
        print(f"{bcolors.FAIL}Target sent unknown responce for send_generic_request!{bcolors.ENDC}")
        device_connection_is_established = False
        return device_connection_is_established

    print(f"{bcolors.OKGREEN}Device acknowledged generic request {request_number}!{bcolors.ENDC}")
    return device_connection_is_established

########################################

def csv_to_json_trace_translate(
    csv_points_trace: io.TextIOWrapper,
    json_points_description: io.TextIOWrapper,
    absolute_timestamps: bool,
):
    """Translates MCU profiling data from CSV format into Trace Event JSON format.

    This function reads raw trace points from a CSV file, uses a JSON description
    file to map event IDs to human-readable names, and outputs a JSON file in the
    Trace Event format suitable for chrome://tracing or ui.perfetto.dev.

    @param csv_points_trace Input CSV file containing raw trace points.
    @param json_points_description JSON file mapping point IDs to event names.
    @param absolute_timestamps If True, inserts a start event with timestamp 0.

    @note The 'logs/' folder must exist in the same directory as the calling Python file.
    """
    global cyccnt_overflow_ticks

    try:
        json_dict: dict = json.load(json_points_description)
        mcu_clock_frequency = json_dict["mcu_clock_frequency"]
    except:
        print(f"{bcolors.FAIL}The JSON file has incorrect structure.{bcolors.ENDC}")
        return

    # Convert CYCCNT overflow ticks to microseconds
    cyccnt_overflow_ms: int = int(cyccnt_overflow_ticks * (1e6 / mcu_clock_frequency))

    csv_reader = csv.reader(csv_points_trace, delimiter=",")
    line = next(csv_reader, None)

    if line == None:
        print(f"{bcolors.FAIL}CSV file is empty!{bcolors.ENDC}")
        return

    # Validate CSV format
    if len(line) != 4:
        print(
            f"{bcolors.FAIL}Number of columns does not match the required CSV format for translating in Trace Event JSON format.{bcolors.ENDC}"
        )
        return

    trace_events: list[dict] = []

    # Insert synthetic start event if absolute timestamps are requested
    if absolute_timestamps:
        trace_events.append(
            {
                "name": "start_profiling",
                "ph": "X",
                "ts": 0,
                "tid": 0,
                "pid": 0,
                "dur": 0,
            }
        )

    cyccnt_overflow_count: int = 0
    previous_trace_end_ticks: int = 0

    # Iterate over CSV rows and convert to Trace Event JSON
    while line is not None:
        # Stop condition if row contains all zeros (optional sentinel)
        if line == ["0", "0", "0", "0"]:
            break
        
        # Compute end ticks for overflow detection
        trace_end_ticks = int(line[0]) + int(line[1])
        # If current end ticks < previous, a CYCCNT overflow occurred
        if previous_trace_end_ticks > trace_end_ticks:
            cyccnt_overflow_count += 1

        # Convert ticks to microseconds, adding overflow compensation
        time_stamp = (int(line[0]) * 1e6 / mcu_clock_frequency
                      ) + cyccnt_overflow_count * cyccnt_overflow_ms
        
        duration = int(line[1]) * 1e6 / mcu_clock_frequency 

        # Append event to JSON trace list
        try:
            trace_events.append(
                {
                    "name": json_dict[line[3]],
                    "ph": "X",
                    "ts": int(time_stamp),
                    "dur": int(duration),
                    "tid": int(line[2]),
                    "pid": 0,
                }
            )
        except KeyError:
            print(
                f"{bcolors.FAIL}The JSON with the description of trace event points does not have a definition for the point with id {int(line[2])}{bcolors.ENDC}"
            )

        previous_trace_end_ticks = trace_end_ticks
        line = next(csv_reader, None)

    # Wrap the trace events into a JSON dictionary
    json_trace_format = {"traceEvents": trace_events}

    # Prepare file to store logs. "logs/" folder must exist in the folder with the pythong file calling this function
    save_path = "logs/"
    file_name = "trace_log_" + str(int(time.time())) + ".json"
    complete_name = os.path.join(save_path, file_name)

    # write JSON trace to file
    with open(complete_name, "w") as json_file:
        json.dump(json_trace_format, json_file, indent=4)

    print(f"All traces were saved into {bcolors.OKGREEN}{file_name}{bcolors.ENDC}")