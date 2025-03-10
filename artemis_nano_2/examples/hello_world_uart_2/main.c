//*****************************************************************************
//
//! @file hello_world_uart_2.c
//!
//! @brief A simple "Hello World" example using the UART peripheral.
//!
//! Purpose: This example prints a "Hello World" message with some device info
//! over UART at 115200 baud. To see the output of this program, run AMFlash,
//! and configure the console for UART. The example sleeps after it is done
//! printing.
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2019, Ambiq Micro
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// 
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
// 
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This is part of revision v2.2.0-7-g63f7c2ba1 of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include "am_mcu_apollo.h"
#include "am_bsp.h"
#include "am_util.h"
#include "lfsr.c"

//*****************************************************************************
//
// Custom data type.
// Note - am_uart_buffer was simply derived from the am_hal_iom_buffer macro.
//
//*****************************************************************************
#define am_uart_buffer(A)                                                   \
union                                                                   \
  {                                                                       \
    uint32_t words[(A + 3) >> 2];                                       \
      uint8_t bytes[A];                                                   \
  }

//*****************************************************************************
//
// Global Variables
//
//*****************************************************************************
am_uart_buffer(1024) g_psWriteData;

volatile uint32_t g_ui32UARTRxIndex = 0;
volatile bool g_bRxTimeoutFlag = false;
volatile bool g_bCmdProcessedFlag = false;
#define STREAM_SIZE 100000
#define BAUD_RATE 1500000

//*****************************************************************************
//
//! @brief UART-based byte sending function
//!
//! This function is used for sending arbitrary bytes via the UART, which for some
//! MCU devices may be multi-module.
//!
//! @return None.
//
//*****************************************************************************
void
am_bsp_uart_send_bytes(void *pHandle, uint8_t *buf, uint32_t ui32numOfBytes)
{

    uint32_t ui32BytesWritten;
    //
    // Print the string via the UART.
    //
    const am_hal_uart_transfer_t sUartWrite =
    {
        .ui32Direction = AM_HAL_UART_WRITE,
        .pui8Data = buf,
        .ui32NumBytes = ui32numOfBytes,
        .ui32TimeoutMs = AM_HAL_UART_WAIT_FOREVER,
        .pui32BytesTransferred = &ui32BytesWritten,
    };

    am_hal_uart_transfer(pHandle, &sUartWrite);

    if (ui32BytesWritten != ui32numOfBytes)
    {
        //
        // Couldn't send the whole string!!
        //
        while(1);
    }
} // am_bsp_uart_string_print()

//*****************************************************************************
//
// UART handles.
//
//*****************************************************************************
void *phUART;
void *phUART1;

#define CHECK_ERRORS(x)                                                       \
    if ((x) != AM_HAL_STATUS_SUCCESS)                                         \
    {                                                                         \
        error_handler(x);                                                     \
    }

volatile uint32_t ui32LastError;

//*****************************************************************************
//
// Catch HAL errors.
//
//*****************************************************************************
void
error_handler(uint32_t ui32ErrorStatus)
{
    ui32LastError = ui32ErrorStatus;

    while (1);
}

//*****************************************************************************
//
// UART buffers.
//
//*****************************************************************************
uint8_t g_pui8TxBuffer[256];
uint8_t g_pui8RxBuffer[256];

uint8_t g_pui8TxBuffer_1[256];
uint8_t g_pui8RxBuffer_1[2];

//*****************************************************************************
//
// UART0 and 1 configuration.
//
//*****************************************************************************
const am_hal_uart_config_t g_sUartConfig =
{
    //
    // Standard UART settings: 115200-8-N-1
    //
    .ui32BaudRate = 115200,
    .ui32DataBits = AM_HAL_UART_DATA_BITS_8,
    .ui32Parity = AM_HAL_UART_PARITY_NONE,
    .ui32StopBits = AM_HAL_UART_ONE_STOP_BIT,
    .ui32FlowControl = AM_HAL_UART_FLOW_CTRL_NONE,

    //
    // Set TX and RX FIFOs to interrupt at half-full.
    //
    .ui32FifoLevels = (AM_HAL_UART_TX_FIFO_1_2 |
                       AM_HAL_UART_RX_FIFO_1_2),

    //
    // Buffers
    //
    .pui8TxBuffer = g_pui8TxBuffer,
    .ui32TxBufferSize = sizeof(g_pui8TxBuffer),
    .pui8RxBuffer = g_pui8RxBuffer,
    .ui32RxBufferSize = sizeof(g_pui8RxBuffer),
};


