/**
 * @file Water_Level_Monitoring.ino
 * @author rakwireless.com
 * @brief This sketch demonstrate reading a water level sensor
 *    and send the data to lora gateway.
 * @version 0.1
 * @date 2020-07-28
 * @copyright Copyright (c) 2020
 */
#include <Arduino.h>

#include <LoRaWan-RAK4630.h> //Click here to get the library: http://librarymanager/All#SX126x

#include <SPI.h>

// Check if the board has an LED port defined
#ifndef LED_BUILTIN
#define LED_BUILTIN 35
#endif

#ifndef LED_BUILTIN2
#define LED_BUILTIN2 36
#endif

bool g_doOTAA = true;
#define SCHED_MAX_EVENT_DATA_SIZE APP_TIMER_SCHED_EVENT_DATA_SIZE /**< Maximum size of scheduler events. */
#define SCHED_QUEUE_SIZE 60										  /**< Maximum number of events in the scheduler queue. */

#define LORAWAN_DATERATE DR_0
#define LORAWAN_TX_POWER TX_POWER_0
#define JOINREQ_NBTRIALS 3 /**< Number of trials for the join request. */
DeviceClass_t g_CurrentClass = CLASS_A;
lmh_confirm g_CurrentConfirm = LMH_CONFIRMED_MSG;
uint8_t g_AppPort = LORAWAN_APP_PORT;

/**@brief Structure containing LoRaWan parameters, needed for lmh_init()
*/
static lmh_param_t g_lora_param_init = {
	LORAWAN_ADR_ON,
	LORAWAN_DATERATE,
	LORAWAN_PUBLIC_NETWORK,
	JOINREQ_NBTRIALS,
	LORAWAN_TX_POWER,
	LORAWAN_DUTYCYCLE_OFF
};

// Foward declaration
static void lorawan_has_joined_handler(void);
static void lorawan_rx_handler(lmh_app_data_t *app_data);
static void lorawan_confirm_class_handler(DeviceClass_t Class);
static void send_lora_frame(void);

/**@brief Structure containing LoRaWan callback functions, needed for lmh_init()
*/
static lmh_callback_t g_lora_callbacks = {
	BoardGetBatteryLevel,
	BoardGetUniqueId,
	BoardGetRandomSeed,
	lorawan_rx_handler,
	lorawan_has_joined_handler,
	lorawan_confirm_class_handler
};

//OTAA keys !!! KEYS ARE MSB !!!
uint8_t g_nodeDeviceEUI[8] = {0x67, 0x79, 0x6e, 0xad, 0xf6, 0xab, 0xd2, 0x16};
uint8_t g_nodeAppEUI[8] = {0xB8, 0x27, 0xEB, 0xFF, 0xFE, 0x39, 0x00, 0x00};
uint8_t g_nodeAppKey[16] = {0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77};

// Private defination
#define LORAWAN_APP_DATA_BUFF_SIZE 64										  /**< buffer size of the data to be transmitted. */
#define LORAWAN_APP_INTERVAL 20000											  /**< Defines for user timer, the application data transmission interval. 20s, value in [ms]. */
static uint8_t g_m_lora_app_data_buffer[LORAWAN_APP_DATA_BUFF_SIZE];			  //< Lora user application data buffer.
static lmh_app_data_t g_m_lora_app_data = {g_m_lora_app_data_buffer, 0, 0, 0, 0}; //< Lora user application data structure.

TimerEvent_t g_appTimer;
static uint32_t timers_init(void);

static uint32_t g_count = 0;
static uint32_t g_count_fail = 0;

void setup()
{
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);

	/*
     WisBLOCK 5811 Power On
  */
	pinMode(WB_IO1, OUTPUT);
	digitalWrite(WB_IO1, HIGH);

	pinMode(WB_A1, INPUT_PULLDOWN);
	analogReference(AR_INTERNAL_3_0);
	analogOversampling(128);

	// Initialize LoRa chip.
	lora_rak4630_init();

	// Initialize Serial for debug output
	time_t timeout = millis();
	Serial.begin(115200);
	while (!Serial)
	{
		if ((millis() - timeout) < 5000)
		{
			delay(100);
		}
		else
		{
			break;
		}
	}

	Serial.println("=====================================");
	Serial.println("Welcome to RAK4630 LoRaWan!!!");
	Serial.println("Type: OTAA");

#if defined(REGION_AS923)
	Serial.println("Region: AS923");
#elif defined(REGION_AU915)
	Serial.println("Region: AU915");
#elif defined(REGION_CN470)
	Serial.println("Region: CN470");
#elif defined(REGION_CN779)
	Serial.println("Region: CN779");
#elif defined(REGION_EU433)
	Serial.println("Region: EU433");
#elif defined(REGION_IN865)
	Serial.println("Region: IN865");
#elif defined(REGION_EU868)
	Serial.println("Region: EU868");
