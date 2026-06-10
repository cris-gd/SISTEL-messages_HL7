#include "BitaHealth.h"
#include "WCommMaster.h"
#include <HL7_Receiver_Master.h>
#include <HL7_Sender.h>

#define BT_SENSOR_NAME    "Sensor_05"
#define BT_GATEWAY_NAME   "Gateway_05"
#define BITALINO_ADDR     "88:6B:0F:AB:F7:BA"

WCommMaster WirelessComm;
BitaHealth BitalinoHealth;

HL7_Sender hl7_sender;
HL7_Receiver_Master hl7_receiver;

// Variables para lectura sensores
float temperature;
uint16_t acc_sample;
uint8_t saturation;
uint8_t diastolic;
uint8_t systolic;

// Códigos MLLP
char VT = 0x0B;
char FS = 0x1C;

// Variables crear un mensaje HL7
String HL7_segment;
String str_field;
String str_subfields[10];

// Variables para recepción mensajes del GATEWAY
String receivedMessage;
char receivedChar;
String hl7_message;
bool comunicacion_activa = false;
bool esperando_ack = false;


// Variables para procesar segmento Z recibido por el GATEWAY
String mensaje_pedido;  // ORU_R01/ADT_A05/OMD_O03
String tipo_error_pedido; // NO/200/201/202/203/205
int  num_sensores; // número entre 0 y 4
String tipo_sensores[4]; // lista con posibles términos: tension, temp, po2, acc

// Declaraciones previas de todas las funciones
void processHL7Segments(String hl7_message);
void main_Z_segment_process(String segment);
void sendMessage();
void main_create_MSH_segment();
void main_create_PID_segment();
void main_create_OBR_segment();
void main_create_OBX_ACC_segment();
void main_create_OBX_PO2_segment();
void main_create_OBX_TEMP_segment();
void main_create_OBX_TENSION_segment();
void main_create_PV1_segment();
void main_create_AL1_segment();
void main_create_ORC_segment();
void main_create_TQ1_segment();
void main_create_ODS_segment();
void main_create_NTE_segment();
void main_create_EVN_segment();
void main_create_OBX_segment();

void setup() {
  Serial.begin(115200);

  // Inicializar BITalino module
  Serial.println("Initializing BITalino module...");
  if (!BitalinoHealth.Init(BITALINO_ADDR)) {
    Serial.println("BITalino initialization failed");
    while (1);
  }
  Serial.println("BITalino module initialized");

  BitalinoHealth.Start();
  Serial.println("BITalino acquisition started");

  // Inicializar pulsioximeter sensor
  Serial.println("Initializing pulsioximeter sensor...");
  BitalinoHealth.initPulsioximeter();
  Serial.println("Pulsioximeter sensor initialized");

  // Inicializar Bluetooth communication
  Serial.println("Initializing Bluetooth communication...");
  if (!WirelessComm.Init(BT_SENSOR_NAME, BT_GATEWAY_NAME)) {
    Serial.println("Bluetooth initialization failed");
    Serial.println("Infinite loop...");
    while (1);
  }
  Serial.println("Bluetooth communication initialized");

  // Inicializar HL7 libraries
  Serial.println("Initializing HL7 libraries...");
  hl7_sender.Init();
  hl7_receiver.Init(&WirelessComm);
  Serial.println("HL7 libraries initialized");

  delay(1000);
}