const am_hal_uart_config_t g_sUartConfig_1 =
{
    //
    // Standard UART settings: 115200-8-N-1
    //
    .ui32BaudRate = BAUD_RATE,
    .ui32DataBits = AM_HAL_UART_DATA_BITS_8,
    .ui32Parity = AM_HAL_UART_PARITY_NONE,
    .ui32StopBits = AM_HAL_UART_ONE_STOP_BIT,
    .ui32FlowControl = AM_HAL_UART_FLOW_CTRL_NONE,

    //
    // Set TX and RX FIFOs to interrupt at half-full.
    //
    .ui32FifoLevels = (AM_HAL_UART_TX_FIFO_1_2 |
                       AM_HAL_UART_RX_FIFO_1_2),

    //
    // Buffers
    //
    .pui8TxBuffer = g_pui8TxBuffer_1,
    .ui32TxBufferSize = sizeof(g_pui8TxBuffer_1),
    .pui8RxBuffer = g_pui8RxBuffer_1,
    .ui32RxBufferSize = sizeof(g_pui8RxBuffer_1),
};

//*****************************************************************************
//
// Handle incoming bytes
//
//*****************************************************************************

void processPackets(uint8_t *pBuf, uint32_t len) {

    uint32_t ui32BytesWritten;

    
    const am_hal_uart_transfer_t sUartWrite =
    {
        .ui32Direction = AM_HAL_UART_WRITE,
        .pui8Data = pBuf,
        .ui32NumBytes = len,
        .ui32TimeoutMs = 0,
        .pui32BytesTransferred = &ui32BytesWritten,
    };
    CHECK_ERRORS(am_hal_uart_transfer(phUART1, &sUartWrite));

    am_hal_uart_tx_flush(phUART1);

    g_ui32UARTRxIndex = 0;

}

//*****************************************************************************
//
// UART0 interrupt handler.
//
//*****************************************************************************
// void
// am_uart_isr(void)
// {
//     //
//     // Service the FIFOs as necessary, and clear the interrupts.
//     //
//     uint32_t ui32Status, ui32Idle;
//     am_hal_uart_interrupt_status_get(phUART, &ui32Status, true);
//     am_hal_uart_interrupt_clear(phUART, ui32Status);
//     am_hal_uart_interrupt_service(phUART, ui32Status, &ui32Idle);
// }

void
am_uart1_isr(void)
{
    //
    // Service the FIFOs as necessary, and clear the interrupts.
    //
    uint32_t ui32Status, ui32Idle;
    am_hal_uart_interrupt_status_get(phUART1, &ui32Status, true);
    am_hal_uart_interrupt_clear(phUART1, ui32Status);
    am_hal_uart_interrupt_service(phUART1, ui32Status, &ui32Idle);
}

//*****************************************************************************
//
// UART1 interrupt handler.
//
//*****************************************************************************
void 
am_uart_isr(void)
{
  uint32_t ui32Status;
  uint8_t * pData = (uint8_t *) &(g_psWriteData.bytes[g_ui32UARTRxIndex]);

  //
  // Read the masked interrupt status from the UART.
  //
  am_hal_uart_interrupt_status_get(phUART, &ui32Status, true);
  am_hal_uart_interrupt_clear(phUART, ui32Status);
  am_hal_uart_interrupt_service(phUART, ui32Status, 0);

  //
  // If there's an RX interrupt, handle it in a way that preserves the
  // timeout interrupt on gaps between packets.
  //
  if (ui32Status & (AM_HAL_UART_INT_RX_TMOUT | AM_HAL_UART_INT_RX))
  {
    uint32_t ui32BytesRead;

    am_hal_uart_transfer_t sRead =
    {
      .ui32Direction = AM_HAL_UART_READ,
      .pui8Data = (uint8_t *) &(g_psWriteData.bytes[g_ui32UARTRxIndex]),
      .ui32NumBytes = 16,
      .ui32TimeoutMs = 0,
      .pui32BytesTransferred = &ui32BytesRead,
    };

    am_hal_uart_transfer(phUART, &sRead);
    g_ui32UARTRxIndex += ui32BytesRead;
    processPackets(pData, ui32BytesRead);
  }
}

//*****************************************************************************
//
// UART print string
//
//*****************************************************************************
void
uart_print(char *pcStr)
{
    uint32_t ui32StrLen = 0;
    uint32_t ui32BytesWritten = 0;

    //
    // Measure the length of the string.
    //
    while (pcStr[ui32StrLen] != 0)
    {
        ui32StrLen++;
    }

    //
    // Print the string via the UART.
    //
    const am_hal_uart_transfer_t sUartWrite =
    {
        .ui32Direction = AM_HAL_UART_WRITE,
        .pui8Data = (uint8_t *) pcStr,
        .ui32NumBytes = ui32StrLen,
        .ui32TimeoutMs = 0,
        .pui32BytesTransferred = &ui32BytesWritten,
    };

    CHECK_ERRORS(am_hal_uart_transfer(phUART, &sUartWrite));

    if (ui32BytesWritten != ui32StrLen)
    {
        //
        // Couldn't send the whole string!!
        //
        while(1);
    }
}

