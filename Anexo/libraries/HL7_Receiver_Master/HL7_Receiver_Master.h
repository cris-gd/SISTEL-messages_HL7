//HL7_Receiver_Master
//Version: 3.0

#ifndef HL7_Receiver_Master_h
#define HL7_Receiver_Master_h

#include "Arduino.h"
#include "WCommMaster.h"

#define HL7_MESSAGE_END_FS      0x1c

#define HL7_FIELD_DELIMITER_DEFAULT_VALUE       '|'
#define HL7_SUBFIELD_DELIMITER_DEFAULT_VALUE    '^'
#define HL7_REPETITION_DELIMITER_DEFAULT_VALUE  '~'
#define HL7_SECOND_DELIMITER_DEFAULT_VALUE      '\\'
#define HL7_THIRD_DELIMITER_DEFAULT_VALUE       '&'

class HL7_Receiver_Master
{
  public:
    HL7_Receiver_Master();
    void Init( WCommMaster *WirelessComm );
	bool readSegment( String *segment );
	void getSegmentType( String *segment, String *type );
	void readField( byte index, String *segment, String *field );
	void readSubfields( String *field, byte *num_subfields, byte *num_subfields_repetition, String subfields[] );
	
  private:
	char _fieldDelimiter;
	char _subfieldDelimiter;
	char _repetitionDelimiter;
	char _secondDelimiter;
	char _thirdDelimiter;
	WCommMaster *_wirelessComm;
	void readSingleSubfield( String *field, String *subfield, bool *repetition, bool *last_subfield_empty );
};

#endif
