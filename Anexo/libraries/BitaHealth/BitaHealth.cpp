//BitaHealth
//Version: 5.0

#include "BitaHealth.h"

extern bool _bitahealth_started;
extern BluetoothSerial _bt;
static BLEUUID _serviceUUID( "c566488a-0882-4e1b-a6d0-0b717e652234" );
static BLEUUID _charUUID( "4051eb11-bf0a-4c74-8730-a48f4193fcea" );
static BLEUUID _charUUID1( "40fdba6b-672e-47c4-808a-e529adff3633" );
static boolean _connected;
static std::string _remoteBLEAddress;
static BLEAdvertisedDevice* _myDevice;
static boolean _doConnect;
static boolean _doScan;
static byte _bitalino_data[ 8 ];
static BLERemoteCharacteristic* _pRemoteCharacteristic;
static BLERemoteCharacteristic* _pRemoteCharacteristic1;
static std::string _BTName;
static byte _bitalino_channels;
static byte _pulse;
static byte _oxygen;
static byte _systolic;
static byte _diastolic;
static String _ecg_name;
static boolean _ecg_available;
static String _ecg;
static File _ecg_file;
boolean FileReadSample( File file, String *sample );

class MyClientCallback : public BLEClientCallbacks
{
  void onConnect( BLEClient* pclient )
  {}