void
uart1_print(char *pcStr)
{
    uint32_t ui32StrLen = 0;
    uint32_t ui32BytesWritten = 0;

    //
    // Measure the length of the string.
    //
    while (pcStr[ui32StrLen] != 0)
    {
        ui32StrLen++;
    }

    //
    // Print the string via the UART.
    //
    const am_hal_uart_transfer_t sUartWrite =
    {
        .ui32Direction = AM_HAL_UART_WRITE,
        .pui8Data = (uint8_t *) pcStr,
        .ui32NumBytes = ui32StrLen,
        .ui32TimeoutMs = 0,
        .pui32BytesTransferred = &ui32BytesWritten,
    };

    CHECK_ERRORS(am_hal_uart_transfer(phUART1, &sUartWrite));

    if (ui32BytesWritten != ui32StrLen)
    {
        //
        // Couldn't send the whole string!!
        //
        while(1);
    }
}


//*****************************************************************************
//
// Main
//
//*****************************************************************************
int
main(void)
{
    //
    // Set the clock frequency.
    //
    am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX, 0);

    //
    // Set the default cache configuration
    //
    am_hal_cachectrl_config(&am_hal_cachectrl_defaults);
    am_hal_cachectrl_enable();

    //
    // Configure the board for low power operation.
    //
    am_bsp_low_power_init();

    //
    // Initialize the printf interface for UART output.
    //
    CHECK_ERRORS(am_hal_uart_initialize(0, &phUART));
    CHECK_ERRORS(am_hal_uart_power_control(phUART, AM_HAL_SYSCTRL_WAKE, false));
    CHECK_ERRORS(am_hal_uart_configure(phUART, &g_sUartConfig));

    CHECK_ERRORS(am_hal_uart_initialize(1, &phUART1));
    CHECK_ERRORS(am_hal_uart_power_control(phUART1, AM_HAL_SYSCTRL_WAKE, false));
    CHECK_ERRORS(am_hal_uart_configure(phUART1, &g_sUartConfig_1));


    //
    // Enable the UART pins.
    //
    am_hal_gpio_pinconfig(AM_BSP_GPIO_COM_UART_TX, g_AM_BSP_GPIO_COM_UART_TX);
    am_hal_gpio_pinconfig(AM_BSP_GPIO_COM_UART_RX, g_AM_BSP_GPIO_COM_UART_RX);

    am_hal_gpio_pinconfig(AM_BSP_GPIO_UART1_TX_39, g_AM_BSP_GPIO_UART1_TX_39);
    am_hal_gpio_pinconfig(AM_BSP_GPIO_UART1_RX_40, g_AM_BSP_GPIO_UART1_RX_40);
     am_hal_gpio_pinconfig(AM_HAL_PIN_38_M3MOSI, g_AM_HAL_GPIO_OUTPUT);

    //
    // Enable interrupts.
    //
    NVIC_EnableIRQ((IRQn_Type)(UART0_IRQn + AM_BSP_UART_PRINT_INST));
    NVIC_EnableIRQ((IRQn_Type)(UART1_IRQn + AM_BSP_UART_PRINT_INST)); //why + 0 ???

    am_hal_interrupt_master_enable();

    //
    // Set the main print interface to use the UART print function we defined.
    //
    am_util_stdio_printf_init(uart_print);

    //
    // Print the banner.
    //
    am_util_stdio_terminal_clear();
    am_util_stdio_printf("Hello World 2!\n\n");
    am_util_stdio_printf("Desired Baud Rate: %d, Actual Baud Rate %d\n", BAUD_RATE, ((am_hal_uart_state_t *) phUART1)->ui32BaudRate);

    
    //
    // Print the compiler version.
    //
    am_hal_uart_tx_flush(phUART);


    am_util_stdio_printf("App Compiler:    %s\n", COMPILER_VERSION);
#ifdef AM_PART_APOLLO3
    am_util_stdio_printf("HAL Compiler:    %s\n", g_ui8HALcompiler);
    am_util_stdio_printf("HAL SDK version: %d.%d.%d\n",
                         g_ui32HALversion.s.Major,
                         g_ui32HALversion.s.Minor,
                         g_ui32HALversion.s.Revision);
    am_util_stdio_printf("HAL compiled with %s-style registers\n",
                         g_ui32HALversion.s.bAMREGS ? "AM_REG" : "CMSIS");

    am_hal_security_info_t secInfo;
    char sINFO[32];
    uint32_t ui32Status;
    ui32Status = am_hal_security_get_info(&secInfo);
    if (ui32Status == AM_HAL_STATUS_SUCCESS)
    {
        if ( secInfo.bInfo0Valid )
        {
            am_util_stdio_sprintf(sINFO, "INFO0 valid, ver 0x%X", secInfo.info0Version);
        }
        else
        {
            am_util_stdio_sprintf(sINFO, "INFO0 invalid");
        }

        am_util_stdio_printf("SBL ver: 0x%x - 0x%x, %s\n",
            secInfo.sblVersion, secInfo.sblVersionAddInfo, sINFO);
    }
    else
    {
        am_util_stdio_printf("am_hal_security_get_info failed 0x%X\n", ui32Status);
    }
