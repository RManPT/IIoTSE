/**
* MEIIdC - IIoTSE 2017/18 @ IPT
* Author: Rui Rodrigues / rman2006@hotmail.com
* 
* LAB3: 		Get LDR from Groove Light Sensor 1.2 simultaneously with DHT22. Define allarm thresholds and use leds to display allerts.
*			EXTRA:	Convert LDR to LUX
*
* APPROACH:	Made use of four concurrent processes, one for each sensor, one to deal with the leds and another for the button.
*			The sensor data is printed to the console with a comment telling if it is within, bellow or above the threashold.
*			As a nine way color code with one RGB would be quite messy decided to add a button action. This way the user may choose the sensor
*			from which to get visual (coloured) info on. Blue = bellow, Green = OK, Red = above.
*			Did not take to much time bothering with optimizations as that study was done on previous assigments and the focus on this one 
*			was to implement a good funcional solution that would exceed the requirements and to surpass all the challenges.
* 
* IMPROVEMENTS:	Altered CC2538 specific tags so we can add math.h to linkage and use powf, logf, expf etc etc.
*			Implemented a way to printf floats up to 20 digits (or more) as the remote won't do it nativly
*			So that the user may know which sensor is being diplayed (without console) a system was implemented:
*				- LUX, constant color,
*				- TMP, 700ms on and 1s off
*				- HUM, 400ms on and 2s off
			Added CC2538 internal temp sensor 
*
* ANNOYANCES:	LDR reading are misleading and all over the place (malfuction?). Full darkness produces 0 when it should produce MAX.
*			Groove 1.2 sheets states that the sensor has a builtin LM358 chip which reverses the relation between light and output values.
*			adc-sensors.c has 10230 on the formula for LDR where it should be 1023 (corrected) as in http://wiki.seeedstudio.com/wiki/Grove_-_Light_Sensor_v1.2
*		
*		
**/

/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "contiki.h"
#include "dev/leds.h"
#include "dev/dht22.h"
#include "dev/adc-sensors.h"
#include "dev/button-sensor.h"
#include "dev/zoul-sensors.h"

/* The following libraries add IP/IPv6 support */
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
/* The simple UDP library API */
#include "simple-udp.h"
/* Library used to read the metadata in the packets */
#include "net/packetbuf.h"
/* And we are including the example.h with the example configuration */
#include "example.h"





//thresholds definitions
#define MAX_LUX 1000
#define MIN_LUX 100
#define MAX_TMP 24
#define MIN_TMP 20
#define MAX_HUM 60
#define MIN_HUM 31
#define MIN_BAT 300
//this depends on application requirements.
#define LUX_INTERVAL 5 		 // For daylight dependent apps I'd recommend 5min
#define DHT_INTERVAL 15		 // For indoor apps like fire detection I'd recommend 15min
#define BAT_INTERVAL 10 		 

/*---------------------------------------------------------------------------*/
PROCESS(remote_grove_light_process, "Grove LDR test process");
PROCESS(remote_dht22_process, "DHT22 test");
PROCESS(remote_leds_process, "LED manager");
PROCESS(remote_button_process, "Button");
PROCESS(remote_battery_process, "Battery level test");
PROCESS(mcast_process, "UDP multicast process");
PROCESS(init_comm_process, "Initializes radio and sockets");
AUTOSTART_PROCESSES(&remote_grove_light_process,&remote_dht22_process,&remote_leds_process,&remote_button_process,&remote_battery_process,&init_comm_process);
/*---------------------------------------------------------------------------*/
static struct etimer et_ldr;
static struct etimer et_dht;
static struct etimer et_led;
static struct etimer et_bat;

// Vector holding 3 sets of RGB, one for each sensor
static int leds[]={0,1,0,0,1,0,0,1,0,0,0,0};
static int lchoose = 9;
//holds battery values to check if it is charging
static int batread[]={0,0,0,0,0}; 
static int bati=0;
//stores last readings to check if there was any change
static int lux_last=0;
static int tmp_last=0;
static int hum_last=0;
//override color code when values outside threshold
static int override=0;

/*---------------------------------------------------------------------------*/
#define SEND_INTERVAL	(15 * CLOCK_SECOND)
/*---------------------------------------------------------------------------*/
/* The structure used in the Simple UDP library to create an UDP connection */
static struct simple_udp_connection mcast_connection;
/*---------------------------------------------------------------------------*/
/* Create a structure and pointer to store the data to be sent as payload */
static struct my_msg_t msg;
static struct my_msg_t *msgPtr = &msg;
/*---------------------------------------------------------------------------*/
/* Keeps account of the number of messages sent */
static uint16_t counter = 0;
/*---------------------------------------------------------------------------*/


