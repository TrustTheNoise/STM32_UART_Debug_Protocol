#include "profiling_pif.h"
#include "utils_pif.h"
#include "device_uart_debug_protocol.h"
#include "math.h"
#include "clocking_pif.h"

#define NUMBER_OF_MEASUREMENTS         100

#define PROFILING_USE_SINGLE_THREAD_ID  0

/**************************************************************************************************/
/*                                                                                                */
/*                                 Static functions declarations                                  */
/*                                                                                                */
/**************************************************************************************************/

static void example_sqrtf_test( void );
static void example_lnf_test( void );
static void example_expf_test( void );
static void example_exp2f_test (void );
static void example_setup_debug_digital_outputs( void );

enum profiling_event_name_id_t
{
    PROFILING_ALL               = 0,
    PROFILING_SQRT              = 1,
    PROFILING_LNF_EXPF          = 2,
    PROFILING_LNF               = 3,
    PROFILING_ALL_EXPF          = 4,
    PROFILING_EXPF              = 5,
    PROFILING_EXP2F             = 6,
    PROFILING_STARTUP           = 7,
};

#if PROFILING_USE_SINGLE_THREAD_ID == 1
    enum profiling_thread_id_t
    {
        PROFILING_THREAD_MAIN       = 0,
        PROFILING_THREAD_LNF_EXPF   = 0,
        PROFILING_THREAD_ALL_EXPF   = 0,
        PROFILING_THREAD_MATH       = 0,
    };
#elif PROFILING_USE_SINGLE_THREAD_ID == 0
    enum profiling_thread_id_t
    {
        PROFILING_THREAD_MAIN       = 0,
        PROFILING_THREAD_LNF_EXPF   = 1,
        PROFILING_THREAD_ALL_EXPF   = 2,
        PROFILING_THREAD_MATH       = 3,
    };
#endif

/**************************************************************************************************/
/*                                                                                                */
/*                                         Main function                                          */
/*                                                                                                */
/**************************************************************************************************/

/**
 * @brief Example of Visual Profiling with Google Trace Event Profiling Tool
 *
 * This example demonstrates:
 *  1. Setting up profiling for trace events using UART, both with the debug stream
 *      and debug buffer methods (see other examples in the Examples_debug folder for more details on these methods).
 *  2. Creating profiling events and using the start/end tracking functions with those events.
 *  3. A simple profiling example for mathematical functions from math.h, repeated NUMBER_OF_MEASUREMENTS times.
 *  4. Comparing the visual profiling results with data captured by a logic analyzer.
 *
 *
 * How to test:
 *  1. Connect a USB–UART converter to PB10(UART_RX) and PB11(UART_TX).
 *  2. Connect the USB–UART converter to your PC.
 *  3. Run the example.
 *  4. Open python_debug_logger
 *
 * For the debug stream:
 *  5.a. Set the COM port (connected to USB–UART) and baud rate to 500000, then run com_save_stream.py.
 *       Ensure there are no errors in the console. The script will continuously save
 *       values into a CSV file with a fast update rate. To stop it, use CTRL+C.
 *       This will stop the stream and save all previously read values into a .csv file.
 *
 * For the debug buffer:
 *  5.b. Set the COM port (connected to USB–UART) and baud rate to 500000, then run com_save_stream.py.
 *       Ensure there are no errors in the console. The script will stop automatically.
 *
 *  6. Check the log folder for the corresponding file: "stream_id_255_log_xxxxxxxxx.csv"
 *     for the debug stream or "buffers_log_xxxxxxxxx.csv" for the debug buffer.
 *  7. Run trace_event_parser.py with two required arguments:
 *     - First: the path to the profiling .csv file.
 *     - Second: the path to the JSON file describing each trace event (included in the example as points_description.json).
 *     Example (assuming both files are in the log folder of python_debug_logger):
 *
 *     python .\trace_event_parser.py .\logs\buffers_log_1757517364.csv .\points_description.json
 *
 *     This will generate a trace_log_xxxxxxxxx.json file.
 *     For more information, run the script with the --help flag.
 *  8. Open a Chromium-based browser and go to chrome://tracing/, or use ui.perfetto.dev.
 *     Click Load/Open trace file and select the generated JSON file. If there are no errors, the visual profiling view will appear.
 *
 * @note If you want to see how profiling looks when all events share the same thread ID,
 *       you can set define PROFILING_USE_SINGLE_THREAD_ID to 1
 *
 * Additional Validation with a Logic Analyzer:
 *  1. Connect the logic analyzer probes to PB0, PB1, PB2, PB3.
 *  2. Configure the analyzer software and start measurement.
 *  3. Run the code.
 *  4. Stop the analyzer when you see activity on each pin.
 *  5. Pin numbers PBx correspond to Thread x in the visual profiling.
 *     You can compare the visual profiling data with the analyzer output.
 */
