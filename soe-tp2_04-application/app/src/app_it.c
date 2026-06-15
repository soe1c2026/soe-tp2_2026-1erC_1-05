/*
 * Copyright (c) 2026 Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @author : Juan Manuel Cruz <jcruz@fi.uba.ar> <jcruz@frba.utn.edu.ar>
 */

/********************** inclusions *******************************************/
/* Project includes */
#include "main.h"

/* Demo includes */
#include "logger.h"
#include "dwt.h"
//Includes para BaseType_t
#include "FreeRTOS.h"
#include "semphr.h"
//Include para la variable h_btn_led_bin_sem
#include "app.h"

/* Application & Tasks includes */
#include "board.h"

/********************** macros and definitions *******************************/

/********************** internal data declaration ****************************/

/********************** internal functions declaration ***********************/

/********************** internal data definition *****************************/

/********************** external data declaration ****************************/

/********************** external functions definition ************************/
void app_it_init(void)
{
	/* Init to be done */
}

void popo(void)
{
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	/* The xHigherPriorityTaskWoken parameter must be initialized to pdFALSE as
	 * it will get set to pdTRUE inside the interrupt safe API function if a
	 * context switch is required. */
	xHigherPriorityTaskWoken = pdFALSE;

	/* Unblock the task by releasing the semaphore. */
	xSemaphoreGiveFromISR(h_btn_led_bin_sem, &xHigherPriorityTaskWoken);

	/* Yield if xHigherPriorityTaskWoken is true.
	 * The actual macro used here is port specific. */
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);

	/* Protect shared resource */
	__asm("CPSID i");	/* disable interrupts */

	__asm("CPSIE i");	/* enable interrupts */
}

/**
  * @brief  EXTI line detection callbacks.
  * @param  GPIO_Pin Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	// Check which version of the gpio triggered this callback
	if (GPIO_Pin == BTN_A_PIN)
	{
		/* Work to be done. */
		HAL_GPIO_TogglePin(LED_A_PORT, LED_A_PIN);
	}
}


/********************** end of file ******************************************/
