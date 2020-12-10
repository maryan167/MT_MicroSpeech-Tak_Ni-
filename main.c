/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the PSoC 6 MCU Hello World Example
*              for ModusToolbox.
*
* Related Document: See Readme.md
*
*******************************************************************************
* (c) 2019-2020, Cypress Semiconductor Corporation. All rights reserved.
*******************************************************************************
* This software, including source code, documentation and related materials
* ("Software"), is owned by Cypress Semiconductor Corporation or one of its
* subsidiaries ("Cypress") and is protected by and subject to worldwide patent
* protection (United States and foreign), United States copyright laws and
* international treaty provisions. Therefore, you may use this Software only
* as provided in the license agreement accompanying the software package from
* which you obtained this Software ("EULA").
*
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software source
* code solely for use in connection with Cypress's integrated circuit products.
* Any reproduction, modification, translation, compilation, or representation
* of this Software except as specified above is prohibited without the express
* written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer of such
* system or application assumes all risk of such use and in doing so agrees to
* indemnify Cypress against all liability.
*******************************************************************************/

#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

#include <stdlib.h>
#include <stdio.h>

#include "led.h"
#include "audio.h"
#include "fft.h"
#include "error.h"
#include "words.h"

void init(int16_t* data);
static  void print_array(int8_t active_button, const int16_t *data, uint16_t frame_num, uint16_t frame_size);

/*******************************************************************************
* Function Name: main
***************************************
* Summary:
* 	This is the main function for CM4 CPU. It does...
*    	1. Initialize the hardware and configure the audio codec.
*    	2. Initialize TF model.
*
*    	Do Forever loop:
*    		3. Check the user button.
*    			If it not pressed the next iteration is started, else
*    				4. Audio recording start within 2 sec.
*    				5. Make FFT.
*    				6. The spectrogram data is checked by NN.
*    				7. The audio is played after this.
*
* Parameters:
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
	int8_t answer[words_count];
	// FFT
	uint16_t frame_size = 128;
	uint16_t frame_num = BUFFER_SIZE/frame_size;

	// Array containing the recorded data (stereo)
	// 	must be two-dimensional.
	int16_t recorded_data[NUM_CHANNELS][BUFFER_SIZE];

	init(recorded_data[0]);

	// Initialize TF model
	setup(frame_num, frame_size/2);

	change_led_duty_cycle(BUTTON3, led[BUTTON3].brightness_passive);
	for(;;){
		/* Check if the User_Button is pressed */
		if (cyhal_gpio_read(CYBSP_USER_BTN) == CYBSP_BTN_PRESSED){
			change_led_duty_cycle(BUTTON0, 100);
			change_led_duty_cycle(BUTTON1, 100);
			change_led_duty_cycle(BUTTON2, 100);
			change_led_duty_cycle(BUTTON3, led[3].brightness_passive);
			change_led_duty_cycle(BUTTON4, 100);

			// To avoid button sound recording.
			Cy_SysLib_Delay(750);

			// Start record
			record_audio(BUTTON4);
//			for (size_t i = 0; i < BUFFER_SIZE; ++i) {
//				recorded_data[0][i] = 123;
//			}
			change_led_duty_cycle(BUTTON4, 100);
			change_led_duty_cycle(BUTTON3, led[3].brightness_passive);

			fft_q15(recorded_data[0], recorded_data[1], frame_num, frame_size);
			//print_array(0, recorded_data[1], frame_num, frame_size);

			check(recorded_data[1], frame_num, frame_size, answer, words_count);

			/*printf("Prediction: [");

			for (size_t i = 0; i < words_count; i++) {
				printf(" %d", answer[i]);
			}

			printf(" ]\n\r");*/

			int c = -1;
			int max = 0;
			int lower = 0;
			for (size_t i = 0; i < words_count; i++) {
				if (answer[i] > 64 && answer[i] > max) {
					c = i;
					max = answer[i];
				} else if (answer[i] < -32) lower++;
			}
			char *word = "Nothing";
			for (size_t i = 0; i < words_count; i++) {
				if (words[i].pos == c) word = words[i].word;
			}

			// printf("%s\n\r", word);

			if (words_count == lower) {
				printf("Silence\n\r");
				change_led_duty_cycle(BUTTON2, led[BUTTON2].brightness_passive);
			} else if (!strcmp(word, "Ni")) {
				printf("Ni\n\r");
				change_led_duty_cycle(BUTTON0, led[BUTTON0].brightness_active);
			} else if (!strcmp(word, "Tak")) {
				printf("Tak\n\r");
				change_led_duty_cycle(BUTTON1, led[BUTTON1].brightness_active);
			} else {
				printf("Inshe\n\r");
				change_led_duty_cycle(BUTTON2, led[BUTTON2].brightness_passive);
			}
			printf("\n\r");

			play_record();
		}
	}
}


