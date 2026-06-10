//WCommSlave
//Version: 2.0

#ifndef WCommSlave_h
#define WCommSlave_h

#include "Arduino.h"
#include "BluetoothSerial.h"

//#define WCOMM_DEBUG
#define WCOMM_SLAVE_CONNECTION_TIMEOUT		60000

class WCommSlave
{
  public:
    WCommSlave();
	boolean Init( std::string src_name );
    void readData( char *c );
	void readData( byte str_length, String *str );
	void writeData( char c );
	void writeData( byte str_length, String str );
	boolean Available(void);
	boolean readDataNonBlocking( char *c );
  private:

};

#endif
