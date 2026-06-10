//HL7_Sender
//Version: 3.0

#ifndef HL7_Sender_h
#define HL7_Sender_h

#include "Arduino.h"

#define HL7_SEGMENT_TYPE_UNDEF  0x00
#define HL7_SEGMENT_TYPE_MSH    0x01
#define HL7_SEGMENT_TYPE_PID    0x02
#define HL7_SEGMENT_TYPE_OBR    0x03
#define HL7_SEGMENT_TYPE_OBX    0x04
#define HL7_SEGMENT_TYPE_MSA    0x05
#define HL7_SEGMENT_TYPE_ERR    0x06
#define HL7_SEGMENT_TYPE_PV1	0x07
#define HL7_SEGMENT_TYPE_EVN	0x08
#define HL7_SEGMENT_TYPE_TQ1	0x09
#define HL7_SEGMENT_TYPE_ORC	0x0a

#define HL7_FIELD_DELIMITER_DEFAULT_VALUE       '|'
#define HL7_SUBFIELD_DELIMITER_DEFAULT_VALUE    '^'
#define HL7_REPETITION_DELIMITER_DEFAULT_VALUE  '~'
#define HL7_SECOND_DELIMITER_DEFAULT_VALUE      '\\'
#define HL7_THIRD_DELIMITER_DEFAULT_VALUE       '&'

class HL7_Sender
{
  public:
    HL7_Sender();
    void Init( void );
	void createSegment( String type );
	String createField( byte num_subfields, byte num_subfields_repetition, String subfields[] );
	void appendField( String field );
	void endSegment( void );
	String getSegment( void );
	
  private:
	char _fieldDelimiter;
	char _subfieldDelimiter;
	char _repetitionDelimiter;
	char _secondDelimiter;
	char _thirdDelimiter;
	String _txSegment;
};

#endif
