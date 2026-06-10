//WCommMaster
//Version: 2.0

#include "WCommMaster.h"

BluetoothSerial _bt;
bool _bitahealth_started;
static std::string _src_name;
static std::string _dest_name;

WCommMaster::WCommMaster( void )
{
	_bitahealth_started = false;
	_src_name = "";
	_dest_name = "";
}

boolean WCommMaster::Init( std::string src_name, std::string dest_name )
{
	bool ret;
		
	if( !_bitahealth_started )
	{
#ifdef WCOMM_DEBUG
		Serial.println( "WCommMaster::Init()" );
		Serial.println( "BitaHealth library not started" );
		Serial.println( "Begining bluetooth serial..." );
		Serial.print( "Source name: " );
		Serial.println( src_name.c_str() );
#endif
		ret = _bt.begin( src_name.c_str(), true );
		if( !ret )
		{
#ifdef WCOMM_DEBUG
			Serial.println( "WCommMaster::Init()" );
			Serial.println( "Bluetooth serial begin failed" );
			Serial.print( "Source name: " );
			Serial.println( src_name.c_str() );
#endif
			return false;
		}
	}
	
#ifdef WCOMM_DEBUG
	Serial.println( "WCommMaster::Init()" );
	Serial.println( "BitaHealth library started" );
	Serial.println( "Connecting bluetooth serial..." );
	Serial.print( "Destination name: " );
	Serial.println( dest_name.c_str() );
#endif
	ret = _bt.connect( dest_name.c_str() );
	if( !ret )
	{
#ifdef WCOMM_DEBUG
		Serial.println( "WCommMaster::Init()" );
		Serial.println( "Bluetooth serial connect failed" );
		Serial.print( "Destination name: " );
		Serial.println( dest_name.c_str() );
#endif
		return false;
	}
	
	_src_name = src_name;
	_dest_name = dest_name;
	
	return true;
}

void WCommMaster::readData( char *c )
{
  while( _bt.available() == 0 )
  {}

  c[ 0 ] = _bt.read();
}

void WCommMaster::readData( byte str_length, String *str )
{
  byte num_bytes_read;
  char c;

  *str = String( "" );
  if( str_length > 0 )
  {
    num_bytes_read = 0;
    while( num_bytes_read < str_length )
    {
      this->readData( &c );
      *str += String( c );
      num_bytes_read++;
    }
  }
}

void WCommMaster::writeData( char c )
{
  _bt.write( c );
}

void WCommMaster::writeData( byte str_length, String str )
{
  byte num_bytes;

  num_bytes = min( str_length, ( byte )str.length() );
  for( byte i = 0; i < num_bytes; i++ )
  {
    this->writeData( str.charAt( i ) );
  }
}

boolean WCommMaster::Available(void)
{
	if ( _bt.available() > 0 ){
		return true;
	}else{
		return false;
	}
	
}
