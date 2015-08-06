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

static msg_t bss2cbuff(void *instance, uint8_t b);

/*===========================================================================*/
/* Module exported variables.                                                */
/*===========================================================================*/

const struct BaseSequentialStreamVMT debug_print_vmt = {
    .write = NULL,
    .read = NULL,
    .put = bss2cbuff,
    .get = NULL
};

BaseSequentialStream debug_print = {
    .vmt = &debug_print_vmt
};

/*===========================================================================*/
/* Module local variables and types.                                         */
/*===========================================================================*/

static THD_WORKING_AREA(waDebugPrint, 128);
static BaseSequentialStream *print_out_ptr;
static circular_buffer_t *cbuff_ptr;

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

static msg_t bss2cbuff(void *instance, uint8_t b)
{
    if (cbuff_ptr != NULL)
    {
        CircularBuffer_WriteSingle(cbuff_ptr, b);
        return HAL_SUCCESS;
    }
    else
        return HAL_FAILED;
}

/**
 * @brief               Transmits a circular buffer over the interface.
 *
 * @param[in] Cbuff     Circular buffer to transmit.
 * @return              Returns HAL_FAILED if it did not succeed to transmit
 *                      the buffer, else HAL_SUCCESS is returned.
 */
static bool TransmitCircularBuffer(circular_buffer_t *Cbuff)
{
    uint8_t *read_pointer;
    uint32_t read_size;

    if (Cbuff != NULL)
    {
        /* Read out the number of bytes to send and the pointer to the
           first byte */
        read_pointer = CircularBuffer_GetReadPointer(Cbuff, &read_size);

        while (read_size > 0)
        {
            /* Send the data from the circular buffer */
            chSequentialStreamWrite(print_out_ptr, read_pointer, read_size);

            /* Increment the circular buffer tail */
            CircularBuffer_IncrementTail(Cbuff, read_size);

            /* Get the read size again in case new data is available or if
               we reached the end of the buffer (to make sure the entire
               buffer is sent) */
            read_pointer = CircularBuffer_GetReadPointer(Cbuff, &read_size);
        }

        /* Transfer finished successfully */
        return HAL_SUCCESS;
    }
    else /* Some error occurred */
        return HAL_FAILED;
}


static THD_FUNCTION(DebugPrintTask, arg)
{
    (void)arg;

    /* Circular buffer holder */
    circular_buffer_t cbuff;

    /* Buffer for transmitting */
    static uint8_t debug_buffer[DEBUG_BUFFER_SIZE];

    /* Initialize the transmit circular buffer */
    CircularBuffer_Init(&cbuff,
                        debug_buffer,
                        DEBUG_BUFFER_SIZE);


    cbuff_ptr = &cbuff;

    while(1)
    {
        chThdSleepMilliseconds(1);

        /* We will only get here is a request to send data has been received */
        TransmitCircularBuffer(&cbuff);
    }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

void vInitDebugPrint(BaseSequentialStream *output_bss)
{
    print_out_ptr = output_bss;

    chThdCreateStatic(waDebugPrint,
                      sizeof(waDebugPrint),
                      IDLEPRIO + 1,
                      DebugPrintTask,
                      NULL);
}
