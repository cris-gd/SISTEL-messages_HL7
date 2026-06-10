//HL7_Sender
//Version: 3.0

#include "Arduino.h"
#include "HL7_Sender.h"

HL7_Sender::HL7_Sender( void )
{

}

void HL7_Sender::Init( void )
{
	_fieldDelimiter = HL7_FIELD_DELIMITER_DEFAULT_VALUE;
	_subfieldDelimiter = HL7_SUBFIELD_DELIMITER_DEFAULT_VALUE;
	_repetitionDelimiter = HL7_REPETITION_DELIMITER_DEFAULT_VALUE;
	_secondDelimiter = HL7_SECOND_DELIMITER_DEFAULT_VALUE;
	_thirdDelimiter = HL7_THIRD_DELIMITER_DEFAULT_VALUE;
	_txSegment = "";
}

void HL7_Sender::createSegment( String type )
{
  _txSegment = "";
  if( type == "MSH" )
  {
	_txSegment = type + String( _fieldDelimiter );
    _txSegment += String( _subfieldDelimiter );
    _txSegment += String( _repetitionDelimiter );
    _txSegment += String( _secondDelimiter );
    _txSegment += String( _thirdDelimiter );
    _txSegment += String( _fieldDelimiter );
  }
  else
  {
	_txSegment = type + String( _fieldDelimiter );
  }
}

String HL7_Sender::createField( byte num_subfields, byte num_subfields_repetition, String subfields[] )
{
  String field;

  field = "";
  for( byte i = 0; i < num_subfields; i++ )
  {
    field += subfields[ i ];
    if( i < num_subfields - 1 )
    {
      field += _subfieldDelimiter;
    }
  }

  if( num_subfields_repetition > 0 )
  {
    field += _subfieldDelimiter;
    field += _repetitionDelimiter;
    for( byte j = num_subfields; j < ( num_subfields + num_subfields_repetition ); j++ )
    {
      field += subfields[ j ];
      if( j < num_subfields + num_subfields_repetition - 1 )
      {
        field += _subfieldDelimiter;
      }
    }
  }
  
  field += _fieldDelimiter;
  return field;
}

void HL7_Sender::appendField( String field )
{
  _txSegment += field;
}

void HL7_Sender::endSegment( void )
{
  _txSegment += "\r\n";
}

String HL7_Sender::getSegment( void )
{
	return _txSegment;
}