/* This is the receiver callback, we tell the Simple UDP library to call this
 * function each time we receive a packet
 */
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  /* Variable used to store the retrieved radio parameters */
  radio_value_t aux;

  /* Create a pointer to the received data, adjust to the expected structure */
  struct my_msg_t *msgPtr = (struct my_msg_t *) data;

  printf("\n***\nMessage from: ");

  /* Converts to readable IPv6 address */
  uip_debug_ipaddr_print(sender_addr);

  printf("\nData received on port %d from port %d with length %d\n",
         receiver_port, sender_port, datalen);

  /* The following functions are the functions provided by the RF library to
   * retrieve information about the channel number we are on, the RSSI (received
   * strenght signal indication), and LQI (link quality information)
   */

  NETSTACK_RADIO.get_value(RADIO_PARAM_CHANNEL, &aux);
  printf("CH: %u ", (unsigned int) aux);

  aux = packetbuf_attr(PACKETBUF_ATTR_RSSI);
  printf("RSSI: %ddBm ", (int8_t)aux);

  aux = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);
  printf("LQI: %u\n", aux);

  /* Print the received data */
   printf("LUX from the router: %ul\n",
           msgPtr->id);

}
/*---------------------------------------------------------------------------*/
static void
take_readings(void)
{
 uint32_t aux;
  counter++;

  msg.id      = 1;
  msg.counter = counter;

  msg.value1  = (uint16_t) cc2538_temp_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED);
  msg.value2  = tmp_last / 10 + (tmp_last % 10)/10;
  msg.value3  = hum_last / 10 + (hum_last % 10)/10;
  msg.value4  = lux_last;
  msg.battery = (uint16_t) batread[bati];

  /* Print the sensor data */
  printf("\n\n**********************************************************************\n");
  printf("MOTE %d SENDING DATA (x%u):\n", msg.id, msg.counter);
  printf("ID: %u, core temp: %u.%u, TMP: %d, HUM: %d, LUX: %d, batt: %u\n",
          msg.id, msg.value1 / 1000, msg.value1 % 1000, msg.value2, msg.value3,
          msg.value4, msg.battery);
  printf("**********************************************************************\n\n\n");
}

/*---------------------------------------------------------------------------*/
static void
print_radio_values(void)
{
  radio_value_t aux;

  printf("\n* Radio parameters:\n");

  NETSTACK_RADIO.get_value(RADIO_PARAM_CHANNEL, &aux);
  printf("   Channel %u", aux);

  NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MIN, &aux);
  printf(" (Min: %u, ", aux);

  NETSTACK_RADIO.get_value(RADIO_CONST_CHANNEL_MAX, &aux);
  printf("Max: %u)\n", aux);

  NETSTACK_RADIO.get_value(RADIO_PARAM_TXPOWER, &aux);
  printf("   Tx Power %3d dBm", aux);

  NETSTACK_RADIO.get_value(RADIO_CONST_TXPOWER_MIN, &aux);
  printf(" (Min: %3d dBm, ", aux);

  NETSTACK_RADIO.get_value(RADIO_CONST_TXPOWER_MAX, &aux);
  printf("Max: %3d dBm)\n", aux);

  /* This value is set in contiki-conf.h and can be changed */
  printf("   PAN ID: 0x%02X\n", IEEE802154_CONF_PANID);
}
/*---------------------------------------------------------------------------*/
static void
set_radio_default_parameters(void)
{
  NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, EXAMPLE_TX_POWER);
  // NETSTACK_RADIO.set_value(RADIO_PARAM_PAN_ID, EXAMPLE_PANID);
  NETSTACK_RADIO.set_value(RADIO_PARAM_CHANNEL, EXAMPLE_CHANNEL);
}
/*---------------------------------------------------------------------------*/
// reverses a string 'str' of length 'len'
void reverse(char *str, int len)
{
	int i=0, j=len-1, temp;
	while (i<j)
	{
		temp = str[i];
		str[i] = str[j];
		str[j] = temp;
		i++; j--;
	}
}
/*---------------------------------------------------------------------------*/
// Converts a given integer x to string str[].  d is the number
// of digits required in output. If d is more than the number
// of digits in x, then 0s are added at the beginning.
int intToStr(int x, char str[], int d)
{
	int i = 0;
	while (x)
	{
		str[i++] = (x%10) + '0';
		x = x/10;
	}

	// If number of digits required is more, then
	// add 0s at the beginning
	while (i < d)
		str[i++] = '0';

	reverse(str, i);
	str[i] = '\0';
	return i;
}

