// Documentation is in the end of the file
#ifndef PROFILING_PIF_H_
#define PROFILING_PIF_H_

//#include "device_definitions.h"
#include "stm32g474xx.h"
//#include "device_mcu_includes.h"

#include "../debug_lib/debug_utils.h"



/**************************************************************************************************/
/*                                                                                                */
/*                                    Global types declarations                                   */
/*                                                                                                */
/**************************************************************************************************/


typedef struct {
    uint32_t start_stamp;
    uint32_t duration;
    uint32_t thread_id;             // needs to be initialized
    uint16_t name_id;               // needs to be initialized
}profiling_event;



/**************************************************************************************************/
/*                                                                                                */
/*                                  Global functions declarations                                 */
/*                                                                                                */
/**************************************************************************************************/

void profiling_enable_dwt_counter( void );

void setup_profiling_buffer_tracing( void );
void setup_profiling_stream_tracing( void );

void profiling_buffer_trace_event_begin(profiling_event* profiling_event_instance);
void profiling_buffer_trace_event_end(profiling_event* profiling_event_instance);

void profiling_stream_trace_event_begin(profiling_event* profiling_event_instance);
void profiling_stream_trace_event_end(profiling_event* profiling_event_instance);


/**************************************************************************************************/
/*                                                                                                */
/*                             Static inline functions implementations                            */
/*                                                                                                */
/**************************************************************************************************/

/**
 * @brief Saves current DWT counter value into a given variable. Can be fully removed by a global definition
 *
 * DWT counter counts clock cycles into 32 bit register and therefore can be used to measure time.
 *
 * The function is done in such a weird way to be able to be disabled using build definitions
 *  without additional error generation
 */
static inline void profiling_save_current_dwt_clock_cycle( uint32_t* save_target )
{
#ifndef PROFILING_DISABLED
    *save_target = DWT->CYCCNT;
#endif
}



#endif /* PROFILING_PIF_H_ */

/**
 *
 * Due to the nature of profiling process it has to be a hardware dependent solution and cannot be effectively abstracted without
 *  causing additional delays in the
 */
