//WCommMaster
//Version: 2.0

#ifndef WCommMaster_h
#define WCommMaster_h

#include "Arduino.h"
#include "BluetoothSerial.h"

//#define WCOMM_DEBUG

class WCommMaster
{
  public:
    WCommMaster();
    boolean Init( std::string src_name, std::string dest_name );
    void readData( char *c );
	void readData( byte str_length, String *str );
	void writeData( char c );
	void writeData( byte str_length, String str );
	boolean Available(void);
  private:

};

#endif
