#include <profiling_pif.h>
#include "../debug_lib/debug_protocol/debug_protocol.h"


/**************************************************************************************************/
/*                                                                                                */
/*                                  Static variables declarations                                 */
/*                                                                                                */
/**************************************************************************************************/


#ifdef DEBUG_ENABLE_PROFILING
static struct {
    debug_u32_buffer start_stamp;
    debug_u32_buffer duration;
    debug_u32_buffer thread_id;
    debug_u16_buffer name_id;
} profiling_buffer;

// TODO Not sure what the optimal value should be here. Set to 8 for
// easier debugging with smaller buffer.
#define STREAM_BUFFER_LENGTH        8
// Double buffer for storing profiling events in stream mode.
static profiling_event profiling_stream_buffer_a[STREAM_BUFFER_LENGTH];
static profiling_event profiling_stream_buffer_b[STREAM_BUFFER_LENGTH];
static profiling_event* record_stream_buffer = profiling_stream_buffer_a;
// Index of the next free slot in the current stream buffer.
static uint8_t buf_index = 0;

static debug_com_stream profiling_stream =
{
        .timeout_ms = 20000,
        .entries_per_message_count = STREAM_BUFFER_LENGTH,
        .id = 255, // TODO: select a separate id for profiling
        .entry_fields_count = 4,
        .entry_fields_types = { U32_Type, U32_Type, U32_Type, U16_Type},
        .message_byte_size = sizeof(profiling_event) * STREAM_BUFFER_LENGTH,
        .is_active = 0,
};
#endif

/****************************************************************************************/
/*                                                                                      */
/*                            Static functions declarations                             */
/*                                                                                      */
/****************************************************************************************/

static inline void profiling_debug_buffer_save(profiling_event* profiling_event_instance);
static inline void profiling_debug_stream_save(profiling_event* profiling_event_instance);

/**************************************************************************************************/
/*                                                                                                */
/*                                  Global functions definitions                                  */
/*                                                                                                */
/**************************************************************************************************/

/**
 * @brief enables DWT counter
 *
 * DWT CNT is a 32 bit counter that counts clock cycles.
 */
void profiling_enable_dwt_counter( void )
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable trace (?)
    ITM->LAR = 0xC5ACCE55; // Unlock access to DWT registers
    DWT->CYCCNT = 0; // Reset counter
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk; // Enable DWT cycle counter
}


/**
 * @brief Setup profiling using debug buffer method.
 *
 * This function initializes the profiling system to work with the debug buffer method.
 * registers all profiling fields in the communication buffer, and assigns the
 * function pointer `profiling_debug_save` to `profiling_debug_buffer_save`.
 */
void setup_profiling_buffer_tracing( void )
{
#ifdef DEBUG_ENABLE_PROFILING
    profiling_enable_dwt_counter();

    debug_register_com_buffer((void*)&profiling_buffer.start_stamp, U32_Type);
    debug_register_com_buffer((void*)&profiling_buffer.duration, U32_Type);
    debug_register_com_buffer((void*)&profiling_buffer.thread_id, U32_Type);
    debug_register_com_buffer((void*)&profiling_buffer.name_id, U16_Type);
#endif
}


/**
 * @brief Setup profiling using stream buffer method.
 *
 * This function initializes the profiling system to work with the stream buffer method.
 * It registers the profiling stream, and assigns the function pointer
 * `profiling_debug_save` to `profiling_debug_stream_save`.
 */
void setup_profiling_stream_tracing( void )
{
#ifdef DEBUG_ENABLE_PROFILING
    profiling_enable_dwt_counter();

    profiling_stream.message = profiling_stream_buffer_a;

    debug_register_com_stream(&profiling_stream);
#endif
}




/**
 * @brief Start a profiling event (buffer-based mode).
 *
 * Records the current DWT cycle count as the event start timestamp.
 *
 * @param profiling_event_instance Pointer to the profiling event structure.
 */
void profiling_buffer_trace_event_begin(profiling_event* profiling_event_instance)
{
#ifdef DEBUG_ENABLE_PROFILING
    profiling_event_instance->start_stamp = DWT->CYCCNT;
#endif
}


/**
 * @brief End a profiling event (buffer-based mode).
 *
 * Calculates the event duration using DWT cycle counts, and saves the
 * duration in debug buffer via profiling_debug_stream_save.
 *
 * @param profiling_event_instance Pointer to the profiling event structure.
 */
void profiling_buffer_trace_event_end(profiling_event* profiling_event_instance)
{
#ifdef DEBUG_ENABLE_PROFILING
    uint32_t stop_stamp = DWT->CYCCNT;

    profiling_event_instance->duration = stop_stamp - profiling_event_instance->start_stamp;

    profiling_debug_buffer_save(profiling_event_instance);
#endif
}



/**
 * @brief Start a profiling event (streaming mode).
 *
 * Records the current DWT cycle count as the event start timestamp
 *
 * @param profiling_event_instance Pointer to the profiling event structure.
 */
void profiling_stream_trace_event_begin(profiling_event* profiling_event_instance)
{
#ifdef DEBUG_ENABLE_PROFILING
    profiling_event_instance->start_stamp = DWT->CYCCNT;
#endif
}


/**
 * @brief End a profiling event (streaming mode).
 *
 * Calculates the event duration using DWT cycle counts, and sends the duration
 * to the debug stream via profiling_debug_stream_save
 *
 * @param profiling_event_instance Pointer to the profiling event structure.
 */