int main(void)
{
    enable_fpu();

    setup_system_clock();

    setup_profiling_stream_tracing();
    setup_profiling_buffer_tracing();

    profiling_event profiling_startup = {
            .name_id = PROFILING_STARTUP,
            .thread_id = PROFILING_THREAD_MAIN,
    };


    profiling_buffer_trace_event_begin(&profiling_startup);

    example_setup_debug_digital_outputs();

    profiling_buffer_trace_event_end(&profiling_startup);



    profiling_event profiling_all = {
            .name_id = PROFILING_ALL,
            .thread_id = PROFILING_THREAD_MAIN,
    };
    profiling_event profiling_lnf_expf = {
            .name_id = PROFILING_LNF_EXPF,
            .thread_id = PROFILING_THREAD_LNF_EXPF,
    };
    profiling_event profiling_all_expf = {
            .name_id = PROFILING_ALL_EXPF,
            .thread_id = PROFILING_THREAD_ALL_EXPF,
    };

    while(1)
    {
        GPIOB->BSRR = GPIO_BSRR_BS_0;
        profiling_stream_trace_event_begin(&profiling_all);

        example_sqrtf_test();
        // All dummy delays are used here only for visual separation in the logic
        // analyzer when NUMBER_OF_MEASUREMENTS is large.
        dummy_delay_us(50);

        GPIOB->BSRR = GPIO_BSRR_BS_1;
        profiling_stream_trace_event_begin(&profiling_lnf_expf);

        example_lnf_test();
        dummy_delay_us(50);

        GPIOB->BSRR = GPIO_BSRR_BS_2;
        profiling_stream_trace_event_begin(&profiling_all_expf);

        example_expf_test();
        dummy_delay_us(50);

        profiling_stream_trace_event_end(&profiling_lnf_expf);
        GPIOB->BSRR = GPIO_BSRR_BR_1;


        example_exp2f_test();
        dummy_delay_us(50);

        profiling_stream_trace_event_end(&profiling_all_expf);
        GPIOB->BSRR = GPIO_BSRR_BR_2;

        profiling_stream_trace_event_end(&profiling_all);
        GPIOB->BSRR = GPIO_BSRR_BR_0;

        dummy_delay_us(MSEC_TO_USEC(500));
    }

    // For debug buffer method, profiling runs once,
    // followed by an infinite loop to keep the microcontroller active.
    while(1){}
}


/**************************************************************************************************/
/*                                                                                                */
/*                                Static functions implementations                                */
/*                                                                                                */
/**************************************************************************************************/

static void example_setup_debug_digital_outputs( void )
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

    const uint32_t gpio_b_moder_clear_mask = ~(GPIO_MODER_MODE0_Msk | GPIO_MODER_MODE1_Msk | GPIO_MODER_MODE2_Msk
                                    | GPIO_MODER_MODE3_Msk);
    const uint32_t gpio_b_moder_set_mask = GPIO_DIGITAL_OUT_Mode << GPIO_MODER_MODE0_Pos
                                            | GPIO_DIGITAL_OUT_Mode << GPIO_MODER_MODE1_Pos
                                            | GPIO_DIGITAL_OUT_Mode << GPIO_MODER_MODE2_Pos
                                            | GPIO_DIGITAL_OUT_Mode << GPIO_MODER_MODE3_Pos;

    GPIOB->MODER = (GPIOB->MODER & gpio_b_moder_clear_mask) | gpio_b_moder_set_mask;
}


static void example_sqrtf_test(void)
{
    profiling_event profiling_sqrt = {
            .name_id = PROFILING_SQRT,
            .thread_id = PROFILING_THREAD_MATH,
    };
    GPIOB->BSRR = GPIO_BSRR_BS_1;
    profiling_stream_trace_event_begin(&profiling_sqrt);

    DEBUG_UNUSED volatile float test_number;

    for (uint16_t i = 0; i < NUMBER_OF_MEASUREMENTS; i++)
    {
        test_number = sqrtf(i);
    }

    profiling_stream_trace_event_end(&profiling_sqrt);
    GPIOB->BSRR = GPIO_BSRR_BR_1;
}


static void example_lnf_test(void)
{
    profiling_event profiling_ln = {
            .name_id = PROFILING_LNF,
            .thread_id = PROFILING_THREAD_MATH,
    };
    DEBUG_UNUSED volatile float test_number;

    GPIOB->BSRR = GPIO_BSRR_BS_2;
    profiling_stream_trace_event_begin(&profiling_ln);

    for (uint16_t i = 0; i < NUMBER_OF_MEASUREMENTS; i++)
    {
        test_number = logf(i);
    }

    profiling_stream_trace_event_end(&profiling_ln);
    GPIOB->BSRR = GPIO_BSRR_BR_2;
}


static void example_expf_test(void)
{
    profiling_event profiling_expf = {
            .name_id = PROFILING_EXPF,
            .thread_id = PROFILING_THREAD_MATH,
    };

    DEBUG_UNUSED volatile float test_number;

    GPIOB->BSRR = GPIO_BSRR_BS_3;
    profiling_stream_trace_event_begin(&profiling_expf);
    for (uint16_t i = 0; i < NUMBER_OF_MEASUREMENTS; i++)
    {
        test_number = expf(i);
    }
    profiling_stream_trace_event_end(&profiling_expf);
    GPIOB->BSRR = GPIO_BSRR_BR_3;
}


static void example_exp2f_test(void)
{
    profiling_event profiling_exp2f = {
            .name_id = PROFILING_EXP2F,
            .thread_id = PROFILING_THREAD_MATH,
    };
    DEBUG_UNUSED volatile float test_number;

    GPIOB->BSRR = GPIO_BSRR_BS_1;
    profiling_stream_trace_event_begin(&profiling_exp2f);

    for(uint16_t i = 0; i < NUMBER_OF_MEASUREMENTS; i++)
    {
        test_number = exp2f(i);
    }
    profiling_stream_trace_event_end(&profiling_exp2f);
    GPIOB->BSRR = GPIO_BSRR_BR_1;
}
