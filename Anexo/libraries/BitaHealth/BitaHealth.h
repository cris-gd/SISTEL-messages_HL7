//BitaHealth
//Version: 5.0

#ifndef BitaHealth_h
#define BitaHealth_h

#include <pthread.h>
#include "Arduino.h"
#include "BLEAddress.h"
#include "BLEDevice.h"
#include "BluetoothSerial.h"
#include "SPIFFS.h"

//#define BITAHEALTH_DEBUG
#define BITAHEALTH_INIT_TIMEOUT								10000
#define BITAHEALTH_BITALINO_STATE_IDLE_VALUE				0x00
#define BITAHEALTH_BITALINO_STATE_LIVE_VALUE   				0x01
#define BITAHEALTH_BITALINO_STATE_SIMULATED_VALUE			0x02
#define BITAHEALTH_BITALINO_CONFIG_SAMPLING_RATE_VALUE		0x03

#define BITAHEALTH_BITALINO_TEMP_CONVERSION_A0		1.12764514E-3
#define BITAHEALTH_BITALINO_TEMP_CONVERSION_A1  	2.34282709E-4
#define BITAHEALTH_BITALINO_TEMP_CONVERSION_A2  	8.77303013E-8

#define BITAHEALTH_BITALINO_LEVEL_QUANTIFIED_LEVELS		8

#define BITAHEALTH_BITALINO_WEIGHT_BASE		75
#define BITAHEALTH_BITALINO_WEIGHT_RESOLUTION	1.627

#define BITAHEALTH_BITALINO_GLUCOSE_BASE		80
#define BITAHEALTH_BITALINO_GLUCOSE_RESOLUTION	39.0625

#define BITAHELATH_BITALINO_ACC_CMIN		412
#define BITAHELATH_BITALINO_ACC_CMAX		612

#define TEMPERATURE_RANDOM_MIN		3500
#define TEMPERATURE_RANDOM_MAX		3900

#define WEIGHT_RANDOM_MIN			7000
#define WEIGHT_RANDOM_MAX			7100

#define SPO2_RANDOM_MIN				93
#define SPO2_RANDOM_MAX				100

#define BPM_RANDOM_MIN				30
#define BPM_RANDOM_MAX				130

#define SAMPLING_FREQ	128

typedef enum e_bitahealth_bitalino_state {
  BITAHEALTH_BITALINO_STATE_IDLE,
  BITAHEALTH_BITALINO_STATE_LIVE,
  BITAHEALTH_BITALINO_STATE_SIMULATED
} bitahealth_bitalino_state_t;

typedef enum e_bitahealth_bitalino_channel {
  BITAHEALTH_BITALINO_CHANNEL_A1 = 0x01,
  BITAHEALTH_BITALINO_CHANNEL_A2 = 0x02,
  BITAHEALTH_BITALINO_CHANNEL_A3 = 0x04,
  BITAHEALTH_BITALINO_CHANNEL_A4 = 0x08,
  BITAHEALTH_BITALINO_CHANNEL_A5 = 0x10,
  BITAHEALTH_BITALINO_CHANNEL_A6 = 0x20,
} bitahealth_bitalino_channel_t;

typedef enum e_bitahealth_bitalino_sampling_rate {
  BITAHEALTH_BITALINO_SAMPLING_RATE_1HZ,
  BITAHEALTH_BITALINO_SAMPLING_RATE_10HZ,
  BITAHEALTH_BITALINO_SAMPLING_RATE_100HZ,
  BITAHEALTH_BITALINO_SAMPLING_RATE_1000HZ
} bitahealth_bitalino_sampling_rate_t;

class BitaHealth
{
  public:
    BitaHealth();
    boolean Init( std::string ble_addr );
	boolean Init( std::string ble_addr, std::string bt_src_name );
	void Start( void );
	uint8_t getLevelQuantified( void );
    float getTemperature( void );
	float getTemperatureSim( void );
	float getLight( void );
    float getWeight( void );
	float getWeightSim( void );
    float getGlucose( void );
	void initPulsioximeter( void );
	void initPulsioximeterSim( void );
	void readPulsioximeter( void );
	void readPulsioximeterSim( void );
	uint8_t getBPM( void );
	uint8_t getBPMSim( void );
	uint8_t getOxygenSaturation( void );
	uint8_t getOxygenSaturationSim( void );
	void readBloodPressure( void );
	uint8_t getSystolic( void );
	uint8_t getDiastolic( void );
	float getRespiration( void );
	uint16_t getAccSample( void );
	float getAccGravity( void );
	boolean initEcg( String ecg_name );
	boolean ecgAvailable( void );
	float getEcg( void );
  private:
	boolean BitalinoSendStateCommand( bitahealth_bitalino_state_t state );
    boolean BitalinoSendConfigCommand( bitahealth_bitalino_sampling_rate_t sampling_rate );
    boolean ConnectToServer( void );
};

#endif
