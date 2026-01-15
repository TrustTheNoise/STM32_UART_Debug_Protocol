import hople_com_dbg_protocol as cdp
import argparse

# Import definitions for console coloring
from helper_scripts import console_colors

bcolors = console_colors.bcolors

parser = argparse.ArgumentParser(
    formatter_class=argparse.RawDescriptionHelpFormatter,
    description="""
Converter of profiling data from a microcontroller (recorded in a CSV file) to JSON format for Google Tracing.

This tool requires two files:
1. The CSV file containing the measurements.
2. A description JSON file specifying th device's clock frequency (used during profiling) and defining names for the profiling IDs.""",
)

parser.add_argument(
    "csv_points_trace",
    type=str,
    help="The CSV file containing profiling measurements with structure [time_stamp, thread_id, id]",
)
parser.add_argument(
    "json_points_description",
    type=str,
    help='JSON file defining names for the profiling IDs that has mandatory field "mcu_clock_frequency" ',
)
parser.add_argument(
    "--absolute-timestamp",
    action="store_true",
    help="Adjust timestamps to start from 0 (absolute mode).",
)


def main():
    """Converter of profiling data from a microcontroller (recorded in a CSV file) to JSON format for Google Tracing.
    
    This tool requires two files:
    1. The CSV file containing the measurements.
    2. A description JSON file specifying the device's clock frequency (used during profiling) and defining names for the profiling IDs.

    Type flag -h with this python module to get more information 
    """
    args = parser.parse_args()

    with (
        open(args.csv_points_trace, mode="r", newline="") as csv_file,
        open(args.json_points_description, mode="r") as json_file,
    ):
        cdp.csv_to_json_trace_translate(csv_file, json_file, args.absolute_timestamp)


if __name__ == "__main__":
    main()
