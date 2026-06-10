#include "WCommSlave.h"

#define BT_GATEWAY_NAME "Gateway_05"

WCommSlave WirelessComm;

// Códigos MLLP
const char VT = 0x0B;   // start block
const char FS = 0x1C;   // end block
const char CR = 0x0D;   // carriage return

// Estados y buffers
bool receivingPC = false;
String pcBuffer = "";

bool receivingSensor = false;
String sensorBuffer = "";

void setup() {
    Serial.begin(115200);

    enviar_log("[GATEWAY] Initializing Bluetooth communication...");

    if (!WirelessComm.Init(BT_GATEWAY_NAME)) {
        enviar_log("[GATEWAY] Bluetooth initialization failed");
        enviar_log("[GATEWAY] Infinite loop...");
        while (1);
    }

    enviar_log("[GATEWAY] Bluetooth communication initialized");
    enviar_log("[GATEWAY] Gateway listo (HL7 <-> SENSOR)");
}

void loop() {
  
    // Lectura desde PC(Serial) y enviar a SENSOR(Bluetooth)
    while (Serial.available() > 0) {

        char c = Serial.read();

        // Inicio mensaje
        if (c == VT) {
            receivingPC = true;
            pcBuffer = "";
            enviar_log("[GATEWAY] Detectado MLLP INICIO desde PC");
            continue;
        }

        // Final mensaje
        if (c == FS && receivingPC) {

            // Consumir CR si está (sin delay)
            if (Serial.peek() == CR)
                Serial.read();

            receivingPC = false;

            enviar_log("[GATEWAY] Detectado MLLP FIN desde PC");
            enviar_log("[GATEWAY] Mensaje completo recibido desde PC");
            //enviar_log(pcBuffer);

            // reenvío Bluetooth
            for (int i = 0; i < pcBuffer.length(); i++)
                WirelessComm.writeData(pcBuffer.charAt(i));

            WirelessComm.writeData(FS);
            WirelessComm.writeData(CR);

            enviar_log("[GATEWAY] HL7 reenviado al SENSOR");

            continue;
        }

        // Parte intermedia mensaje
        if (receivingPC)
            pcBuffer += c;
    }

    // Lectura desde SENSOR(Bluetooth) y enviar a PC(Serial)
    while (WirelessComm.Available()) {

        char b;
        WirelessComm.readData(&b);

        // Inicio mensaje
        if (b == VT) {
            receivingSensor = true;
            sensorBuffer = "";
            enviar_log("[GATEWAY] Detectado MLLP INICIO desde SENSOR");
            continue;
        }

        // Final mensaje
        if (b == FS && receivingSensor) {

            // no esperamos CR, lo mandamos nosotros
            receivingSensor = false;

            enviar_log("[GATEWAY] Detectado MLLP FIN desde SENSOR");
            enviar_log("[GATEWAY] Mensaje completo recibido desde SENSOR");
            //enviar_log(sensorBuffer);

            reenviarAlPC(sensorBuffer);

            continue;
        }

        // Parte intermedia mensaje
        if (receivingSensor && b != CR)
            sensorBuffer += b;
    }

}

// Enviar info de LOG al PC sin interferir en MLLP
void enviar_log(String msg) {
    Serial.write(0x02);   // STX: comienzo log
    Serial.print(msg);
    Serial.write(0x03);   // ETX: fin log
}

// Enviar HL7 al PC con MLLP
void reenviarAlPC(String msg) {
    enviar_log("[GATEWAY] Reenviando HL7 al PC");

    Serial.write(VT);
    Serial.print(msg);
    Serial.write(FS);
    Serial.write(CR);
}
