//HL7_Receiver_Master
//Version: 3.0

#include "Arduino.h"
#include "WCommMaster.h"
#include "HL7_Receiver_Master.h"

HL7_Receiver_Master::HL7_Receiver_Master( void )
{

}

void HL7_Receiver_Master::Init( WCommMaster *WirelessComm )
{
	_fieldDelimiter = HL7_FIELD_DELIMITER_DEFAULT_VALUE;
	_subfieldDelimiter = HL7_SUBFIELD_DELIMITER_DEFAULT_VALUE;
	_repetitionDelimiter = HL7_REPETITION_DELIMITER_DEFAULT_VALUE;
	_secondDelimiter = HL7_SECOND_DELIMITER_DEFAULT_VALUE;
	_thirdDelimiter = HL7_THIRD_DELIMITER_DEFAULT_VALUE;
	_wirelessComm = WirelessComm;
}

bool HL7_Receiver_Master::readSegment( String *segment )
{
  char current_char;
  bool end_wait;
  bool ret;
  
  ret = false;
  end_wait = false;
  *segment = "";
  while( !end_wait )
  {
	  _wirelessComm->readData( &current_char );
    if( current_char == HL7_MESSAGE_END_FS )
    {
      *segment = "";
      ret = true;
      end_wait = true;
    }
    else if( current_char == '\r' )
    {
    }
    else if( current_char == '\n' )
    {
      end_wait = true;
    }
    else
    {
	  segment->concat( current_char );
    }
  }
  return ret;
}

void HL7_Receiver_Master::getSegmentType( String *segment, String *type )
{
  String str_field;
  
  this->readField( 0, segment, type );
}

void HL7_Receiver_Master::readField( byte index, String *segment, String *field )
{
  byte current_index;
  bool field_found;
  byte field_start_index;
  byte field_delimiter_index;
  
  current_index = 0;
  field_start_index = 0;
  field_found = false;
  while( ( field_start_index < segment->length() ) && ( !field_found ) )
  {
    field_delimiter_index = segment->indexOf( _fieldDelimiter, field_start_index );
    if( field_delimiter_index == 255 )
    {
      *field = segment->substring( field_start_index );
	  field_start_index = segment->length();
      if( current_index == index )
      {
		  field_found = true;
	  }
	  else
	  {
        *field = "";
      }
    }
    else
    {
      *field = segment->substring( field_start_index, field_delimiter_index );
	  field_start_index = field_delimiter_index + 1;
      if( current_index == index )
      {
        field_found = true;
      }
      current_index++;
    }
  }

  if( !field_found )
  {
    *field = "";
  }
}

void HL7_Receiver_Master::readSubfields( String *field, byte *num_subfields, byte *num_subfields_repetition, String subfields[] )
{
  byte current_index;
  bool subfields_repetition_reading;
  bool repetition_aux;
  bool last_subfield_empty_aux;
  
  current_index = 0;
  *num_subfields = 0;
  *num_subfields_repetition = 0;
  subfields_repetition_reading = false;
  repetition_aux = false;
  last_subfield_empty_aux = false;
  while( field->length() > 0 )
  {
	subfields[ current_index ] = "";
    readSingleSubfield( field, subfields + current_index, &repetition_aux, &last_subfield_empty_aux );
    current_index++;
    if( !subfields_repetition_reading )
    {
      if( !repetition_aux )
      {
        (*num_subfields)++;
        if( last_subfield_empty_aux )
        {
          (*num_subfields)++;
        }
      }
      else
      {
        subfields_repetition_reading = true;
        (*num_subfields_repetition)++;
        if( last_subfield_empty_aux )
        {
          (*num_subfields_repetition)++;
        }
      }
    }
    else
    {
      (*num_subfields_repetition)++;
      if( last_subfield_empty_aux )
      {
        (*num_subfields_repetition)++;
      }
    }
  }
}

void HL7_Receiver_Master::readSingleSubfield( String *field, String *subfield, bool *repetition, bool *last_subfield_empty )
{
  byte subfield_delimiter_index;
  
  *subfield = "";
  *repetition = false;
  *last_subfield_empty = false;
  subfield_delimiter_index = field->indexOf( _subfieldDelimiter );
  if( subfield_delimiter_index == 255 )
  {
    *subfield = *field;
    *field = "";
    if( subfield->length() > 0 )
    {
      if( subfield->charAt( 0 ) == _repetitionDelimiter )
      {
        *subfield = subfield->substring( 1 );
        *repetition = true;
      }
    }
  }
  else
  {
    *subfield = field->substring( 0, subfield_delimiter_index );
    *field = field->substring( ++subfield_delimiter_index );
    if( subfield->length() > 0 )
    {
      if( subfield->charAt( 0 ) == _repetitionDelimiter )
      {
        *subfield = subfield->substring( 1 );
        *repetition = true;
      }
    }

    if( field->length() == 0 )
    {
      *last_subfield_empty = true;
    }
  }
}
