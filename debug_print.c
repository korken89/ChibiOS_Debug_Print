/* *
 *
 * Working layer and thread for debug prints
 *
 * */

#include "ch.h"
#include "hal.h"
#include "circularbuffer.h"
#include "debug_print.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/*===========================================================================*/
/* Module exported variables.                                                */
/*===========================================================================*/

/*===========================================================================*/
/* Module local variables and types.                                         */
/*===========================================================================*/

THD_WORKING_AREA(waDebugPrint, 128);

static thread_t *debug_thd;
/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/


/**
 * @brief           The USB Serial Manager task will handle incoming
 *                  data and direct it for decode and processing.
 *
 * @param[in] arg   Input argument (unused).
 */
static THD_FUNCTION(DebugPrintTask, arg)
{
    (void)arg;

    /* Circular buffer holder */
    circular_buffer_t cbuff;

    /* Buffer for transmitting serial USB commands */
    static uint8_t debug_buffer[DEBUG_BUFFER_SIZE];

    /* Initialize the USB transmit circular buffer */
    CircularBuffer_Init(&cbuff,
                        debug_buffer,
                        DEBUG_BUFFER_SIZE);

    /* Put the data pump thread into the list of available data pumps */
    debug_thd = chThdGetSelfX();

    while(1)
    {
        /* Wait for a start transmission event */
        chEvtWaitAny();

        /* We will only get here is a request to send data has been received */
        USBTransmitCircularBuffer(&data_pumps.USBTransmitBuffer);
    }
}