void loop() {
  WirelessComm.readData( &receivedChar );
  receivedMessage = String( receivedChar );

  // Inicio mensaje
  if (receivedMessage == String(VT)) {
    //Serial.println(">> [MLLP] Inicio mensaje HL7 detectado");
    hl7_message = "";
  } 
  
  // Final mensaje
  else if (receivedMessage == String(FS)) {
    //Serial.println(">> [MLLP] Fin de mensaje HL7 detectado (FS)");
    
    //Procesar y verificar el mensaje HL7 proveniente del GATEWAY
    processHL7Segments(hl7_message);

    hl7_message = "";
  }

  // Parte intermedia mensaje
  else {
    // Serial.print( receivedMessage );
    hl7_message += receivedMessage;
  }


  // Lógica envio continuo de datos
  if (!esperando_ack){
    if (comunicacion_activa && mensaje_pedido == "ORU_R01"  && tipo_error_pedido == "NO") {
      for (int i = 0; i < num_sensores; i++) {
        if (tipo_sensores[i] == "po2") {
        // Leer datos del pulsioxímetro
        BitalinoHealth.readPulsioximeter();
        saturation = BitalinoHealth.getOxygenSaturation();
        BitalinoHealth.readBloodPressure();
      } else if (tipo_sensores[i] == "acc") {
        //Leer datos acelerómetro
        acc_sample = BitalinoHealth.getAccSample();
      } else if (tipo_sensores[i] == "temp") {
        // Leer temperatura
        temperature = BitalinoHealth.getTemperature();
      } else if (tipo_sensores[i] == "tension") {
        // Leer tensiómetro
        diastolic = BitalinoHealth.getDiastolic();
        systolic = BitalinoHealth.getSystolic();
      }
    }        
      sendMessage();
      esperando_ack = true;
    } else if (mensaje_pedido == "ORU_R01" && tipo_error_pedido != "NO" && !comunicacion_activa){
        for (int i = 0; i < num_sensores; i++) {
            if (tipo_sensores[i] == "po2") {
              BitalinoHealth.readPulsioximeter();
              saturation = BitalinoHealth.getOxygenSaturation();
              BitalinoHealth.readBloodPressure();
            } else if (tipo_sensores[i] == "acc") {
              acc_sample = BitalinoHealth.getAccSample();
            } else if (tipo_sensores[i] == "temp") {
              temperature = BitalinoHealth.getTemperature();
            } else if (tipo_sensores[i] == "tension") {
              diastolic = BitalinoHealth.getDiastolic();
              systolic = BitalinoHealth.getSystolic();
            }
        }
        sendMessage();
        esperando_ack = true;
        mensaje_pedido = ""; 
    } else if (mensaje_pedido != "" && mensaje_pedido != "ORU_R01" && !comunicacion_activa){
      sendMessage();
      esperando_ack = true;
      mensaje_pedido = ""; 
    }
  }

}


// ----------------------------------------------------
// Procesar segmentos HL7 recibidos
// ----------------------------------------------------
void processHL7Segments(String hl7_message) {
  String segment;
  String segment_type;

  // Dividir el mensaje en segmentos
  int segment_start = 0;
  int segment_end = hl7_message.indexOf('\r', segment_start);

  while (segment_end != -1) {
    segment = hl7_message.substring(segment_start, segment_end);

    if (segment.length() > 0) {
      segment.trim();

      // Obtener el tipo de segmento
      hl7_receiver.getSegmentType(&segment, &segment_type);
      segment_type.trim();

      // Serial.print(">> [HL7] Segmento detectado: ");
      // Serial.println(segment_type);
      // Serial.print(">> [HL7] Contenido: ");
      // Serial.println(segment);

      // Procesar según el tipo de segmento
      if (segment_type == "Z") {
        main_Z_segment_process(segment);
      } else if (segment_type == "MSA") {
        String campo1;
        hl7_receiver.readField(1, &segment, &campo1);  // será AA o AE

        // Serial.print(">> [ACK] Código ACK recibido: ");
        // Serial.println(campo1);

        esperando_ack=false;

        if (campo1 == "AR") {
          Serial.println(">> [ACK] Error detectado. Comunicación detenida");
          comunicacion_activa = false;
        }
        else if (campo1 == "AA") {
          Serial.println(">> [ACK] Correcto");
 
          if (comunicacion_activa == true && mensaje_pedido =="ORU_R01") {
            Serial.println(">> [ACK] Preparando siguiente ORU...");
          } else {
            // Para mensajes no ORU (ADT_A05, OMD_O03, etc.)
            Serial.println(">> [ACK] Mensaje no ORU procesado correctamente. Comunicación detenida");
            comunicacion_activa=false;
          }
        }
      }
    }

    segment_start = segment_end + 1;
    segment_end = hl7_message.indexOf('\r', segment_start);
  }
}

