
#include "fapi/sys_services.h"
#include "fapi/drv_gpio.h"
#include "fapi/drv_uart.h"
#include "stdio.h"


int main()
{
    FAPI_GPIO_SetPinFunction( 33, 23 + 0x0080 /*0x97*/ );
#if 0
    FAPI_GPIO_SetPinFunction( 32, 21 + 0x0100 /*0x115*/ );
#endif
    FAPI_UART_SetBaudrate( 0, 115200 );
    FAPI_UART_SetDataBits( 0, 8 );
    FAPI_UART_SetStopBits( 0, 1 );
    FAPI_UART_SetParity( 0, 0 );
    FAPI_UART_SetFlowControl( 0, 0 );
    FAPI_UART_SetStdioBlockIndex( 0 );

    RTOS_InitServices();

    FAPI_GPIO_Init();

    FAPI_UART_Init();

    printf("\nHello World\n");

    exit(0);
}