/******************************************************************************
 * 	Function name: init
 **************************************
 *	Summary:
 *		Initialize all peripherals
 *
 *	Parameters:
 *		*data      -  array with data that will be print;
 */
void init(int16_t* data){
	// To control the initialize process.
		cy_status status;
	    cy_rslt_t result;

		// Initialize the device and board peripherals
	    result = cybsp_init() ;
	    if (result != CY_RSLT_SUCCESS){
	    	/* Halt the CPU if the device and board peripherals initialization failed */
	    	halt();
	    }


		// Enable global interrupts
	    __enable_irq();

		/* Initialize retarget-io to use the debug UART port
		     * CYBSP_SPI_MISO -> P12_1 -> TX
		     * CYBSP_SPI_MOSI -> P12_0 -> RX */
		result = cy_retarget_io_init(CYBSP_SPI_MISO, CYBSP_SPI_MOSI, CY_RETARGET_IO_BAUDRATE);
		if (result != CY_RSLT_SUCCESS){
				/* Halt the CPU if the Button initialization failed */
				halt_with_error("\tMain -> main() ->"
								"\n\r\t\t\t-> UART initialization failed");
			}


	    // Initialize the TCPWM for PWM
	    initialize_led();


		// Initialize the User Button
	    result = cyhal_gpio_init((cyhal_gpio_t) CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT, CYHAL_GPIO_DRIVE_PULLUP, CYBSP_BTN_OFF);
	    if (result != CY_RSLT_SUCCESS){
			/* Halt the CPU if the Button initialization failed */
			halt_with_error("\tMain -> main() ->"
							"\n\r\t\t\t-> Button initialization failed");
		}
	    cyhal_gpio_enable_event((cyhal_gpio_t) CYBSP_USER_BTN, CYHAL_GPIO_IRQ_BOTH, CYHAL_ISR_PRIORITY_DEFAULT, true);

	    // Initialize peripheral components for audio
	    initialize_audio(data);

		// Initialize CapSense
		initialize_capsense();
}


/******************************************************************************
 * Function Name: UART
 **************************************
 * Summary:
 * 	Function to simplify output.
 * 	Output format:
 * 		1 0 0 3 3 3 0 0 0 0 0 0 0 0 0 0 5 6 1... -> 1 0 2 3 3 3 0 10 5 6 1...
 *
 * 		1	 				-> 1
 * 		0 0 				-> 0 2
 * 		3 3 3 				-> 3 3 3
 * 		0 0 0 0 0 0 0 0 0 0 -> 0 10
 * 		5 6 1				-> 5 6 1
 *
 * Parameters:
 * 	active_button	-	number to choose letter and type of output:
 * 																	-1	- Spectrogram
 * 																	0	- No
 * 																	1	- Yes
 * 	*data			-	array with data that will be print;
 *	frame_num		-	number FFT frames;
 *	frame_size		-	number of sound bits for FFT.
 *
 */
static void print_array(int8_t active_button, const int16_t *data, uint16_t frame_num, uint16_t frame_size){
  static uint16_t counter = 0;
  uint32_t size = frame_num * frame_size;
  char s[10];


  if (0 == active_button)
        printf("SN %d\r\n", counter);
  else
        printf("SY %d\r\n", counter);
  counter++;
  printf("%d %d ", frame_num, frame_size/2);

  uint32_t pos = frame_size/2;
  uint32_t t_pos;
  uint8_t prev_zero = 0;  // Bool
  uint16_t zero_count = 0;
  uint32_t t = 0;
  for (int i=0; i<frame_num; i++){
	  t_pos = pos;
	  for (int j=frame_size/2; j<frame_size; j++){
		  if (0 == data[t_pos]){
			  zero_count++;
			  prev_zero = 1;
		  }
		  else{
			  if (1 == prev_zero){
				  printf("%d ", 0);
				  printf("%u ", zero_count);
				  zero_count = 0;
				  prev_zero = 0;
			  }
			  printf("%d ", data[t_pos]);
		  }
		  t_pos++;
	  }
	  pos += frame_size;
  }
}


/* [] END OF FILE */
