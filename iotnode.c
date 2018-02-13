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
#include <stdbool.h>
#include "contiki.h"
#include "gpio.h"
#include "dev/leds.h"
#include "dev/dht22.h"
#include "dev/adc-sensors.h"
#include "dev/button-sensor.h"
#include "dev/zoul-sensors.h"
#include "dev/antenna-sw.h"

/* The following libraries add IP/IPv6 support */
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
/* The simple UDP library API */
#include "simple-udp.h"
/* Library used to read the metadata in the packets */
#include "net/packetbuf.h"
/* And we are including the example.h with the example configuration */
#include "example.h"
/*---------------------------------------------------------------------------*/
/* Enables printing debug output from the IP/IPv6 libraries */
#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"




//thresholds definitions
#define MAX_LUX 200
#define MIN_LUX 50
#define MAX_TMP 24
#define MIN_TMP 20
#define MAX_HUM 60
#define MIN_HUM 31
#define MIN_BAT 300
//this depends on application requirements.
#define LUX_INTERVAL 10 		 // For daylight dependentent apps I'd recommend 5min
#define DHT_INTERVAL 30		 // For indoor apps like fire detection I'd recommend 15min
#define BAT_INTERVAL 120 		 

/*---------------------------------------------------------------------------*/
PROCESS(remote_grove_light_process, "Grove LDR test process");
PROCESS(remote_dht22_process, "DHT22 test");
PROCESS(remote_leds_process, "LED manager");
PROCESS(remote_button_process, "Button");
PROCESS(remote_battery_process, "Battery level test");
PROCESS(udp_process, "UDP  process");
PROCESS(init_comm_process, "Initializes radio and sockets");
PROCESS(blink_process, "External LED");
AUTOSTART_PROCESSES(&init_comm_process,&remote_grove_light_process,&remote_dht22_process,&remote_leds_process,&remote_button_process,&remote_battery_process,&blink_process);
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
static struct uip_udp_conn *client_conn;

/* This is the server IPv6 address */
static uip_ipaddr_t server_ipaddr;

