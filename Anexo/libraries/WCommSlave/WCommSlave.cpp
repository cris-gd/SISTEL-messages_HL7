//WCommSlave
//Version: 2.0

#include "WCommSlave.h"

BluetoothSerial _bt;
static std::string _src_name;
static std::string _dest_name;

WCommSlave::WCommSlave( void )
{
	_src_name = "";
	_dest_name = "";
}

boolean WCommSlave::Init( std::string src_name )
{
	bool ret;

	ret = _bt.begin( src_name.c_str(), false );
	if( !ret )
	{
#ifdef WCOMM_DEBUG
		Serial.println( "WCommSlave::Init()" );
		Serial.println( "Bluetooth serial begin failed" );
		Serial.print( "Source name: " );
		Serial.println( src_name.c_str() );
#endif
		return false;
	}
	
	ret = _bt.connect();
	if( !ret )
	{
		ret = _bt.connected( WCOMM_SLAVE_CONNECTION_TIMEOUT );
		if( !ret )
		{
#ifdef WCOMM_DEBUG
			Serial.println( "WCommSlave::Init()" );
			Serial.println( "Bluetooth serial connect failed" );
#endif
			return false;
		}
	}
	
	_src_name = src_name;
	
	return true;
}

void WCommSlave::readData( char *c )
{
  while( _bt.available() == 0 )
  {}

  c[ 0 ] = _bt.read();
}

void WCommSlave::readData( byte str_length, String *str )
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

void WCommSlave::writeData( char c )
{
  _bt.write( c );
}

void WCommSlave::writeData( byte str_length, String str )
{
  byte num_bytes;

  num_bytes = min( str_length, ( byte )str.length() );
  for( byte i = 0; i < num_bytes; i++ )
  {
    this->writeData( str.charAt( i ) );
  }
}

boolean WCommSlave::Available(void)
{
	if ( _bt.available() > 0 ){
		return true;
	}else{
		return false;
	}
	
}

boolean WCommSlave::readDataNonBlocking( char *c )
{
  if( _bt.available() > 0 )
  {
	c[0]=_bt.read();
	return true;
  }
  return false;

}

