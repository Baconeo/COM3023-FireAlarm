#include "contiki.h"
#include "lib/random.h"
#include "net/rime.h"
#include "etimer.h"
#include <stdio.h>

#include "dev/light-sensor.h"
#include "dev/sht11-sensor.h"
#include "dev/button-sensor.h"
#include "dev/leds.h"

#define BROADCAST_PORT 1234
#define CHECK_INTERVAL (5 * CLOCK_SECOND)
#define CALIBRATION_INTERVAL (2 * CLOCK_SECOND)
#define BLINK_INTERVAL (0.5 * CLOCK_SECOND)

static struct broadcast_conn broadcast;
static int TEMP_LIMIT;
static int LIGHT_LIMIT;

PROCESS(alarm, "Alarm Process");

PROCESS(calibration, "Calibration Process");
PROCESS(checker, "Checker Process");
AUTOSTART_PROCESSES(&checker, &calibration);

unsigned short d1(float f) {
  return ((unsigned short)f);
}
unsigned short d2(float f) {
  return (1000*(f-d1(f)));
}

unsigned int lux(float lx) {
  return (0.625*1e6*((1.5*lx/4096)/100000)*1000);
}
float lux2(float lx) {
  return (0.625*1e6*((1.5*lx/4096)/100000)*1000);
}

float tempTranslate(float temp) {
	return 0.01 * temp - 39.6;
}

static void calibrate(int cTemps[], int cLights[]) {
	int i=0;
	int tSize=sizeof(cTemps);
	int lSize=sizeof(cLights);
	int temp=0;
	int light=0;

	while(i<tSize) {	
		temp += cTemps[i];
		light += cLights[i];
		i++;
	}

	TEMP_LIMIT = d1(temp/tSize);
	LIGHT_LIMIT = d1(light/lSize);
	printf("CALIBRATION COMPLETED: %ulx %uC\n", LIGHT_LIMIT, TEMP_LIMIT);
}

static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
	unsigned char * data = (char *)packetbuf_dataptr();
	if(data == "ALARM") {
		printf("!! ALARM !!\n");
		printf("EXCEEDED LIMITS AT (%d.%d) \n", from->u8[0], from->u8[1]);
	}

}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

PROCESS_THREAD(checker, ev, data) 
{
	static struct etimer et;
	PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
	PROCESS_BEGIN();
	broadcast_open(&broadcast, 240, &broadcast_call);

	// Initial Calibration
	static int i=0;
	static int temps[10];
	static int lights[10];
	printf("<-- CALIBRATON STARTED -->\n");
	while(i<10) {	
		etimer_set(&et, CALIBRATION_INTERVAL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		SENSORS_ACTIVATE(light_sensor);
		SENSORS_ACTIVATE(sht11_sensor);

		temps[i] = d1(tempTranslate(sht11_sensor.value(SHT11_SENSOR_TEMP)));
		lights[i] = lux(light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC));

		SENSORS_DEACTIVATE(light_sensor);
		SENSORS_DEACTIVATE(sht11_sensor);
		i++;
	}
	calibrate(temps,lights);

	while(1) {
		etimer_set(&et, CHECK_INTERVAL);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

		SENSORS_ACTIVATE(light_sensor);
		SENSORS_ACTIVATE(sht11_sensor);

		unsigned int formattedTemp = d1(tempTranslate(sht11_sensor.value(SHT11_SENSOR_TEMP)));
		unsigned int formattedLux = lux(light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC));

		// Disabling sensors once readings taken to conserve energy
		SENSORS_DEACTIVATE(light_sensor);
		SENSORS_DEACTIVATE(sht11_sensor);

		if (formattedTemp >= TEMP_LIMIT && formattedLux >= LIGHT_LIMIT+100) {
			printf("!! ALARM !!\n");
			printf("!! CURRENT TEMPERATE AT: %uC & LIGHT LEVELS AT: %ulx HAVE EXCEEDED LIMITS !!\n", formattedTemp, formattedLux);
			packetbuf_copyfrom("ALARM",4);

			if(!process_is_running(&alarm)) {
				process_start(&alarm, NULL);
			}
		} else {
			process_exit(&alarm);
			if(leds_get()==4) {
				leds_toggle(LEDS_RED);
			}
		}
	}
	PROCESS_END();
}

PROCESS_THREAD(alarm, ev, data) 
{
	static struct etimer et_blink;
	PROCESS_BEGIN();

	while(1) {
	    leds_toggle(LEDS_RED);
	    etimer_set(&et_blink, BLINK_INTERVAL);
	    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
	}

	return NULL;
	PROCESS_END();
}

PROCESS_THREAD(calibration, ev, data) 
{
	static struct etimer et;
	PROCESS_BEGIN();
	
	while(1) {
		SENSORS_ACTIVATE(button_sensor);
		PROCESS_WAIT_EVENT_UNTIL(data==&button_sensor);

		printf("<-- CALIBRATON STARTED -->\n");
		static int i;
		static int temps[10];
		static int lights[10];
		i=0;

		while(i<10) {	
			etimer_set(&et, CALIBRATION_INTERVAL);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

			SENSORS_ACTIVATE(light_sensor);
			SENSORS_ACTIVATE(sht11_sensor);

			temps[i] = d1(tempTranslate(sht11_sensor.value(SHT11_SENSOR_TEMP)));
			lights[i] = lux(light_sensor.value(LIGHT_SENSOR_PHOTOSYNTHETIC));

			SENSORS_DEACTIVATE(light_sensor);
			SENSORS_DEACTIVATE(sht11_sensor);
			i++;
		}
		calibrate(temps,lights);
	}
	PROCESS_END();
}