  void onDisconnect( BLEClient* pclient )
  {
    _connected = false;
#ifdef BITAHEALTH_DEBUG
    Serial.println( "onDisconnect" );
#endif
  }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks
{
  void onResult( BLEAdvertisedDevice advertisedDevice )
  {
#ifdef BITAHEALTH_DEBUG
    Serial.print( "BLE Advertised Device found: " );
    Serial.println( advertisedDevice.toString().c_str() );
#endif

    // We have found a device, let us now see if it contains the service we are looking for.
    if( advertisedDevice.getAddress().equals( BLEAddress( _remoteBLEAddress ) ) )
    {
#ifdef BITAHEALTH_DEBUG
      Serial.println( "BLE addresses matched" );
#endif
      if( advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService( _serviceUUID ) )
      {
        BLEDevice::getScan()->stop();
        _myDevice = new BLEAdvertisedDevice( advertisedDevice );
        _doConnect = true;
        _doScan = true;
      } 
    }
  }
};

static void notifyCallback( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify )
{
#ifdef BITAHEALTH_DEBUG
	Serial.print( "Notify callback for characteristic " );
    Serial.print( pBLERemoteCharacteristic->getUUID().toString().c_str() );
    Serial.print( " of data length " );
    Serial.println( length );
    Serial.print( "data: " );
    Serial.println( ( char* )pData );
#endif
    _bitalino_data[ 0 ] = pData[ 0 ];
    _bitalino_data[ 1 ] = pData[ 1 ];
    _bitalino_data[ 2 ] = pData[ 2 ];
    _bitalino_data[ 3 ] = pData[ 3 ];
    _bitalino_data[ 4 ] = pData[ 4 ];
    _bitalino_data[ 5 ] = pData[ 5 ];
    _bitalino_data[ 6 ] = pData[ 6 ];
    _bitalino_data[ 7 ] = pData[ 7 ];
    
#ifdef BITAHEALTH_DEBUG
    Serial.println( pData[ 0 ], HEX );
    Serial.println( pData[ 1 ], HEX );
    Serial.println( pData[ 2 ], HEX );
    Serial.println( pData[ 3 ], HEX );
    Serial.println( pData[ 4 ], HEX );
    Serial.println( pData[ 5 ], HEX );
    Serial.println( pData[ 6 ], HEX );
    Serial.println( pData[ 7 ], HEX );
#endif
}

void *printThreadId( void *threadid )
{
  while( 1 )
  {
    if( FileReadSample( _ecg_file, &_ecg ) )
    {
      //ECG file is finished
      //Close file and re-open it for sample reading
      _ecg_file.close();
      _ecg_file = SPIFFS.open( _ecg_name );
      if( !_ecg_file )
      {
        Serial.print( _ecg_name );
        Serial.println( " open file failed!" );
      }
    }

    _ecg_available = true;
    delay( 1000 / SAMPLING_FREQ );
  }
}

BitaHealth::BitaHealth( void )
{
	_doConnect = false;
	_connected = false;
	_doScan = false;
	_pRemoteCharacteristic = NULL;
	_pRemoteCharacteristic1 = NULL;
	_myDevice = NULL;
	_remoteBLEAddress = "";
	_BTName = "";
	_bitalino_channels = 0;
	_bitalino_data[ 0 ] = 0;
	_bitalino_data[ 1 ] = 0;
	_bitalino_data[ 2 ] = 0;
	_bitalino_data[ 3 ] = 0;
	_bitalino_data[ 4 ] = 0;
	_bitalino_data[ 5 ] = 0;
	_bitalino_data[ 6 ] = 0;
	_bitalino_data[ 7 ] = 0;
	_pulse = 0;
	_oxygen = 0;
	_systolic = 0;
	_diastolic = 0;
	_ecg_name = "";
	_ecg_available = false;
	_ecg = "";
}

boolean BitaHealth::Init( std::string ble_addr )
{
	uint32_t timeout_reference;
	bool init_finished;
  
	_remoteBLEAddress = ble_addr;
	_bitalino_channels = BITAHEALTH_BITALINO_CHANNEL_A1;
	_bitalino_channels |= BITAHEALTH_BITALINO_CHANNEL_A2;
	_bitalino_channels |= BITAHEALTH_BITALINO_CHANNEL_A3;
	_bitalino_channels |= BITAHEALTH_BITALINO_CHANNEL_A4;
	_bitalino_channels |= BITAHEALTH_BITALINO_CHANNEL_A5;
	_bitalino_channels |= BITAHEALTH_BITALINO_CHANNEL_A6;

#ifdef BITAHEALTH_DEBUG
	Serial.println( "BitaHealth_init()" );
	Serial.print( "Remote BLE address to connect: " );
	Serial.println( _remoteBLEAddress.c_str() );
	Serial.print( "Bitalino channels to acquire: " );
	Serial.println( _bitalino_channels, HEX );
#endif
	BLEDevice::init( "" );

	// Retrieve a Scanner and set the callback we want to use to be informed when we
	// have detected a new device. Specify that we want active scanning and start the
	// scan to run for 5 seconds.
	BLEScan* pBLEScan = BLEDevice::getScan();
	pBLEScan->setAdvertisedDeviceCallbacks( new MyAdvertisedDeviceCallbacks() );
	pBLEScan->setInterval( 1349 );
	pBLEScan->setWindow( 449 );
	pBLEScan->setActiveScan( true );
	pBLEScan->start( 5, false );

	init_finished = false;
	timeout_reference = millis();
	while( !init_finished && ( millis() - timeout_reference < BITAHEALTH_INIT_TIMEOUT ) )
	{
		if( _doConnect == true )
		{
#ifdef BITAHEALTH_DEBUG
			Serial.println( "BitaHealth_init()" );
			Serial.println( "_doConnect = true" );
#endif
			_doConnect = false;
			if( this->ConnectToServer() )
			{
#ifdef BITAHEALTH_DEBUG
				Serial.println( "BitaHealth_init()" );
				Serial.println( "init_finished = true" );
#endif
				init_finished = true;
			}
		}

		delay( 10 );
	}

	if( !init_finished )
	{
		return false;
	}

	if( !this->BitalinoSendStateCommand( BITAHEALTH_BITALINO_STATE_IDLE ) )
	{
#ifdef BITAHEALTH_DEBUG
		Serial.println( "BitaHealth_init()" );
		Serial.println( "bitalino send state command failed" );
		Serial.println( "state: idle" );
#endif
	}

	if( !this->BitalinoSendConfigCommand( BITAHEALTH_BITALINO_SAMPLING_RATE_10HZ ) )
	{
#ifdef BITAHEALTH_DEBUG
		Serial.println( "BitaHealth_init()" );
		Serial.println( "bitalino send config command failed" );
		Serial.println( "sampling rate: 100Hz" );
#endif
	}
  
	return true;
}

boolean BitaHealth::Init( std::string ble_addr, std::string bt_src_name )
{
	uint32_t timeout_reference;
	bool init_finished;
	bool ret;
  
	_remoteBLEAddress = ble_addr;
	_bitalino_channels = BITAHEALTH_BITALINO_CHANNEL_A1;
	_bitalino_channels |= BITAHEALTH_BITALINO_CHANNEL_A2;
	_bitalino_channels |= BITAHEALTH_BITALINO_CHANNEL_A3;
	_bitalino_channels |= BITAHEALTH_BITALINO_CHANNEL_A4;
	_bitalino_channels |= BITAHEALTH_BITALINO_CHANNEL_A5;
	_bitalino_channels |= BITAHEALTH_BITALINO_CHANNEL_A6;

#ifdef BITAHEALTH_DEBUG
	Serial.println( "BitaHealth_init()" );
	Serial.print( "Remote BLE address to connect: " );
	Serial.println( _remoteBLEAddress.c_str() );
	Serial.print( "Bitalino channels to acquire: " );
	Serial.println( _bitalino_channels, HEX );
#endif
	BLEDevice::init( "" );
	ret = _bt.begin( bt_src_name.c_str(), true );
	if( !ret )
	{
#ifdef BITAHEALTH_DEBUG
		Serial.println( "BitaHealth_init()" );
		Serial.println( "Begin bluetooth failed" );
		Serial.print( "Source name: " );
		Serial.println( bt_src_name.c_str() );
#endif
		return false;
	}
	
	_BTName = bt_src_name;
	_bitahealth_started = true;

	// Retrieve a Scanner and set the callback we want to use to be informed when we
	// have detected a new device. Specify that we want active scanning and start the
	// scan to run for 5 seconds.
	BLEScan* pBLEScan = BLEDevice::getScan();
	pBLEScan->setAdvertisedDeviceCallbacks( new MyAdvertisedDeviceCallbacks() );
	pBLEScan->setInterval( 1349 );
	pBLEScan->setWindow( 449 );
	pBLEScan->setActiveScan( true );
	pBLEScan->start( 5, false );

	init_finished = false;
	timeout_reference = millis();
	while( !init_finished && ( millis() - timeout_reference < BITAHEALTH_INIT_TIMEOUT ) )
	{
		if( _doConnect == true )
		{
#ifdef BITAHEALTH_DEBUG
			Serial.println( "BitaHealth_init()" );
			Serial.println( "_doConnect = true" );
#endif
			_doConnect = false;
			if( this->ConnectToServer() )
			{
#ifdef BITAHEALTH_DEBUG
				Serial.println( "BitaHealth_init()" );
				Serial.println( "init_finished = true" );
#endif
				init_finished = true;
			}
		}

		delay( 10 );
	}

	if( !init_finished )
	{
		return false;
	}

	if( !this->BitalinoSendStateCommand( BITAHEALTH_BITALINO_STATE_IDLE ) )
	{
#ifdef BITAHEALTH_DEBUG
		Serial.println( "BitaHealth_init()" );
		Serial.println( "bitalino send state command failed" );
		Serial.println( "state: idle" );
#endif
	}

	if( !this->BitalinoSendConfigCommand( BITAHEALTH_BITALINO_SAMPLING_RATE_10HZ ) )
	{
#ifdef BITAHEALTH_DEBUG
		Serial.println( "BitaHealth_init()" );
		Serial.println( "bitalino send config command failed" );
		Serial.println( "sampling rate: 100Hz" );
#endif
	}
  
	return true;
}

void BitaHealth::Start( void )
{
	if( !this->BitalinoSendStateCommand( BITAHEALTH_BITALINO_STATE_LIVE ) )
	{
#ifdef BITAHEALTH_DEBUG
		Serial.println( "BitaHealth::Start()" );
		Serial.println( "bitalino send state command failed" );
		Serial.println( "state: live" );
#endif
	}
}

boolean BitaHealth::BitalinoSendStateCommand( bitahealth_bitalino_state_t state )
{
	byte command;
	
	switch( state )
	{
		case BITAHEALTH_BITALINO_STATE_IDLE:
		command = 0x00;
		command |= BITAHEALTH_BITALINO_STATE_IDLE_VALUE;
		break;
		case BITAHEALTH_BITALINO_STATE_LIVE:
		command = _bitalino_channels;
		command <<= 0x02;
		command &= 0xfc;
		command |= BITAHEALTH_BITALINO_STATE_LIVE_VALUE;
		break;
		case BITAHEALTH_BITALINO_STATE_SIMULATED:
		command = _bitalino_channels;
		command <<= 0x02;
		command &= 0xfc;
		command |= BITAHEALTH_BITALINO_STATE_SIMULATED_VALUE;
		break;
		default:
#ifdef BITAHEALTH_DEBUG
		Serial.println( "BitaHealth_prvBitalinoSendStateCommand()" );
		Serial.println( "default state" );
#endif
		command = 0x00;
		return false;
		break;
	}

#ifdef BITAHEALTH_DEBUG
	Serial.println( "BitaHealth_prvBitalinoSendStateCommand()" );
	Serial.print( "command: " );
	Serial.println( command, HEX );
#endif
	_pRemoteCharacteristic->writeValue( command, 1 );
	return true;
}

boolean BitaHealth::BitalinoSendConfigCommand( bitahealth_bitalino_sampling_rate_t sampling_rate )
{
	byte command;
	
	command = 0x00;
	command |= sampling_rate;
	command <<= 0x06;
	command &= 0xc0;
	command |= BITAHEALTH_BITALINO_CONFIG_SAMPLING_RATE_VALUE;
  
#ifdef BITAHEALTH_DEBUG
	Serial.println( "BitaHealth_prvBitalinoSendConfigCommand()" );
	Serial.print( "command: " );
	Serial.println( command, HEX );
#endif
	_pRemoteCharacteristic->writeValue( command, 1 );
	return true;
}

boolean BitaHealth::ConnectToServer( void )
{
#ifdef BITAHEALTH_DEBUG
	Serial.print( "Forming a connection to " );
    Serial.println( _myDevice->getAddress().toString().c_str() );
#endif    
    BLEClient* pClient = BLEDevice::createClient();

#ifdef BITAHEALTH_DEBUG
    Serial.println( " - Created client" );
#endif
    pClient->setClientCallbacks( new MyClientCallback() );

    // Connect to the remove BLE Server.
    pClient->connect( _myDevice );  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
#ifdef BITAHEALTH_DEBUG
    Serial.println( " - Connected to server" );
#endif
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService( _serviceUUID );
    if( pRemoteService == nullptr )
    {
#ifdef BITAHEALTH_DEBUG
		Serial.print( "Failed to find our service UUID: " );
		Serial.println( _serviceUUID.toString().c_str() );
#endif
		pClient->disconnect();
		return false;
    }
#ifdef BITAHEALTH_DEBUG
    Serial.println( " - Found our service" );
#endif

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    _pRemoteCharacteristic = pRemoteService->getCharacteristic( _charUUID );
    if( _pRemoteCharacteristic == nullptr )
    {
#ifdef BITAHEALTH_DEBUG
		Serial.print( "Failed to find our characteristic UUID: " );
		Serial.println( _charUUID.toString().c_str() );
#endif
		pClient->disconnect();
		return false;
    }
#ifdef BITAHEALTH_DEBUG
    Serial.println( " - Found our characteristic" );
#endif

    // Read the value of the characteristic.
    if( _pRemoteCharacteristic->canRead() )
    {
		std::string value = _pRemoteCharacteristic->readValue();
#ifdef BITAHEALTH_DEBUG
		Serial.print( "The characteristic value was: " );
		Serial.println( value.c_str() );
#endif
    }

    if( _pRemoteCharacteristic->canNotify() )
    {
#ifdef BITAHEALTH_DEBUG
		Serial.println( "The characteristic can notify" );
#endif
		_pRemoteCharacteristic->registerForNotify( notifyCallback );
    }

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    _pRemoteCharacteristic1 = pRemoteService->getCharacteristic( _charUUID1 );
    if( _pRemoteCharacteristic1 == nullptr )
    {
#ifdef BITAHEALTH_DEBUG
		Serial.print( "Failed to find our characteristic UUID1: " );
		Serial.println( _charUUID1.toString().c_str() );
#endif
		pClient->disconnect();
		return false;
    }
#ifdef BITAHEALTH_DEBUG
    Serial.println( " - Found our characteristic" );
#endif

    // Read the value of the characteristic.
    if( _pRemoteCharacteristic1->canRead() )
    {
		std::string value = _pRemoteCharacteristic1->readValue();
#ifdef BITAHEALTH_DEBUG
		Serial.print( "The characteristic value was: " );
		Serial.println( value.c_str() );
#endif
    }

    if( _pRemoteCharacteristic1->canNotify() )
    {
#ifdef BITAHEALTH_DEBUG
		Serial.println( "The characteristic can notify" );
#endif
		_pRemoteCharacteristic1->registerForNotify( notifyCallback );
    }

    _connected = true;
    return true;
}

float BitaHealth::getTemperature( void )
{
	uint16_t sample;
	float value;
	float ntcv;
	float ntcr;
	float log_ntc;
	float pow_log_ntc;
	
	sample = _bitalino_data[ 6 ];
	sample = sample << 8;
	sample = sample | _bitalino_data[ 5 ];
	sample = sample >> 2;
	sample = sample & 0x03ff;

	ntcv = ( float )sample * 3.3 / 1024.0;
	ntcr = ( ntcv * 10000.0 ) / ( 3.3 - ntcv );  
	log_ntc = log( ntcr );
	pow_log_ntc = pow( log_ntc, 3 );
	value = 1.0 / ( BITAHEALTH_BITALINO_TEMP_CONVERSION_A0 + ( BITAHEALTH_BITALINO_TEMP_CONVERSION_A1 * log_ntc ) + ( BITAHEALTH_BITALINO_TEMP_CONVERSION_A2 * pow_log_ntc ) );
	value -= 273.15;

#ifdef BITAHEALTH_DEBUG
	Serial.println( "BitaHealth::getTemperature()" );
	Serial.print( "sample: " );
	Serial.println( sample );
	Serial.print( "temperature: " );
	Serial.println( value );
#endif
	return value;
}

float BitaHealth::getTemperatureSim( void )
{
	float Temperature;
	
	Temperature = ( float )random( TEMPERATURE_RANDOM_MIN, TEMPERATURE_RANDOM_MAX + 1 );
	Temperature = Temperature / 100;
	
	return Temperature;
}

float BitaHealth::getLight( void )
{
	uint16_t sample;
	float value;
	
    sample = _bitalino_data[ 5 ];
    sample = sample << 8;
    sample = sample | _bitalino_data[ 4 ];
    sample = sample & 0x03ff;
	
	value = ( ( float )sample / 1024.0 ) * 100.0;
	
#ifdef BITAHEALTH_DEBUG
	Serial.println( "BitaHealth::getLight()" );
	Serial.print( "sample: " );
	Serial.println( sample );
	Serial.print( "light: " );
	Serial.println( value );
#endif
	return value;
}

uint8_t BitaHealth::getLevelQuantified( void )
{
	uint16_t sample;
	float value;
	float delta;
	uint8_t level;
	
	sample = _bitalino_data[ 1 ];
	sample = sample << 8;
	sample = sample | _bitalino_data[ 0 ];
	sample = sample >> 6;
	sample = sample & 0x003f;

	value = ( float )sample * 3.3 / 64.0;
	delta = 3.3 / ( float )BITAHEALTH_BITALINO_LEVEL_QUANTIFIED_LEVELS;
	for( uint8_t i = 0; i < BITAHEALTH_BITALINO_LEVEL_QUANTIFIED_LEVELS; i++ )
	{
		if( ( ( ( float )i * delta ) <= value ) && ( value < ( ( float )( i + 1 ) * delta ) ) )
		{
			level = i;
			break;
		}
	}
	
#ifdef BITAHEALTH_DEBUG
	Serial.println( "BitaHealth::getPotQuantified()" );
	Serial.print( "sample: " );
	Serial.println( sample );
	Serial.print( "level: " );
	Serial.println( level );
#endif
	return level;
}

float BitaHealth::getWeight( void )
{
	uint16_t sample;
	float value;
	
	sample = _bitalino_data[ 1 ];
	sample = sample << 8;
	sample = sample | _bitalino_data[ 0 ];
	sample = sample >> 6;
	sample = sample & 0x003f;

	value = BITAHEALTH_BITALINO_WEIGHT_BASE + ( ( float )sample * 3.3 / 64.0 ) * BITAHEALTH_BITALINO_WEIGHT_RESOLUTION ;

#ifdef BITAHEALTH_DEBUG
	Serial.println( "BitaHealth::getWeight()" );
	Serial.print( "sample: " );
	Serial.println( sample );
	Serial.print( "weight: " );
	Serial.println( value );
#endif
	return value;
}

float BitaHealth::getWeightSim( void )
{
	float Weight;
		
	Weight = ( float )random( WEIGHT_RANDOM_MIN, WEIGHT_RANDOM_MAX + 1 );
	Weight = Weight / 100;
		
	return Weight;
}

float BitaHealth::getGlucose( void )
{
	uint16_t sample;
	float value;
	
	sample = _bitalino_data[ 0 ];
	sample = sample & 0x003f;

	value = BITAHEALTH_BITALINO_GLUCOSE_BASE + ( ( float )sample * 3.3 / 64.0 ) * BITAHEALTH_BITALINO_GLUCOSE_RESOLUTION ;

#ifdef BITAHEALTH_DEBUG
	Serial.println( "BitaHealth::getGlucose()" );
	Serial.print( "sample: " );
	Serial.println( sample );
	Serial.print( "glucose: " );
	Serial.println( value );
#endif
	return value;
}

void BitaHealth::initPulsioximeter( void )
{
  
}

void BitaHealth::initPulsioximeterSim( void )
{
  
}

boolean BitaHealth::initEcg( String ecg_name )
{
  pthread_t thread;
  int returnValue;
  
  if( !SPIFFS.begin( true ) )
  {
    return false;
  }

  _ecg_file = SPIFFS.open( ecg_name );
  if( !_ecg_file )
  {
    return false;
  }

  _ecg_name = ecg_name;
  returnValue = pthread_create( &thread, NULL, printThreadId, (void *)0 );
  if( returnValue )
  {
    return false;
  }

  return true;
}

boolean BitaHealth::ecgAvailable( void )
{
  return _ecg_available;
}

float BitaHealth::getEcg( void )
{
	_ecg_available = false;
	return _ecg.toFloat();
}



boolean FileReadSample( File file, String *sample )
{
  char c_prev;
  char c_curr;
  bool line_detected;

  *sample = String( "" );
  line_detected = false;
  c_prev = c_curr;
  while( file.available() && !line_detected )
  {
    c_curr = file.read();
    if( c_curr != '\r' && c_curr != '\n' )
    {
      *sample += String( c_curr );
    }

    if( c_prev == '\r' && c_curr =='\n' )
    {
      line_detected = true;
    }
    c_prev = c_curr;
  }

  if( !line_detected )
  {
    return true;
  }

  return false;
}

void BitaHealth::readPulsioximeter( void )
{
  uint16_t sample1;
  uint16_t sample2;
    
  sample1 = _bitalino_data[ 5 ];
  sample1 = sample1 << 8;
  sample1 = sample1 | _bitalino_data[ 4 ];
  sample1 = sample1 & 0x03ff;

  sample2 = _bitalino_data[ 3 ];
  sample2 = sample2 << 8;
  sample2 = sample2 | _bitalino_data[ 2 ];
  sample2 = sample2 >> 6;
  sample2 = sample2 & 0x03ff;

  _pulse =  ( 0.25 * ( float )sample1 ) - 0.8;
  _oxygen = ( 0.25 * ( float )sample2 ) - 0.8;

#ifdef BITAHEALTH_DEBUG
  Serial.println( "BitaHealth::readPulsioximeter()" );
  Serial.print( "sample1: " );
  Serial.println( sample1 );
  Serial.print( "pulse: " );
  Serial.println( _pulse );
  Serial.print( "sample2: " );
  Serial.println( sample2 );
  Serial.print( "oxygen: " );
  Serial.println( _oxygen );
#endif
}

void BitaHealth::readPulsioximeterSim( void )
{

}

uint8_t BitaHealth::getBPM( void )
{
  return _pulse;
}

uint8_t BitaHealth::getBPMSim( void )
{
	uint8_t bpm;
	
	bpm = ( uint8_t )random( BPM_RANDOM_MIN, BPM_RANDOM_MAX + 1 );
	return bpm;
}

uint8_t BitaHealth::getOxygenSaturation( void )
{
  return _oxygen;
}

uint8_t BitaHealth::getOxygenSaturationSim( void )
{
	uint8_t spo2;
	
	spo2 = ( uint8_t )random( SPO2_RANDOM_MIN, SPO2_RANDOM_MAX + 1 );
	return spo2;
}

void BitaHealth::readBloodPressure( void )
{
  uint16_t sample1;
  uint16_t sample2;
    
  sample1 = _bitalino_data[ 5 ];
  sample1 = sample1 << 8;
  sample1 = sample1 | _bitalino_data[ 4 ];
  sample1 = sample1 & 0x03ff;

  sample2 = _bitalino_data[ 3 ];
  sample2 = sample2 << 8;
  sample2 = sample2 | _bitalino_data[ 2 ];
  sample2 = sample2 >> 6;
  sample2 = sample2 & 0x03ff;

  _diastolic =  ( 0.25 * ( float )sample1 ) - 0.8;
  _systolic = ( 0.25 * ( float )sample2 ) - 0.8;

#ifdef BITAHEALTH_DEBUG
  Serial.println( "BitaHealth::readBloodPressure()" );
  Serial.print( "sample1: " );
  Serial.println( sample1 );
  Serial.print( "diastolic: " );
  Serial.println( _diastolic );
  Serial.print( "sample2: " );
  Serial.println( sample2 );
  Serial.print( "systolic: " );
  Serial.println( _systolic );
#endif
}

uint8_t BitaHealth::getSystolic( void )
{
  return _systolic;
}

uint8_t BitaHealth::getDiastolic( void )
{
  return _diastolic;
}

float BitaHealth::getRespiration( void )
{
  uint16_t sample;
  float value;
    
  sample = _bitalino_data[ 2 ];
  sample = sample << 8;
  sample = sample | _bitalino_data[ 1 ];
  sample = sample >> 4;
  sample = sample & 0x03ff;

  value = ( ( ( float )sample / 1024.0 ) - 0.5 ) * 100.0;
    
#ifdef BITAHEALTH_DEBUG
  Serial.println( "BitaHealth::getRespiration()" );
  Serial.print( "sample: " );
  Serial.println( sample );
  Serial.print( "respiration: " );
  Serial.println( value );
#endif
  return value;
}

uint16_t BitaHealth::getAccSample( void )
{
	uint16_t sample;
	
	sample = _bitalino_data[ 6 ];
	sample = sample << 8;
	sample = sample | _bitalino_data[ 5 ];
	sample = sample >> 2;
	sample = sample & 0x03ff;

#ifdef BITAHEALTH_DEBUG
	Serial.println( "BitaHealth::getAccSample()" );
	Serial.print( "sample: " );
	Serial.println( sample );
#endif
	return sample;
}

float BitaHealth::getAccGravity( void )
{
	uint16_t sample;
	float gravity;
	
	sample = _bitalino_data[ 6 ];
	sample = sample << 8;
	sample = sample | _bitalino_data[ 5 ];
	sample = sample >> 2;
	sample = sample & 0x03ff;
	
	  gravity = ( ( ( ( float )sample - ( float )BITAHELATH_BITALINO_ACC_CMIN ) / ( ( float )BITAHELATH_BITALINO_ACC_CMAX - ( float )BITAHELATH_BITALINO_ACC_CMIN ) ) * 2.0 ) - 1.0;

#ifdef BITAHEALTH_DEBUG
	Serial.println( "BitaHealth::getAccGravity()" );
	Serial.print( "sample: " );
	Serial.println( sample );
	Serial.print( "gravity: " );
	Serial.println( gravity );
#endif
	return gravity;
}