/*---------------------------------------------------------------------------*/
// Converts a floating point number to string.
void ftoa(float n, char *res, int afterpoint)
{
	// Extract integer part
	int ipart = (int)n;

	// Extract floating part
	float fpart = n - (float)ipart;

	// convert integer part to string
	int i = intToStr(ipart, res, 0);

	// check for display option after point
	if (afterpoint != 0)
	{
		res[i] = '.';  // add dot

		// Get the value of fraction part upto given no.
		// of points after dot. The third parameter is needed
		// to handle cases like 233.007
		fpart = fpart * pow(10, afterpoint);

		intToStr((int)fpart, res + i + 1, afterpoint);
	}
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(remote_grove_light_process, ev, data)
{
	PROCESS_BEGIN();
	uint16_t ldr;
	float lux;

	// using pin 2 so we can use this sensor with the dht22
	adc_sensors.configure(ANALOG_GROVE_LIGHT, 2);

	while(1) {

		etimer_set(&et_ldr, CLOCK_SECOND*LUX_INTERVAL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_ldr));

		//gets values from grove light sensor
		ldr = adc_sensors.value(ANALOG_GROVE_LIGHT);

		if(ldr != ADC_WRAPPER_ERROR) {
			// value = (10230 - (value * 10)) / value;
			// lux= 63*(powf(ldr,-0.7));
			// The given formula was not producing the expected values for lux = 10000 / (R[kΩ]*10)^(4/3)
			// After extensive research and experiments this was the best formula to use
			lux = 10000 / powf(ldr/1000*10,4/3);
			// Required to printf a float on re-mote 
			char res[20];
			ftoa(lux, res, 2);
			//checks if lux changed
			if (abs(lux-lux_last)>=1) { 
				lux_last=lux; printf("LUX CHANGED\n");
				if (!process_is_running(&mcast_process)) { process_start(&mcast_process, "Process start"); }
			}
			printf("LDR = %05u | LUX = %s - ", ldr, res);
			// Checks thresholds, prints and sets the leds
			if (lux<MIN_LUX) { printf("It is dark, turning lights on!\n"); leds[0]=1; leds[1]=0; leds[2]=0; if (override) { lchoose=0;}  }
			else if (lux>MAX_LUX) { printf("It is too bright, closing shades!\n"); leds[0]=0; leds[1]=0; leds[2]=1; if (override) { lchoose=0;} }
				else { printf("OK\n");  leds[0]=0; leds[1]=1; leds[2]=0;} 
		} else {
		 printf("Error, enable the DEBUG flag in adc-wrapper.c for info\n");
		 PROCESS_EXIT();
		}
	}
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(remote_dht22_process, ev, data)
{
    int temperature, humidity, temp;
    PROCESS_BEGIN();
    SENSORS_ACTIVATE(dht22);

    while(1) {
	     etimer_set(&et_dht, CLOCK_SECOND*DHT_INTERVAL);
	     PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_dht));
	     //get values from CC2538 internal temp sensor
		temp = cc2538_temp_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED);
		//gets values from dht22
	     if(dht22_read_all(&temperature, &humidity) != DHT22_ERROR) {
	     	printf("-------------------------------------------------------\n");	     
			if (abs(temperature-tmp_last)>=0.5) { 
				tmp_last=temperature; printf("TMP CHANGED\n");
				if (!process_is_running(&mcast_process)) { process_start(&mcast_process, "Process start"); }
			}
			
			printf("TEMPERATURE: EXT %02d.%02dºC (INT %02d.%02dºC) - ", temperature / 10, temperature % 10, temp / 1000, temp % 1000/10);
			// Checks temperature thresholds, prints and sets the leds
			if (temperature/10>MAX_TMP) { printf("It is too hot, turning fans on!\n"); leds[3]=1; leds[4]=0; leds[5]=0; if (override) { lchoose=3;} }
			else if (temperature/10<MIN_TMP) { printf("It is too cold, turning heat on!\n"); leds[3]=0; leds[4]=0; leds[5]=1; if (override) { lchoose=3;} }
				else { printf("OK\n");   leds[3]=0; leds[4]=1; leds[5]=0;} 
			// Checks humidity thresholds, prints and sets the leds
			if (abs(humidity-hum_last)>=0.5) { 
				hum_last=humidity; printf("HUM CHANGED\n");
				if (!process_is_running(&mcast_process)) { process_start(&mcast_process, "Process start"); }
			}
			
			printf("HUMIDITY: %02d.%02d RH - ", humidity / 10, humidity % 10);
			if (humidity/10>MAX_HUM) { printf("It is too humide, turning fans on!\n"); leds[6]=1; leds[7]=0; leds[8]=0; if (override) { lchoose=6;} }
			else if (humidity/10<MIN_HUM) { printf("It is too dry, turning mist on!\n"); leds[6]=0; leds[7]=0; leds[8]=1; if (override) { lchoose=6;} }
				else { printf("OK\n");   leds[6]=0; leds[7]=1; leds[8]=0; } 		
	     	printf("-------------------------------------------------------\n");
	    } else {
	      printf("Failed to read the sensor\n");
	    }
	}
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(remote_battery_process, ev, data)
{
	PROCESS_BEGIN();

	SENSORS_ACTIVATE(vdd3_sensor);

	while(1) {
		etimer_set(&et_bat, CLOCK_SECOND*BAT_INTERVAL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_bat));
		uint16_t bateria = vdd3_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED);
		printf("Battery: %i mV - ", bateria);
		if (bateria<MIN_BAT) { printf("Low battery, please recharge!\n"); leds[3]=1; leds[4]=0; leds[5]=0; }
		else if (bateria>3215) { printf("Fully charged!\n"); leds[3]=0; leds[4]=0; leds[5]=1; }
			else { printf("OK\n"); leds[3]=0; leds[4]=1; leds[5]=0; }
		if ((batread[4]-batread[0])>3) printf("------------\nCharging...\n------------\n"); 	
		batread[bati]=bateria; 
		if (bati>3) bati=0; else bati++;
	}

	SENSORS_DEACTIVATE(vdd3_sensor);

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/	
PROCESS_THREAD(remote_leds_process, ev, data)
{
	PROCESS_BEGIN();

	while(1) {
	     etimer_set(&et_led, CLOCK_SECOND*(lchoose/3));
	     PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_led));
		int i;
		// Displays leds for selected sensor
		for (i=0+lchoose; i<3+lchoose; i++) {
			int l = i%3+1;
			if (l==3) l++;
			if (leds[i]==0) leds_off(l);
				else leds_on(l); 
		}
	     etimer_set(&et_led, CLOCK_SECOND-CLOCK_SECOND*lchoose/100);
	     PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_led));
	     leds_off(LEDS_ALL);
	}
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(remote_button_process, ev, data)
{
	PROCESS_BEGIN();
	//Enables button sensor
	SENSORS_ACTIVATE(button_sensor); 
	while(1){
		//process is suspended until there is an event on the button sensor (ie. button was pressed)
		PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);
		// On each button click a sensor is selected on the leds vector
		printf("\n\nSwitching display to ");
		if (lchoose==9) lchoose = 0; else lchoose+=3;

		if (lchoose==0) printf("LUX...\n\n");
		else if (lchoose==3) printf("TMP...\n\n");
			else if (lchoose==6) printf("HUM...\n\n");
				else printf("NONE...\n\n");
	printf("Display codes: blue = bellow, green = ok, red = high\n\n");
	}
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(init_comm_process, ev, data)
{
  PROCESS_BEGIN();
	  set_radio_default_parameters();
	  print_radio_values();
	  simple_udp_register(&mcast_connection, UDP_CLIENT_PORT, NULL,
		                 UDP_CLIENT_PORT, receiver);  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mcast_process, ev, data)
{
	PROCESS_BEGIN();	
	 	/* Data container used to store the IPv6 addresses */
	    uip_ipaddr_t addr;
	    printf("\nSending packet to multicast adddress ");
	    /* Create a link-local multicast address to all nodes */
	    uip_create_linklocal_allnodes_mcast(&addr);
	    uip_debug_ipaddr_print(&addr);
	    printf("\n");
	    /* Take sensor readings and store into the message structure */
	    take_readings();
	    /* Send the multicast packet to all devices */
	    simple_udp_sendto(&mcast_connection, msgPtr, sizeof(msg), &addr);
	PROCESS_END();

}