#endif // AM_PART_APOLLO3

    //
    // We are done printing.
    // Disable the UART and interrupts
    //
    am_hal_uart_tx_flush(phUART);


    am_util_stdio_printf_init(uart1_print);
    uint32_t loop_count = 0;

    while(true) {
        loop_count++;
        am_util_delay_ms(2000);
        lfsr = PRBS_IV;

    #ifdef AM_BSP_NUM_LEDS
        uint32_t ux;
        uint32_t ui32GPIONumber;
        for (ux = 0; ux < AM_BSP_NUM_LEDS; ux++) {
            ui32GPIONumber = am_bsp_psLEDs[ux].ui32GPIONumber;
            am_hal_gpio_pinconfig(ui32GPIONumber, g_AM_HAL_GPIO_OUTPUT);
            am_devices_led_on(am_bsp_psLEDs, ux);
        }
            am_hal_gpio_state_write(AM_HAL_PIN_38_M3MOSI, AM_HAL_GPIO_OUTPUT_SET);
    #endif // AM_BSP_NUM_LEDS

        uint8_t PREAMBLE[4] = {0x00, 0x00, 0x00, 0x00}; 
    
        am_bsp_uart_send_bytes(phUART1, PREAMBLE, 4);

        am_hal_uart_tx_flush(phUART1);
        

        uint8_t STREAM_LEN[4];
        STREAM_LEN[0] = (STREAM_SIZE >> 24) & 0xFF;
        STREAM_LEN[1] = (STREAM_SIZE >> 16) & 0xFF;
        STREAM_LEN[2] = (STREAM_SIZE >> 8) & 0xFF;
        STREAM_LEN[3] = STREAM_SIZE & 0xFF;

        am_bsp_uart_send_bytes(phUART1, STREAM_LEN, 4);
        am_hal_uart_tx_flush(phUART1);
        am_util_delay_ms(2000);


    #ifdef AM_BSP_NUM_LEDS
        for (ux = 0; ux < AM_BSP_NUM_LEDS; ux++) {
            am_devices_led_off(am_bsp_psLEDs, ux);
        }
        am_hal_gpio_state_write(AM_HAL_PIN_38_M3MOSI, AM_HAL_GPIO_OUTPUT_CLEAR);

    #endif // AM_BSP_NUM_LEDS
        
        uint32_t uy;
        for (uy = 0; uy <= STREAM_SIZE; uy++) {
            // am_util_stdio_printf("\tHello to the other side!"); 
            uint32_t output = prbs();
            uint8_t value[4];

            value[0] = (output >> 24) & 0xFF;
            value[1] = (output >> 16) & 0xFF;
            value[2] = (output >> 8) & 0xFF;
            value[3] = output & 0xFF;


            am_bsp_uart_send_bytes(phUART1, value, 4); 
            
            // send random bit sequence
            // send a number seq or alternating 1/0s
            // have a sort of preamble and control bits
            // then followed by payload
            // then followed by CRC
            // - something to write about under communication protocols
            // - how do we quantify the number of errors? bit-error rate?
            // - measuring bit-error rate (how does it vary with wire length?)
            // - power consumption
            

            // am_hal_uart_tx_flush(phUART1);
        }


    #ifdef AM_BSP_NUM_LEDS
        for (ux = 0; ux < AM_BSP_NUM_LEDS; ux++) {
            am_devices_led_on(am_bsp_psLEDs, ux);
        }
        am_hal_gpio_state_write(AM_HAL_PIN_38_M3MOSI, AM_HAL_GPIO_OUTPUT_SET);

    #endif // AM_BSP_NUM_LEDS

        am_util_stdio_printf_init(uart_print);
        am_util_stdio_printf("%d Done...\n", loop_count);
        am_hal_uart_tx_flush(phUART);
    }


    CHECK_ERRORS(am_hal_uart_power_control(phUART, AM_HAL_SYSCTRL_DEEPSLEEP, false));
    CHECK_ERRORS(am_hal_uart_power_control(phUART1, AM_HAL_SYSCTRL_DEEPSLEEP, false));


    //
    // Loop forever while sleeping.
    //
    while (1)
    {
        //
        // Go to Deep Sleep.
        //
        am_hal_sysctrl_sleep(AM_HAL_SYSCTRL_SLEEP_DEEP);
    }
}