// ----------------------------------------------------
// Procesar segmento Z recibido del GATEWAY
// ----------------------------------------------------
void main_Z_segment_process(String segment) {
  String field;
  String subfields[5]; // Máximo puede leer 4 sensores
  byte num_subfields, num_subfields_repetition;

  Serial.println(">> [Z] Procesando segmento Z");

  // Campo Z-1: Comprobar que es de inicio
  hl7_receiver.readField(1, &segment, &field);
  String tipo_Z = field;

  if (field == "INICIO") {
    Serial.println(">> [Z] Detectado Z INICIO");
    // Campo Z-2: Tipo mensaje a enviar
    hl7_receiver.readField(2, &segment, &field);

    // Procesar subcampos del campo Z-2
    hl7_receiver.readSubfields(&field, &num_subfields, &num_subfields_repetition, subfields);

    // El primer subcampo es el tipo de mensaje a enviar
    if (num_subfields > 0) {
      mensaje_pedido = subfields[0];
      Serial.print(">> [Z INICIO] Tipo de mensaje solicitado: ");
      Serial.println(mensaje_pedido);
    }

    // Procesar sensores a partir del segundo subcampo
    num_sensores = 0;

    for (byte i = 1; i < num_subfields && num_sensores < 4; i++) {
      String codigo_loinc = subfields[i];

      if (codigo_loinc == "80493-0") {
        tipo_sensores[num_sensores] = "acc";
        num_sensores++;
      }
      else if (codigo_loinc == "20564-1") {
        tipo_sensores[num_sensores] = "po2";
        num_sensores++;
      }
      else if (codigo_loinc == "8310-5") {
        tipo_sensores[num_sensores] = "temp";
        num_sensores++;
      }
      else if (codigo_loinc == "55284-4") {
        tipo_sensores[num_sensores] = "tension";
        num_sensores++;
      }
    }

    if (mensaje_pedido =="ORU_R01"){
      // Verificar sensores configurados
      Serial.print(">> [Z INICIO] Número sensores pedido: ");
      Serial.println(num_sensores);
      for (int i = 0; i < num_sensores; i++) {
        Serial.print(">> [Z INICIO] Sensor ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(tipo_sensores[i]);
      }
    }


    // Campo Z-3: Error SI o NO
    hl7_receiver.readField(3, &segment, &field);
    tipo_error_pedido = field;
    Serial.print(">> [Z INICIO] Error pedido: ");
    Serial.println(tipo_error_pedido);

    if (tipo_error_pedido != "NO") {
      // Campo Z-4: Tipo error
      hl7_receiver.readField(4, &segment, &field);
      tipo_error_pedido = field;
      Serial.print(">> [Z INICIO] Tipo error pedido: ");
      Serial.println(tipo_error_pedido);
    }

    // Control sobre si seguir o no enviando mensajes
    if (mensaje_pedido == "ORU_R01" && tipo_error_pedido == "NO") {
      comunicacion_activa = true;
    } else {
      comunicacion_activa = false;
    }

    esperando_ack=false;
    
    Serial.println(">> [Z INICIO] Comenzando a enviar mensajes ...");

  } else if (field == "FINAL") {

    Serial.println(">> [Z] Detectado Z FINAL, envio mensajes parado");

    // Detener comunicación
    comunicacion_activa = false;
    esperando_ack=false;

  }

}

// ----------------------------------------------------
// Enviar mensaje
// ----------------------------------------------------
void sendMessage() {
  Serial.println(">> Mensaje enviado: ");

  // VT
  HL7_segment = String(VT);
  WirelessComm.writeData(HL7_segment.length(), HL7_segment);
  delay(500);

  // Enviar segmento MSH
  main_create_MSH_segment();
  HL7_segment = hl7_sender.getSegment();
  WirelessComm.writeData(HL7_segment.length(), HL7_segment);
  Serial.println(HL7_segment);
  delay(500);

  if (mensaje_pedido == "ADT_A05") {
    // Enviar segmento EVN
    main_create_EVN_segment();
    HL7_segment = hl7_sender.getSegment();
    WirelessComm.writeData(HL7_segment.length(), HL7_segment);
    Serial.println(HL7_segment);
    delay(500);
  }

  // Enviar segmento PID
  main_create_PID_segment();
  HL7_segment = hl7_sender.getSegment();
  WirelessComm.writeData(HL7_segment.length(), HL7_segment);
  Serial.println(HL7_segment);
  delay(500);

  if (mensaje_pedido == "ORU_R01") {
    // Enviar segmento OBR
    main_create_OBR_segment();
    HL7_segment = hl7_sender.getSegment();
    WirelessComm.writeData(HL7_segment.length(), HL7_segment);
    Serial.println(HL7_segment);
    delay(500);

    for (int i = 0; i < num_sensores; i++) {

      if (tipo_sensores[i] == "po2") {
        // Enviar segmento OBX_PO2 (Oximetría)
        main_create_OBX_PO2_segment();
        HL7_segment = hl7_sender.getSegment();
        WirelessComm.writeData(HL7_segment.length(), HL7_segment);
        Serial.println(HL7_segment);
        delay(500);

      } else if (tipo_sensores[i] == "acc") {
        // Enviar segmento OBX_ACC (Aceleración)
        main_create_OBX_ACC_segment();
        HL7_segment = hl7_sender.getSegment();
        WirelessComm.writeData(HL7_segment.length(), HL7_segment);
        Serial.println(HL7_segment);
        delay(500);

      } else if (tipo_sensores[i] == "temp") {
        // Enviar segmento OBX_TEMP (Temperatura)
        main_create_OBX_TEMP_segment();
        HL7_segment = hl7_sender.getSegment();
        WirelessComm.writeData(HL7_segment.length(), HL7_segment);
        Serial.println(HL7_segment);
        delay(500);

      } else if (tipo_sensores[i] == "tension") {
        // Enviar segmento OBX_TENSION (Tensiometro)
        main_create_OBX_TENSION_segment();
        HL7_segment = hl7_sender.getSegment();
        WirelessComm.writeData(HL7_segment.length(), HL7_segment);
        Serial.println(HL7_segment);
        delay(500);
      }
    }
  } else {
    // Enviar segmento PV1
    main_create_PV1_segment();
    HL7_segment = hl7_sender.getSegment();
    WirelessComm.writeData(HL7_segment.length(), HL7_segment);
    Serial.println(HL7_segment);
    delay(500);

    if (mensaje_pedido == "OMD_O03") {
      // Enviar segmento AL1
      main_create_AL1_segment();
      HL7_segment = hl7_sender.getSegment();
      WirelessComm.writeData(HL7_segment.length(), HL7_segment);
      Serial.println(HL7_segment);
      delay(500);

      // Enviar segmento ORC
      main_create_ORC_segment();
      HL7_segment = hl7_sender.getSegment();
      WirelessComm.writeData(HL7_segment.length(), HL7_segment);
      Serial.println(HL7_segment);
      delay(500);

      // Enviar segmento TQ1
      main_create_TQ1_segment();
      HL7_segment = hl7_sender.getSegment();
      WirelessComm.writeData(HL7_segment.length(), HL7_segment);
      Serial.println(HL7_segment);
      delay(500);

      // Enviar segmento ODS
      main_create_ODS_segment();
      HL7_segment = hl7_sender.getSegment();
      WirelessComm.writeData(HL7_segment.length(), HL7_segment);
      Serial.println(HL7_segment);
      delay(500);

      // Enviar segmento NTE
      main_create_NTE_segment();
      HL7_segment = hl7_sender.getSegment();
      WirelessComm.writeData(HL7_segment.length(), HL7_segment);
      Serial.println(HL7_segment);
      delay(500);

    } else if (mensaje_pedido == "ADT_A05") {
      // Enviar segmento OBX
      main_create_OBX_segment();
      HL7_segment = hl7_sender.getSegment();
      WirelessComm.writeData(HL7_segment.length(), HL7_segment);
      Serial.println(HL7_segment);
      delay(500);

      // Enviar segmento AL1
      main_create_AL1_segment();
      HL7_segment = hl7_sender.getSegment();
      WirelessComm.writeData(HL7_segment.length(), HL7_segment);
      Serial.println(HL7_segment);
      delay(500);
    }
  }

  // FS
  HL7_segment = String(FS);
  WirelessComm.writeData(HL7_segment.length(), HL7_segment);
  delay(500);
}

// ----------------------------------------------------
// Crear segmento MSH
// ----------------------------------------------------
void main_create_MSH_segment() {
  hl7_sender.createSegment("MSH");

  str_subfields[0] = "GRUPO_PRACTICAS5";
  str_subfields[1] = "";
  str_subfields[2] = "";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "20251218130550";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);

  if (mensaje_pedido=="ORU_R01"){
    if (tipo_error_pedido == "200") {
        str_subfields[0] = "ORX";
        str_subfields[1] = "R01";
        str_subfields[2] = "ORX_R01";
      } else if (tipo_error_pedido == "201") {
        str_subfields[0] = "ORU";
        str_subfields[1] = "R0X";
        str_subfields[2] = "ORU_R0X";
      } else {
        str_subfields[0] = "ORU";
        str_subfields[1] = "R01";
        str_subfields[2] = "ORU_R01";
      }
    } else if (mensaje_pedido=="OMD_O03"){
      if (tipo_error_pedido == "200") {
        str_subfields[0] = "OMX";
        str_subfields[1] = "O03";
        str_subfields[2] = "OMX_O03";
      } else if (tipo_error_pedido == "201") {
        str_subfields[0] = "OMD";
        str_subfields[1] = "O0X";
        str_subfields[2] = "OMD_O0X";
      } else {
        str_subfields[0] = "OMD";
        str_subfields[1] = "O03";
        str_subfields[2] = "OMD_O03";
      }
    } else if (mensaje_pedido=="ADT_A05"){
      if (tipo_error_pedido == "200") {
        str_subfields[0] = "ADX";
        str_subfields[1] = "A05";
        str_subfields[2] = "ADX_A05";
      } else if (tipo_error_pedido == "201") {
        str_subfields[0] = "ADT";
        str_subfields[1] = "A0X";
        str_subfields[2] = "ADT_A0X";
      } else {
        str_subfields[0] = "ADT";
        str_subfields[1] = "A05";
        str_subfields[2] = "ADT_A05";
      }
    } 

  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);

  str_subfields[0] = "MESSAGE_ID_SENDER_1";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);

  if (tipo_error_pedido == "202") {
    str_subfields[0] = "X";
  } else {
    str_subfields[0] = "T";
  }
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);

  if (tipo_error_pedido == "203") {
    str_subfields[0] = "3.0";
  } else {
    str_subfields[0] = "2.6";
  }
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);

  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "AL";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "AL";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "UPNA";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento PID
