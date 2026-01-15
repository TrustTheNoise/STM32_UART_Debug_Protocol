#include "debug_pif.h"
#include "debug_utils.h"

/**************************************************************************************************/
/*                                                                                                */
/*                                     Peripheral definitions                                     */
/*                                                                                                */
/**************************************************************************************************/
/**
 * This code uses:
 *
 *  PB10 - USART3_TX  - UART_TX
 *  PB11 - USART3_RX - UART_RX
 *
 * USART:
 *  USART3
 *
 * DMA:
 *  DMA2_Channel1 - USART3 RX
 *  DMA2_Channel2 - USART3 TX
 *
 * DMA MUX:
 *  DMAMUX1_Channel8 - DMA2_Channel1
 *  DMAMUX1_Channel9 - DMA2_Channel2
 *
 * Interrupts:
 *  USART3_IRQn
 *  DMA CHx full transfer complete IRQ
 */

/**************************************************************************************************/
/*                                                                                                */
/*                                        Local definitions                                       */
/*                                                                                                */
/**************************************************************************************************/

// Simplification definitions for DMA channels. Only used to improve readability.
//  If other channels are needed. All "@update-type: manual" functions must be updated manually!
#define UART_RX_DMA_CH                      DMA2_Channel1
#define UART_RX_DMAMUX_CH                   DMAMUX1_Channel8

#define UART_TX_DMA_CH                      DMA2_Channel2
#define UART_TX_DMAMUX_CH                   DMAMUX1_Channel9

/**************************************************************************************************/
/*                                                                                                */
/*                                  Static variables declarations                                 */
/*                                                                                                */
/**************************************************************************************************/

static uint8_t dma_buffer[DEBUG_UART_BUFFER_SIZE] = {0};

/**************************************************************************************************/
/*                                                                                                */
/*                                  Static functions declarations                                 */
/*                                                                                                */
/**************************************************************************************************/

static inline void setup_uart_gpio( void );

static inline void setup_uart_peripheral( uint32_t desired_uart_baud_rate );

static inline uint16_t setup_uart_dma( void );

/**************************************************************************************************/
/*                                                                                                */
/*                                Global functions implementations                                */
/*                                                                                                */
/**************************************************************************************************/


void setup_debug_uart_dma_interrupt( void )
{
    // Enable full transfer complete interrupt on DMA2 CH2
    DMA2_Channel2->CCR |= DMA_CCR_TCIE; // [#manual] This line is [manual] because next line is [manual]
    NVIC_EnableIRQ( DMA2_Channel2_IRQn ); // [#manual]
}


/**
 * @brief sets up everything needed for debug interface to work properly
 */
void setup_uart_debug_interface( uint32_t desired_uart_baud_rate )
{
    // Todo either specify UART baud rate as
    setup_uart_gpio();

    setup_uart_peripheral( desired_uart_baud_rate );

    (void)LOG_ERROR( setup_uart_dma() );

    setup_debug_uart_dma_interrupt();

}


/**************************************************************************************************/
/*                                                                                                */
/*                       Simple communication UART functions implementations                      */
/*                                                                                                */
/**************************************************************************************************/

/**
 * @brief Resets DMA, sets it back up again and sends the target message.
 *
 * @update-type: #auto
 *
 * @note The message must live long enough to be copied by the DMA! If message is corrupted so will
 * be the transfer. This is a problem of this implementation, but it also allows us not to use any other buffer in between.
 */
void debug_uart_send_message_dma( const uint8_t* message, const uint32_t message_len )
{
    while ( (USART3->ISR & USART_ISR_TC) != USART_ISR_TC ){} // Check that the last message was fully sent

    UART_TX_DMA_CH->CCR &= ~DMA_CCR_EN;

    UART_TX_DMA_CH->CMAR = (uint32_t)message;   // Set new source memory address
    UART_TX_DMA_CH->CNDTR = message_len;

    USART3->ICR = USART_ICR_TCCF; // Clear "Transmission complete" flag

    UART_TX_DMA_CH->CCR  |= DMA_CCR_MINC // Memory increment mode
                        | DMA_CCR_DIR   // Send from memory to peripheral
                        | DMA_CCR_EN;   // Enable DMA
}

/**
 * @brief returns the length of the message received by DMA
 *
 * @update-type: #auto
 */
uint32_t debug_uart_get_received_message_len( void )
{
    return (DEBUG_UART_BUFFER_SIZE - UART_RX_DMA_CH->CNDTR);
}

/**
 * @brief
 *
 * @update-type: #none
 */
uint8_t* debug_uart_get_receive_buffer( void )
{
    return dma_buffer;
}

/**
 * @brief
 *
 * @update-type: #auto
 */
inline void debug_uart_reset_rx_dma( void )
{
    UART_RX_DMA_CH->CCR &= ~DMA_CCR_EN; // Disable DMA.

    UART_RX_DMA_CH->CNDTR = DEBUG_UART_BUFFER_SIZE; // Reset memory location
    UART_RX_DMA_CH->CCR  |= DMA_CCR_MINC // Memory increment mode
                        | DMA_CCR_EN;   // Enable DMA
}

/**************************************************************************************************/
/*                                                                                                */
/*                                 Static functions implementations                               */
/*                                                                                                */
/**************************************************************************************************/

/**
 * @brief
 *
 * @update-type: #manual
 */
