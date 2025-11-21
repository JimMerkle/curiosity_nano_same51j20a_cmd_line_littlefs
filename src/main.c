/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes
#include "command_line/command_line.h"
#include "littlefs/lfs_interface.h"


// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

int main ( void )
{
   /* Initialize all modules */
   SYS_Initialize ( NULL );
   SYSTICK_TimerStart();
   //-------------------------------------------------------
   // Before we start TC0, reconfigure the period
   /* Configure timer period */
   TC0_REGS->COUNT32.TC_CC[0U] = 0xFFFFFFFFU;
   /* Clear all interrupt flags */
   TC0_REGS->COUNT32.TC_INTFLAG = (uint8_t)TC_INTFLAG_Msk;
   while((TC0_REGS->COUNT32.TC_SYNCBUSY) != 0U)
   { /* Wait for Write Synchronization */ }
   TC0_TimerStart(); // microsecond counter
   //-------------------------------------------------------

   cl_setup();

   SYSTICK_DelayMs(100);
    
   // Mount LittleFS 
   lfs_init(&lfs, &lfs_cfg);
   


   while ( true )
   {
      /* Maintain state machines of all polled MPLAB Harmony modules. */
      //SYS_Tasks ( ); // Nothing here at this time
      cl_loop();
      LED_PA14_AL_Toggle();
      SYSTICK_DelayMs(50);
#if 0
      // if sercom2 character available, grab it and write it 10 times, else
      // show "hello" message
      const char sercom2_msg[]={"Hello from SERCOM2\n"};
      char buff[16];
      if(SERCOM2_USART_ReadCountGet() > 0) {
         SERCOM2_USART_Read((uint8_t *)buff, 1); // get 1 char
         for(unsigned i=1;i<10;i++) {
            buff[i] = buff[0];
         }
         buff[10] = '\n';
         SERCOM2_USART_Write((uint8_t *)buff, 11 );
      } else {
         SERCOM2_USART_Write((uint8_t *)sercom2_msg, sizeof(sercom2_msg)-1 );
      }
#endif

   }

   /* Execution should not come here during normal operation */

   return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