// -------------------------------------------------------------------
void main_create_PID_segment() {
  hl7_sender.createSegment("PID");

  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);

  if (tipo_error_pedido == "101") {
    str_field = hl7_sender.createField(0, 0, str_subfields);
    hl7_sender.appendField(str_field);
  } else {
    str_subfields[0] = "ID_1";
    str_subfields[1] = "";
    str_subfields[2] = "";
    str_subfields[3] = "ASS_AUTH";
    str_field = hl7_sender.createField(4, 0, str_subfields);
    hl7_sender.appendField(str_field);
  }

  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "Salas";
  str_subfields[1] = "Paquita";
  str_subfields[2] = "";
  str_subfields[3] = "L";
  str_subfields[4] = "Salitas";
  str_subfields[5] = "Paca";
  str_subfields[6] = "";
  str_subfields[7] = "N";
  str_field = hl7_sender.createField(4, 4, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento OBR
// -------------------------------------------------------------------
void main_create_OBR_segment() {
  hl7_sender.createSegment("OBR");

  str_subfields[0] = "1";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "PLACER";
  str_subfields[1] = "";
  str_subfields[2] = "";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "FILLER";
  str_subfields[1] = "";
  str_subfields[2] = "";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "29274-8";
  str_subfields[1] = "VITAL SIGNS MEASUREMENTS";
  str_subfields[2] = "LN";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento OBX (Acelerómetro)
// -------------------------------------------------------------------
void main_create_OBX_ACC_segment() {
  hl7_sender.createSegment("OBX");

  str_subfields[0] = "1";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "NM";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "80493-0";
  str_subfields[1] = "ACCELERATION MAGNITUDE";
  str_subfields[2] = "LN";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "1";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  
  str_subfields[0] = String(acc_sample);
  
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "m/s2";
  str_subfields[1] = "metro por segundo cuadrado";
  str_subfields[2] = "UCUM";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "R";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}


// -------------------------------------------------------------------
// Crear segmento OBX (Oximetría)
// -------------------------------------------------------------------
void main_create_OBX_PO2_segment() {
  hl7_sender.createSegment("OBX");

  str_subfields[0] = "2";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "NM";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "20564-1";
  str_subfields[1] = "OXYGEN SATURATION IN BLOOD";
  str_subfields[2] = "LN";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "1";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  
  str_subfields[0] = String(saturation);
  
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "%{saturation}/";
  str_subfields[1] = "porcentaje";
  str_subfields[2] = "UCUM";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "R";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento OBX (Temperatura)
// -------------------------------------------------------------------
void main_create_OBX_TEMP_segment() {
  hl7_sender.createSegment("OBX");

  str_subfields[0] = "3";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "NM";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "8310-5";
  str_subfields[1] = "BODY TEMPERATURE";
  str_subfields[2] = "LN";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "1";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  
  str_subfields[0] = String(temperature,1);
  
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "Cel";
  str_subfields[1] = "Grados Celsius";
  str_subfields[2] = "UCUM";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "R";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento OBX (Tensión)
// -------------------------------------------------------------------
void main_create_OBX_TENSION_segment() {
  hl7_sender.createSegment("OBX");

  str_subfields[0] = "4";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "NM";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "55284-4";
  str_subfields[1] = "BLOOD PRESSURE";
  str_subfields[2] = "LN";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "1";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  
  str_subfields[0] = String(systolic) + "/" + String(diastolic);
  
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "mm[Hg]";
  str_subfields[1] = "milímetro de mercurio";
  str_subfields[2] = "UCUM";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "R";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento PV1
// -------------------------------------------------------------------
void main_create_PV1_segment() {
  hl7_sender.createSegment("PV1");

  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "1";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "O";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "URGENCIAS";
  str_subfields[1] = "BOX3";
  str_field = hl7_sender.createField(2, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "1234";
  str_subfields[1] = "MARTINEZ";
  str_subfields[2] = "CARLOS";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento AL1
// -------------------------------------------------------------------
void main_create_AL1_segment() {
  hl7_sender.createSegment("AL1");

  str_subfields[0] = "1";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "FA";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "";
  str_subfields[1] = "Cacahuete";
  str_subfields[2] = "L";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "SV";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "";
  str_subfields[1] = "Anafilaxia";
  str_subfields[2] = "L";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "20251218";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento ORC
// -------------------------------------------------------------------
void main_create_ORC_segment() {
  hl7_sender.createSegment("ORC");

  str_subfields[0] = "NW";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "OrdDiet456";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "";
  str_subfields[1] = "Urgente";
  str_field = hl7_sender.createField(2, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "1234";
  str_subfields[1] = "MARTINEZ";
  str_subfields[2] = "CARLOS";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento TQ1
// -------------------------------------------------------------------
void main_create_TQ1_segment() {
  hl7_sender.createSegment("TQ1");

  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "20251218130550";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "R";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento ODS
// -------------------------------------------------------------------
void main_create_ODS_segment() {
  hl7_sender.createSegment("ODS");

  str_subfields[0] = "D";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "";
  str_subfields[1] = "SIN CACAHUETES";
  str_subfields[2] = "99PAT";
  str_subfields[3] = "";
  str_subfields[4] = "";
  str_subfields[5] = "";
  str_subfields[6] = "";
  str_subfields[7] = "Sin cacahuetes o trazas";
  str_field = hl7_sender.createField(8, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "";
  str_subfields[1] = "ESTRICTO: Alergia severa confirmada";
  str_field = hl7_sender.createField(2, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento NTE
// -------------------------------------------------------------------
void main_create_NTE_segment() {
  hl7_sender.createSegment("NTE");

  str_subfields[0] = "1";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "VERIFICAR ETIQUETAS DE PRODUCTOS PROCESADOS POR POSIBLE CONTAMINACION CRUZADA";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento EVN
// -------------------------------------------------------------------
void main_create_EVN_segment() {
  hl7_sender.createSegment("EVN");

  str_subfields[0] = "A05";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "20251218130550";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}

// -------------------------------------------------------------------
// Crear segmento OBX
// -------------------------------------------------------------------
void main_create_OBX_segment() {
  hl7_sender.createSegment("OBX");

  str_subfields[0] = "1";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "CE";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "11323-3";
  str_subfields[1] = "GRAVEDAD";
  str_subfields[2] = "LN";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "";
  str_subfields[1] = "ALTA";
  str_subfields[2] = "";
  str_field = hl7_sender.createField(3, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_field = hl7_sender.createField(0, 0, str_subfields);
  hl7_sender.appendField(str_field);
  str_subfields[0] = "R";
  str_field = hl7_sender.createField(1, 0, str_subfields);
  hl7_sender.appendField(str_field);

  hl7_sender.endSegment();
}
