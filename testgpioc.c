/**
* MEIIdC - IIoTSE 2017/18 @ IPT
* Author: Rui Rodrigues / rman2006@hotmail.com
* 
* LAB5: 	Use timer interrupts to toggle led, connected to GPIO, on/off in 5 seconds interval. Toggle Led, connected to GPIO, on/off with a button.
*
* APPROACH:	Make use of a process that calls ISR (interrupt service routine). Use another process to detect button events. 
* 
* IMPROVEMENTS:	Created an extra function to deal with the GPIO SET and CLEAR and also detect which event was called printing it out to console.
*			Button is used either to turn on or off at will without interfering with ISR.
*
* ANNOYANCES:	We don't have an external button to test external interrupts so we made use of internal button events.
*			Button-sensor.c driver DEBOUNCE_DURATION must be set to 0 (Ln 57, Col 44)
*		
*		
**/

/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include "contiki.h"
#include "gpio.h"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "sys/ctimer.h"
/*---------------------------------------------------------------------------*/
#define PORT_BASE	GPIO_PORT_TO_BASE(GPIO_C_NUM)
#define PIN_MASK	GPIO_PIN_MASK(3)

/*---------------------------------------------------------------------------*/
PROCESS(blink_process, "LED blink");
PROCESS(button_process, "Button");

AUTOSTART_PROCESSES(&blink_process,&button_process);
/*---------------------------------------------------------------------------*/
static struct ctimer timer;

//toggles led and prints out events status to console;
void led_toggle(void *data)
{
	//prints out which events is toggling A5
	if (data == &button_sensor) printf("Button event! ");
		else printf("Interrupt event! ");
	//if A5 is high set it to low and vice-versa	
	if (GPIO_READ_PIN(PORT_BASE, PIN_MASK)) {
		GPIO_CLR_PIN(PORT_BASE, PIN_MASK);
		printf("Turning OFF\n"); 
	} else {
	GPIO_SET_PIN(PORT_BASE, PIN_MASK);
		printf("Turning ON\n");
	}
}

static void
callback(void *ptr)
{
	// resets timer so callback function keeps being called
	ctimer_reset(&timer);	
	//toggles led and prints out status to console;
	led_toggle(NULL);
}

void init(void)
{
	// defines ISR
	ctimer_set(&timer, CLOCK_SECOND*5, callback, NULL);
}

PROCESS_THREAD(blink_process, ev, data)
{
	PROCESS_BEGIN();	
	// takes control over pin A5
	GPIO_SOFTWARE_CONTROL(PORT_BASE, PIN_MASK);

	// start as HIGH; how to make it start as low??
	GPIO_SET_OUTPUT(PORT_BASE, PIN_MASK);
	GPIO_SET_PIN(PORT_BASE, PIN_MASK);

	init();
	PROCESS_END();
}

PROCESS_THREAD(button_process, ev, data)
{
	PROCESS_BEGIN();
	//Enables button sensor
	SENSORS_ACTIVATE(button_sensor); 

	while(1){
		//process is suspended until there is an event on the button sensor (ie. button was pressed)
		PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);
		//toggles led and prints out status to console;
		led_toggle(data);
	}
	PROCESS_END();
}

