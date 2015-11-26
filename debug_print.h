#ifndef __DEBUG_PRINT_H
#define __DEBUG_PRINT_H

/*===========================================================================*/
/* Module global definitions.                                                */
/*===========================================================================*/

#ifndef DEBUG_BUFFER_SIZE
#define DEBUG_BUFFER_SIZE 256
#endif

#define SLIP_END        0xC0
#define SLIP_ESC        0xDB
#define SLIP_ESC_END    0xDC
#define SLIP_ESC_ESC    0xDD


extern BaseSequentialStream debug_print;

/*===========================================================================*/
/* Module data structures and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Module macros.                                                            */
/*===========================================================================*/

/*===========================================================================*/
/* Module inline functions.                                                  */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

void vInitDebugPrint(BaseSequentialStream *output_bss);
bool GenerateSLIP(uint8_t *data, const uint32_t data_count);

#endif