static void setup_uart_gpio( void )
{
    // The whole function below is [#manual]

    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN;

    const uint32_t gpio_b_moder_clear_mask = ~(GPIO_MODER_MODE10_Msk | GPIO_MODER_MODE11_Msk);
    const uint32_t gpio_b_moder_set_mask = GPIO_ALTERNATE_Mode << GPIO_MODER_MODE10_Pos
                                | GPIO_ALTERNATE_Mode << GPIO_MODER_MODE11_Pos;
    GPIOB->MODER = (GPIOB->MODER & gpio_b_moder_clear_mask) | gpio_b_moder_set_mask;

    GPIOB->AFR[1] |= ALTERNATE_FUNCTION_7 << GPIO_AFRH_AFSEL10_Pos
                    | ALTERNATE_FUNCTION_7 << GPIO_AFRH_AFSEL11_Pos;
}


/**
 * @brief Sets up USART to a given baud rate
 *
 * @update-type: #manual
 */
static inline void setup_uart_peripheral( const uint32_t desired_uart_baud_rate )
{
    RCC->APB1ENR1 |= RCC_APB1ENR1_USART3EN; // [#manual]

    USART3->BRR = SYSTEM_CLOCK_FREQUENCY_HZ / desired_uart_baud_rate; // Baud rate for oversampling by 16
    USART3->CR1 |= USART_CR1_UE     // Start UART
//                | USART_CR1_FIFOEN  // Enable FIFOs
                | USART_CR1_RTOIE;  // Enable receiver timeout interrupt

    USART3->CR2 |= USART_CR2_RTOEN; // Enable receiver timeout;
    USART3->RTOR = 10; // number of bits before receiver timeout interrupt;


    USART3->CR3 |= USART_CR3_EIE;   // Enable errors interrupt
    NVIC_EnableIRQ(USART3_IRQn);    // [#manual] Enable interrupt in the core

    USART3->CR1 |= USART_CR1_TE | USART_CR1_RE; // enable TX and RX. TX will send IDLE frame after being enabled
}


/**
 * @brief
 *
 * @update-type: #manual
 */
static inline uint16_t setup_uart_dma( void )
{
#ifdef DEBUG_PIF_DMA_VALIDATION
    if (UART_RX_DMA_CH->CPAR != 0)
    {
        return DMA_CHANNEL_OVERLAY_ERROR_OFFSET;
    }

    if(UART_TX_DMA_CH->CPAR != 0)
    {
        return DMA_CHANNEL_OVERLAY_ERROR_OFFSET;
    }
#endif

    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN | RCC_AHB1ENR_DMAMUX1EN; // [#manual]

    USART3->CR3 |= USART_CR3_DMAR| USART_CR3_DMAT; // Enable DMA for RX and TX;

    UART_RX_DMAMUX_CH->CCR |= DMAMUX_CxCR_DMAREQ_USART3_RX; // [#manual]
    UART_TX_DMAMUX_CH->CCR |= DMAMUX_CxCR_DMAREQ_USART3_TX; // [#manual]

    // Set all fields for USART RX DMA
    UART_RX_DMA_CH->CPAR = (uint32_t)&(USART3->RDR);
    UART_RX_DMA_CH->CMAR = (uint32_t)dma_buffer;
    UART_RX_DMA_CH->CNDTR = DEBUG_UART_BUFFER_SIZE;     // Number of bytes to receive;
    UART_RX_DMA_CH->CCR |= DMA_CCR_MINC;
    UART_RX_DMA_CH->CCR |= DMA_CCR_EN;              // Enable RX DMA channel

    // Set only peripheral address for TX
    UART_TX_DMA_CH->CPAR = (uint32_t)&(USART3->TDR);

    return 0;
}


/**************************************************************************************************/
/*                                                                                                */
/*                                        Interrupt handlers                                      */
/*                                                                                                */
/**************************************************************************************************/

/**
 * @brief debug UART interrupt handler
 */
void USART3_IRQHandler()
{
    // UART data receive sections
    if( USART3->ISR & USART_ISR_RTOF ) // UART message received
    {
        // Read and do something with data received from DMA
        const uint16_t received_size = debug_uart_get_received_message_len();
        uint8_t* received_buffer = debug_uart_get_receive_buffer();

        // Reset RX DMA after every timeout to make DMA write back from 0.
        // Given that we work with pointers, this can cause first bytes of the bu ffer to be overwritten
        // If a new message from master arrives right away. But behaviour like this is hard to effectively
        // prevent, and therefore it will be considered as an error on the side of the master implementation.
        debug_uart_reset_rx_dma();

        debug_itf_handle_rx(received_buffer, received_size);

        USART3->ICR |= USART_ICR_RTOCF; // Clear the timeout interrupt flag
    }

    // UART error handling section
    if (USART3->ISR & USART_ISR_ORE) // Overrun error detected;
    {
        USART3->ICR = USART_ICR_ORECF;
        LOG_ERROR(533); // USART1_OVERRUN_Error
    }

    if (USART3->ISR & USART_ISR_NE) // Noise error detected;
    {
        USART3->ICR = USART_ICR_NECF;
        LOG_ERROR(522); // USART1_NOISE_DETECTION_Error
    }

    if(USART3->ISR & USART_ISR_FE) // Framing error detected;
    {
        USART3->ICR = USART_ICR_FECF;
        LOG_ERROR(511); // USART1_FRAMING_Error
    }
}


/**
 * @brief debug UART TX DMA interrupt handler
 */
void DMA2_CH2_IRQHandler( void )
{
    if(DMA2->ISR & DMA_ISR_TCIF2)
    {
        debug_itf_handle_tx();

        // Clear full transfer complete, half transfer complete and global interrupt flags for DMA channel.
        //  Even though CGIFx will clear all other flags by itself, all 3 flags are cleared manually for possible
        //  Compatibility reasons with other STM32 families
        DMA2->IFCR = DMA_IFCR_CTCIF2 | DMA_IFCR_CHTIF2 | DMA_IFCR_CGIF2;
    }
}



// EOF
