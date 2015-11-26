/* *
 *
 * Working layer and thread for debug prints
 *
 * */

#include "ch.h"
#include "hal.h"
#include "debug_print.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

static msg_t bss2cbuff(void *instance, uint8_t b);

/**
 * @brief   Circular buffer holder definition.
 */
typedef struct
{
	/**
	 * @brief   Position of the head of the buffer.
	 */
    uint32_t head;          /* Newest element */
	/**
	 * @brief   Position of the tail of the buffer.
	 */
    uint32_t tail;          /* Oldest element */
	/**
	 * @brief   Size of the circular buffer.
	 */
    uint32_t size;          /* Size of buffer */
	/**
	 * @brief   Pointer to the data holding region.
	 */
    uint8_t *buffer;        /* Pointer to memory area */
} circular_buffer_t;

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

/**
 * @brief                   Initializes a circular buffer except the mutex.
 *
 * @param[in] Cbuff         Pointer to the circular buffer.
 * @param[in] buffer        Pointer to where the circular buffer data is stored.
 * @param[in] buffer_size   Size of the circular buffer in bytes.
 */
static void CircularBuffer_Init(circular_buffer_t *Cbuff,
                         uint8_t *buffer,
                         uint32_t buffer_size)
{
    Cbuff->head = 0;
    Cbuff->tail = 0;
    Cbuff->size = buffer_size;
    Cbuff->buffer = buffer;
}

/**
 * @brief               Writes a byte to a circular buffer.
 *
 * @param[in] Cbuff     Pointer to the circular buffer.
 */
static void CircularBuffer_WriteSingle(circular_buffer_t *Cbuff, uint8_t data)
{
    Cbuff->buffer[Cbuff->head] = data;
    Cbuff->head = ((Cbuff->head + 1) % Cbuff->size);
}

/**
 * @brief               Increment the circular buffer pointer to math the
 *                      number of bytes written.
 *
 * @param[in/out] Cbuff Pointer to the circular buffer.
 * @param[in] count     Number of bytes to increment the pointer.
 */
static bool CircularBuffer_Increment(circular_buffer_t *Cbuff, int32_t count)
{
    if (count == -1) /* Error! */
        return HAL_FAILED;

    else
    {
        Cbuff->head = ((Cbuff->head + count) % Cbuff->size);
        return HAL_SUCCESS;
    }
}

/**
 * @brief               Generates a pointer to the tail byte and returns a size
 *                      which for how many bytes can be read from the circular
 *                      buffer.
 *
 * @param[in/out] Cbuff Pointer to the circular buffer.
 * @param[out] size     Pointer to the size holder.
 * @return              Pointer to the buffer with offset.
 */
static uint8_t *CircularBuffer_GetReadPointer(circular_buffer_t *Cbuff,
                                       uint32_t *size)
{
    uint8_t *p;

    p = (Cbuff->buffer + Cbuff->tail);

    if (Cbuff->head < Cbuff->tail)
        *size = Cbuff->size - Cbuff->tail;
    else
        *size = Cbuff->head - Cbuff->tail;

    return p;
}

static void CircularBuffer_IncrementTail(circular_buffer_t *Cbuff, int32_t count)
{
    Cbuff->tail = ((Cbuff->tail + count) % Cbuff->size);
}

static uint32_t CircularBuffer_SpaceLeft(circular_buffer_t *Cbuff)
{
    return (Cbuff->tail + Cbuff->size - Cbuff->head - 1) % Cbuff->size;
}



/**
 * @brief               Writes a byte to the circular buffer, if its vaule is
 *                      SYNC: write it twice.
 *
 * @param[in/out] Cbuff Pointer to the circular buffer.
 * @param[in/out] data  Byte being written.
 * @param[in/out] count Pointer to tracking variable for the size of the
 *                      data being written to the circular buffer.
 */
void CircularBuffer_WriteNoIncrement(circular_buffer_t *Cbuff,
                                     uint8_t data,
                                     int32_t *count,
                                     uint8_t *crc8,
                                     uint16_t *crc16)
{
    if (data == SLIP_END)
    {
        Cbuff->buffer[(Cbuff->head + *count) % Cbuff->size] = SLIP_ESC;
        *count += 1;
        Cbuff->buffer[(Cbuff->head + *count) % Cbuff->size] = SLIP_ESC_END;
        *count += 1;
    }
    else if (data == SLIP_ESC)
    {
        Cbuff->buffer[(Cbuff->head + *count) % Cbuff->size] = SLIP_ESC;
        *count += 1;
        Cbuff->buffer[(Cbuff->head + *count) % Cbuff->size] = SLIP_ESC_ESC;
        *count += 1;
    }
    else
    {
        /* Check if we have 2 bytes free, in case of data = SYNC */
        Cbuff->buffer[(Cbuff->head + *count) % Cbuff->size] = data;
        *count += 1;
    }
}

/**
 * @brief               Writes a SYNC byte to the circular buffer.
 *
 * @param[in/out] Cbuff Pointer to the circular buffer.
 * @param[in/out] count Pointer to tracking variable for the size of the
 *                      data being written to the circular buffer.
 * @param[in] crc8      Pointer to the CRC8 data holder.
 * @param[in] crc16     Pointer to the CRC16 data holder.
 */
void CircularBuffer_WriteSYNCNoIncrement(circular_buffer_t *Cbuff,
                                         int32_t *count)
{
    Cbuff->buffer[(Cbuff->head + *count) % Cbuff->size] = SLIP_ESC;
    *count += 1;
}

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
    circular_buffer_t slip_cbuff;

    /* Buffer for transmitting */
    static uint8_t debug_buffer[DEBUG_BUFFER_SIZE];
    static uint8_t slip_buffer[DEBUG_BUFFER_SIZE];

    /* Initialize the transmit circular buffer */
    CircularBuffer_Init(&cbuff,
                        debug_buffer,
                        DEBUG_BUFFER_SIZE);

    CircularBuffer_Init(&slip_cbuff,
                        slip_buffer,
                        DEBUG_BUFFER_SIZE);

    cbuff_ptr = &cbuff;
    slip_cbuff_ptr = &slip_cbuff;

    while(1)
    {
        chThdSleepMilliseconds(1);

        /* We will only get here is a request to send data has been received */
        TransmitCircularBuffer(&cbuff);
        TransmitCircularBuffer(&slip_cbuff);
    }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

static circular_buffer_t *slip_cbuff_ptr;

void vInitDebugPrint(BaseSequentialStream *output_bss)
{
    print_out_ptr = output_bss;

    chThdCreateStatic(waDebugPrint,
                      sizeof(waDebugPrint),
                      IDLEPRIO + 1,
                      DebugPrintTask,
                      NULL);
}


bool GenerateSLIP(uint8_t *data, const uint32_t data_count)
{
    int32_t count = 0;
    uint32_t i;

    if (CircularBuffer_SpaceLeft(Cbuff) < (data_count + 2))
        return HAL_FAILED;

    /* Add the header */
    CircularBuffer_WriteSYNCNoIncrement(Cbuff, &count);

    /* Add the data to the message */
    for (i = 0; i < data_count; i++)
        CircularBuffer_WriteNoIncrement(Cbuff, data[i], &count);

    /* Add the end */
    CircularBuffer_WriteSYNCNoIncrement(Cbuff, &count);

    /* Check if the message fit inside the buffer */
    return CircularBuffer_Increment(Cbuff, count);
}