/* Keeps account of the number of messages sent */
static uint16_t counter = 0;
/*---------------------------------------------------------------------------*/
/* Create a structure and pointer to store the data to be sent as payload */
static struct my_msg_t msg;
static struct my_msg_t *msgPtr = &msg;
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
send_packet(void)
{

  counter++;

  msg.id      = 1;
 /* if (msg.light == NULL) { msg.light = 0; }
  if (msg.light > 0) { msg.light = 1; }
  if (msg.avac == NULL) { msg.avac = 0; }
  if (msg.avac > 0) { msg.avac = 1; }*/
  
  	/*char res[20];
	snprintf(res,20,"%d.%d",tmp_last/10, tmp_last % 10);
	printf("D%s",res);
	double t = strtod(res,NULL);
	printf("\nT%d\n",t);*/
  
  int d = cc2538_temp_sensor.value(CC2538_SENSORS_VALUE_TYPE_CONVERTED);
  msg.value1  =  (uint16_t) d;
  msg.value2  =  (uint16_t) tmp_last;
  msg.value3  =  (uint16_t) hum_last;
  msg.value4  =  (uint16_t) lux_last;
  msg.battery =  (uint16_t) batread[0];

/*	printf("%d : %d\n",tmp_last,msg.value2);
	printf("%d : %d\n",hum_last,msg.value3);
	printf("%d : %d\n",lux_last,msg.value4);
printf("%d : %d\n",batread[0],msg.battery);
*/


  /* Print the sensor data */
  printf("\n\n**********************************************************************\n");
  printf("MOTE %d SENDING DATA (x%u):\n", msg.id, counter);
  /*printf("ID: %u, core temp: %u.%u, TMP: %d, HUM: %d, LUX: %d, batt: %u | light: %d, AVAC: %d \n",
          msg.id, d / 1000, d % 1000, msg.value2,  msg.value3,
          msg.value4, msg.battery, msg.light, msg.avac);*/
  

  /* Convert to network byte order as expected by the UDPServer application */
  msg.light = UIP_HTONS(msg.light);
  msg.avac = UIP_HTONS(msg.avac);
  msg.value1  = UIP_HTONS(msg.value1);
  msg.value2  = UIP_HTONS(msg.value2);
  msg.value3  = UIP_HTONS(msg.value3);
  msg.value4  = UIP_HTONS(msg.value4);
  msg.battery = UIP_HTONS(msg.battery);
  
  PRINTF("Send readings to %u'\n",server_ipaddr.u8[sizeof(server_ipaddr.u8) - 1]);   
  printf("**********************************************************************\n\n\n");
  
  uip_udp_packet_sendto(client_conn, msgPtr, sizeof(msg),
                        &server_ipaddr, UIP_HTONS(UDP_SERVER_PORT));
                        
  
                  

}
/*---------------------------------------------------------------------------*/
static void
print_radio_values(void)
{
  radio_value_t aux;
  
  //1dB
  NETSTACK_RADIO.set_value(RADIO_PARAM_TXPOWER, 1);

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
  
  aux = antenna_sw_get();
  if (aux == 0) printf("Using 2.4Ghz antenna"); 
  else printf("Using Sub 1-Ghz antenna");

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
static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  printf("Client IPv6 addresses:\n");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      printf("\n");
      /* hack to make address "final" */
      if (state == ADDR_TENTATIVE) {
        uip_ds6_if.addr_list[i].state = ADDR_PREFERRED;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
/* This is a hack to set ourselves the global address, use for testing */
static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;

/* The choice of server address determines its 6LoWPAN header compression.
 * (Our address will be compressed Mode 3 since it is derived from our link-local address)
 * Obviously the choice made here must also be selected in udp-server.c.
 *
 * For correct Wireshark decoding using a sniffer, add the /64 prefix to the 6LowPAN protocol preferences,
 * e.g. set Context 0 to fd00::.  At present Wireshark copies Context/128 and then overwrites it.
 * (Setting Context 0 to fd00::1111:2222:3333:4444 will report a 16 bit compressed address of fd00::1111:22ff:fe33:xxxx)
 *
 * Note the IPCMV6 checksum verification depends on the correct uncompressed addresses.
 */

  uip_ip6addr(&ipaddr, 0xfd00, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
}
/*---------------------------------------------------------------------------*//*---------------------------------------------------------------------------*/
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
void led_toggle(int status)
{
	msg.light = status;
}
/*---------------------------------------------------------------------------*/
void avac_toggle(int status)
{
     msg.avac = status;
	if (status==1) {
		if (!GPIO_READ_PIN(PORT_BASE, PIN_MASK)) { GPIO_SET_PIN(PORT_BASE, PIN_MASK); }
	} else {
		if (GPIO_READ_PIN(PORT_BASE, PIN_MASK)) { GPIO_CLR_PIN(PORT_BASE, PIN_MASK); }
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
				send_packet();
			}
			printf("LDR = %05u | LUX = %s - ", ldr, res);
			// Checks thresholds, prints and sets the leds
			if (lux<MIN_LUX) { printf("It is dark, turning lights on!\n"); led_toggle(1); leds[0]=1; leds[1]=0; leds[2]=0; if (override) { lchoose=0;}  }
			else if (lux>MAX_LUX) { printf("It is too bright, closing shades!\n"); led_toggle(0); leds[0]=0; leds[1]=0; leds[2]=1; if (override) { lchoose=0;} }
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
			if (abs(temperature-tmp_last)>=1) { 
				tmp_last=temperature; printf("TMP CHANGED\n");
				send_packet();
			}			
			printf("TEMPERATURE: EXT %02d.%02dºC (INT %02d.%02dºC) - ", temperature / 10, temperature % 10, temp / 1000, temp % 1000/10);
			// Checks temperature thresholds, prints and sets the leds
			if (temperature/10>MAX_TMP) { printf("It is too hot, turning fans on!\n"); avac_toggle(1); leds[3]=1; leds[4]=0; leds[5]=0; if (override) { lchoose=3;} }
			else if (temperature/10<MIN_TMP) { printf("It is too cold, turning heat on!\n"); avac_toggle(1); leds[3]=0; leds[4]=0; leds[5]=1; if (override) { lchoose=3;} }
			if (temperature/10<MAX_HUM && temperature/10>MIN_HUM) { printf("OK\n"); avac_toggle(0); leds[3]=0; leds[4]=1; leds[5]=0;} 

			// Checks humidity thresholds, prints and sets the leds
			if (abs(humidity-hum_last)>=1) { 
				hum_last=humidity; printf("HUM CHANGED\n");
				send_packet();
			}			
			printf("HUMIDITY: %02d.%02d RH - ", humidity / 10, humidity % 10);
			if (humidity/10>MAX_HUM) { printf("It is too humide, turning fans on!\n"); avac_toggle(1); leds[6]=1; leds[7]=0; leds[8]=0; if (override) { lchoose=6;} }
			else if (humidity/10<MIN_HUM) { printf("It is too dry, turning mist on!\n"); avac_toggle(1); leds[6]=0; leds[7]=0; leds[8]=1; if (override) { lchoose=6;} }
			if (humidity/10<MAX_HUM && humidity/10>MIN_HUM) { printf("OK\n");  avac_toggle(1);  leds[6]=0; leds[7]=1; leds[8]=0; } 		
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
		send_packet();
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
	  simple_udp_register(&client_conn, UDP_CLIENT_PORT, NULL,
		                 UDP_CLIENT_PORT, receiver);  


 /* Remove the comment to set the global address ourselves, as it is it will
   *obtain the IPv6 prefix from the DODAG root and create its IPv6 global
   * address
   */
  /* set_global_address(); */

	  printf("UDP client process started\n");
  		                 
	  /* Set the server address here */ 
  	  uip_ip6addr(&server_ipaddr, 0xfd00, 0, 0, 0, 0, 0, 0, 1);

	  printf("Server address: ");
	  PRINT6ADDR(&server_ipaddr);
	  printf("\n");

	  /* Print the node's addresses */
	  print_local_addresses();
	  
	  
	  /* Create a new connection with remote host.  When a connection is created
   * with udp_new(), it gets a local port number assigned automatically.
   * The "UIP_HTONS()" macro converts to network byte order.
   * The IP address of the remote host and the pointer to the data are not used
   * so those are set to NULL
   */
  client_conn = udp_new(NULL, UIP_HTONS(UDP_SERVER_PORT), NULL); 

  if(client_conn == NULL) {
    PRINTF("No UDP connection available, exiting the process!\n");
    PROCESS_EXIT();
  }

  /* This function binds a UDP connection to a specified local por */
  udp_bind(client_conn, UIP_HTONS(UDP_CLIENT_PORT)); 

  PRINTF("Created a connection with the server ");
  PRINT6ADDR(&client_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n", UIP_HTONS(client_conn->lport),
                                       UIP_HTONS(client_conn->rport));
                                       
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_process, ev, data)
{
	PROCESS_BEGIN();	
	// takes control over pin C3
	GPIO_SOFTWARE_CONTROL(PORT_BASE, PIN_MASK);

	// start as LOW; 
	GPIO_SET_OUTPUT(PORT_BASE, PIN_MASK);
	GPIO_CLR_PIN(PORT_BASE, PIN_MASK);

	PROCESS_END();
}
/*---------------------------------------------------------------------------

PROCESS_THREAD(udp_process, ev, data)
{
	PROCESS_BEGIN();	
	 	/* Data container used to store the IPv6 addresses */
	    //uip_ipaddr_t addr;
	   // printf("\nSending packet to multicast adddress ");
	    /* Create a link-local multicast address to all nodes */
	   // uip_create_linklocal_allnodes_mcast(&addr);
	   // uip_debug_ipaddr_print(&addr);
	   // printf("\n");
	    /* Take sensor readings and store into the message structure 
	    send_packet();
	    /* Send the multicast packet to all devices
	   
                        
	    //simple_udp_sendto(&mcast_connection, msgPtr, sizeof(msg), &addr);
	PROCESS_END();

}


*/