void profiling_stream_trace_event_end(profiling_event* profiling_event_instance)
{
#ifdef DEBUG_ENABLE_PROFILING
    uint32_t stop_stamp = DWT->CYCCNT;

    profiling_event_instance->duration = stop_stamp - profiling_event_instance->start_stamp;

    profiling_debug_stream_save(profiling_event_instance);
#endif
}

/**************************************************************************************************/
/*                                                                                                */
/*                                Static functions implementations                                */
/*                                                                                                */
/**************************************************************************************************/


/**
 * @brief Save a profiling event to the debug buffer.
 *
 * This function writes the values of a profiling event instance
 * into the registered debug buffers.
 *
 * @param[in] profiling_event_instance Pointer to the profiling event to save.
 */
static inline void profiling_debug_buffer_save(profiling_event* profiling_event_instance)
{
#ifdef DEBUG_ENABLE_PROFILING
    debug_add_value_to_u32_buffer(&profiling_buffer.start_stamp,
            profiling_event_instance->start_stamp);

    debug_add_value_to_u32_buffer(&profiling_buffer.duration,
            profiling_event_instance->duration);

    debug_add_value_to_u32_buffer(&profiling_buffer.thread_id,
            profiling_event_instance->thread_id);

    debug_add_value_to_u16_buffer(&profiling_buffer.name_id,
            profiling_event_instance->name_id);
#endif
}


/**
 * @brief Save a profiling event to the stream buffer.
 *
 * This function writes a profiling event instance into the current
 * record in the stream buffer. When the buffer is full, it swaps
 * buffers and triggers the debug system to send the collected data.
 *
 * @param[in] profiling_event_instance Pointer to the profiling event to save.
 */
static inline void profiling_debug_stream_save(profiling_event* profiling_event_instance)
{
#ifdef DEBUG_ENABLE_PROFILING
    // Get pointer to current record in stream buffer
    profiling_event* buf = &record_stream_buffer[buf_index];

    // Copy profiling event data to current buffer record
    buf->start_stamp = profiling_event_instance->start_stamp;
    buf->duration = profiling_event_instance->duration;
    buf->thread_id = profiling_event_instance->thread_id;
    buf->name_id = profiling_event_instance->name_id;

    // Increment buffer index and wrap around if needed
    buf_index += 1;

    // If buffer is full, swap buffers and notify debug system
    if(buf_index == STREAM_BUFFER_LENGTH)
    {
        buf_index = 0;
        if(record_stream_buffer == profiling_stream_buffer_a)
        {
            profiling_stream.message = profiling_stream_buffer_a;
            record_stream_buffer = profiling_stream_buffer_b;
        }else
        {
            profiling_stream.message = profiling_stream_buffer_b;
            record_stream_buffer = profiling_stream_buffer_a;
        }

        // Trigger the debug system to update the stream
        debug_update_com_stream();
    }
#endif
}


/**************************************************************************************************/
/*                                                                                                */
/*                                          Unused code                                           */
/*                                                                                                */
/**************************************************************************************************/

/**
 * @brief Legacy automatic thread ID allocator (currently unused).
 *
 * This code was previously used to automatically assign compact and unique
 * thread IDs for profiling trace events. The allocator ensures that thread
 * IDs are reused efficiently and remain tightly packed. It maintains a bitmask
 * (`thread_id_mask`) where each bit represents the usage status of a thread ID
 *
 * When a new profiling event begins, alloc_thread_id() finds the lowest
 * available ID (using __builtin_ctz() to locate the first zero bit) and marks
 * it as used. When the event ends, free_thread_id() releases the ID by
 * clearing its corresponding bit in the mask.
 *
 * With this implementation, up to 32 unique thread IDs (0–31) can be
 * active at once.
 *
 * This mechanism is not used in the current profiling system because automatic
 * sequential assignment of thread IDs resulted in visually cluttered profiling
 * output. All events were displayed in a single mixed timeline, making it
 * difficult to distinguish between different logical levels or system modules.
 *
 * With manual thread ID assignment, each subsystem or logical layer can be
 * clearly separated in the trace visualization, producing a much cleaner and
 * more informative profiling view.
 */


#define MAX_THREAD_ID           31
#define ERROR_THREAD_ID         32
// Bitmask tracking allocated thread IDs.
static uint32_t thread_id_mask = 0; // (0b1 << id) -> id is occupied


/**
 * @brief Allocate a unique thread ID.
 *
 * This function finds the first available thread ID by checking the
 * thread_id_mask. Each bit in thread_id_mask represents whether the
 * corresponding thread ID is in use (1) or free (0). It marks the
 * allocated thread ID as used and returns it.
 *
 * @return Allocated thread ID (0..MAX_THREAD_ID) or ERROR_THREAD_ID if no IDs are free.
 */
static inline uint8_t alloc_thread_id(void)
{
    if(thread_id_mask == UINT32_MAX) // there are no free thread id's
    {
        return ERROR_THREAD_ID;
    }
    uint8_t idx = __builtin_ctz(~thread_id_mask);
    thread_id_mask |= (0b1u << idx);
    return idx;
}


/**
 * @brief Free a previously allocated thread ID.
 *
 * This function clears the corresponding bit in thread_id_mask,
 * marking the thread ID as available again.
 *
 * @param[in] thread_id The thread ID to free.
 */
static inline void free_thread_id(uint32_t thread_id)
{
    // TODO Maybe remove this check since alloc_thread_id won't
    // allow thread_id to be outside the required range?
    if(thread_id < 0 || thread_id > MAX_THREAD_ID)
    {
        LOG_ERROR(5556); // TODO change error code
        return;
    }
    thread_id_mask &= ~(0b1u << thread_id);
}