#elif defined(REGION_KR920)
	Serial.println("Region: KR920");
#elif defined(REGION_US915)
	Serial.println("Region: US915");
#elif defined(REGION_US915_HYBRID)
	Serial.println("Region: US915_HYBRID");
#else
	Serial.println("Please define a region in the compiler options.");
#endif
	Serial.println("=====================================");

	//creat a user timer to send data to server period
	uint32_t err_code;
	err_code = timers_init();
	if (err_code != 0)
	{
		Serial.printf("timers_init failed - %d\n", err_code);
	}

	// Setup the EUIs and Keys
	lmh_setDevEui(g_nodeDeviceEUI);
	lmh_setAppEui(g_nodeAppEUI);
	lmh_setAppKey(g_nodeAppKey);

	// Initialize LoRaWan
	err_code = lmh_init(&g_lora_callbacks, g_lora_param_init, g_doOTAA);
	if (err_code != 0)
	{
		Serial.printf("lmh_init failed - %d\n", err_code);
	}

	// Start Join procedure
	lmh_join();
}

void loop()
{
	// Handle Radio events
	Radio.IrqProcess();
}

/**@brief LoRa function for handling HasJoined event.
*/
void lorawan_has_joined_handler(void)
{
	Serial.println("OTAA Mode, Network Joined!");

	lmh_error_status ret = lmh_class_request(g_CurrentClass);
	if (ret == LMH_SUCCESS)
	{
		delay(1000);
		TimerSetValue(&g_appTimer, LORAWAN_APP_INTERVAL);
		TimerStart(&g_appTimer);
	}
}

/**@brief Function for handling LoRaWan received data from Gateway

   @param[in] app_data  Pointer to rx data
*/
void lorawan_rx_handler(lmh_app_data_t *app_data)
{
	Serial.printf("LoRa Packet received on port %d, size:%d, rssi:%d, snr:%d, data:%s\n",
				  app_data->port, app_data->buffsize, app_data->rssi, app_data->snr, app_data->buffer);
}

void lorawan_confirm_class_handler(DeviceClass_t Class)
{
	Serial.printf("switch to class %c done\n", "ABC"[Class]);
	// Informs the server that switch has occurred ASAP
	g_m_lora_app_data.buffsize = 0;
	g_m_lora_app_data.port = g_AppPort;
	lmh_send(&g_m_lora_app_data, g_CurrentConfirm);
}

int get_depths(void)
{
	int i;

	int sensor_pin = WB_A1;   // select the input pin for the potentiometer
	int mcu_ain_raw = 0; // variable to store the value coming from the sensor

	int depths; // variable to store the value of oil depths
	int average_raw;
	float voltage_ain, voltage_sensor;

	for (i = 0; i < 5; i++)
	{
		mcu_ain_raw += analogRead(sensor_pin);
	}
	average_raw = mcu_ain_raw / i;

	voltage_ain = average_raw * 3.0 / 1024; //raef 3.0v / 10bit ADC

	voltage_sensor = voltage_ain / 0.6; //WisBlock RAK5811 (0 ~ 5V).   Input signal reduced to 6/10 and output

	depths = (voltage_sensor * 1000 - 574) * 2.5; //Convert to millivolt. 574mv is the default output from sensor

	Serial.printf("-------depths------ = %d mm\n", depths);
	return depths;
}

void send_lora_frame(void)
{
	int depths;

	if (lmh_join_status_get() != LMH_SET)
	{
		//Not joined, try again later
		return;
	}

	depths = get_depths(); //Depth range: (0 ~ 5000mm)

	uint32_t i = 0;

	g_m_lora_app_data.port = g_AppPort;
	g_m_lora_app_data.buffer[i++] = 0x07;
	g_m_lora_app_data.buffer[i++] = (depths >> 8) & 0xFF;
	g_m_lora_app_data.buffer[i++] = depths & 0xFF;
	g_m_lora_app_data.buffsize = i;

	lmh_error_status error = lmh_send(&g_m_lora_app_data, g_CurrentConfirm);
	if (error == LMH_SUCCESS)
	{
		g_count++;
		Serial.printf("lmh_send ok count %d\n", g_count);
	}
	else
	{
		g_count_fail++;
		Serial.printf("lmh_send fail count %d\n", g_count_fail);
	}
}

/**@brief Function for handling user timerout event.
*/
void tx_lora_periodic_handler(void)
{
	TimerSetValue(&g_appTimer, LORAWAN_APP_INTERVAL);
	TimerStart(&g_appTimer);
	Serial.println("Sending frame now...");
	send_lora_frame();
}

/**@brief Function for the Timer initialization.

   @details Initializes the timer module. This creates and starts application timers.
*/
uint32_t timers_init(void)
{
	TimerInit(&g_appTimer, tx_lora_periodic_handler);
	return 0;
}
