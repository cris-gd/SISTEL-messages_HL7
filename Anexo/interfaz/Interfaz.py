from PyQt5 import QtWidgets,QtCore
from PyQt5.QtCore import QObject, QThread, pyqtSignal
from UI_Interfaz import Ui_MainWindow
from SensorWindow import SensorWindow 
import serial
import time
from datetime import datetime
from NonORUWindow import NonORUWindow


# ============================================================
# WORKER PARA LEER EL GATEWAY EN PARALELO
# ============================================================
class SerialReader(QObject):
    data_received = pyqtSignal(bytes)  # Cambiar a bytes en lugar de str
    finished = pyqtSignal()

    def __init__(self, port=None, ser=None):
        super().__init__()
        self.port = port
        self.ser = ser
        self.running = True

    def run(self):
        try:
            if self.ser is not None:
                # Nada que abrir
                pass
            else:
                if self.port is None:
                    raise ValueError("SerialReader necesita 'port' o 'ser'")
                
                self.ser = serial.Serial(self.port, 115200, timeout=1)
                time.sleep(2)

            self.data_received.emit(b"[INFO] Conectado para lectura")

            buffer = b""
            while self.running:
                try:
                    if self.ser.in_waiting > 0:
                        # Leer todos los bytes disponibles
                        data = self.ser.read(self.ser.in_waiting)
                        if data:
                            self.data_received.emit(data)
                    else:
                        time.sleep(0.01)
                except Exception as e:
                    self.data_received.emit(f"[ERROR lectura] {e}".encode())
                    break

        except Exception as e:
            self.data_received.emit(f"[ERROR] {e}".encode())
        self.finished.emit()

    def stop(self):
        self.running = False
        if self.ser and self.ser.is_open:
            self.ser.close()

# ============================================================
# WORKER PARA ENVIAR AL GATEWAY EN PARALELO
# ============================================================
class SerialSender(QObject):
    finished = pyqtSignal()
    sent = pyqtSignal(int)  # bytes enviados
    error = pyqtSignal(str)

    def __init__(self, serial_gateway, message):
        super().__init__()
        self.serial_gateway = serial_gateway
        self.message = message
        self.running = True

    def run(self):
        try:
            if self.serial_gateway and self.serial_gateway.is_open:
                # Enviar con delimitadores MLLP
                mllp_message = b'\x0b' + self.message.encode() + b'\x1c\x0d'
                
                # Enviar mensaje
                bytes_written = self.serial_gateway.write(mllp_message)
                self.serial_gateway.flush()
                time.sleep(0.1)
                if self.serial_gateway.out_waiting == 0:
                    self.sent.emit(bytes_written)
                else:
                    self.error.emit(f"Quedaron {self.serial_gateway.out_waiting} bytes en buffer")
                    
            else:
                    self.error.emit("Gateway no conectado")
                
        except Exception as e:
            self.error.emit(f"Error de envío: {e}")
        finally:
            self.finished.emit()

    def stop(self):
        self.running = False

# ============================================================
# MAIN WINDOW
# ============================================================        
class MainWindow(QtWidgets.QMainWindow, Ui_MainWindow):
    def __init__(self):
        super().__init__()
        self.setupUi(self)

        self.hl7_buffer = ""

        self.reader_thread = None
        self.reader_gateway_thread = None
        self.reader_gateway = None
        self.send_thread = None
        self.send_worker = None
        self.en_mensaje_mllp = False 

        # señales UI
        self.comboErrorGlobal.currentIndexChanged.connect(self.actualizar_error_global)
        self.comboTipoMensaje.currentIndexChanged.connect(self.actualizar_visibilidad_oru)
        self.comboNumSensORU.currentIndexChanged.connect(self.crear_selectores_oru)

        self.btnInicio.clicked.connect(lambda: self.enviar("Inicio"))
        self.btnFin.clicked.connect(lambda: self.enviar("Final"))

        # NUEVAS SEÑALES PARA ACTUALIZAR VISTA PREVIA
        self.comboErrorGlobal.currentIndexChanged.connect(self.actualizar_vista_previa)
        self.comboTipoMensaje.currentIndexChanged.connect(self.actualizar_vista_previa)
        self.comboCodErrorGlobal.currentIndexChanged.connect(self.actualizar_vista_previa)
        self.comboNumSensORU.currentIndexChanged.connect(self.actualizar_vista_previa)

        # crear selectores ORU iniciales
        self.crear_selectores_oru()

        # Variables para comunicación serial
        self.serial_gateway = None  # Para enviar al Gateway (COM3)
        self.sensor_windows = []
        self.non_oru_windows = []

        # Variables para comunicación serial
        self.init_serial()

        # Gestión estado de la comunicación
        self.comunicacion_activa = False
        self.esperando_ack = False
        self.mensaje_pendiente_ack = None

    def init_serial(self):
        try:
            # CERRAR CONEXIÓN ANTERIOR
            if hasattr(self, 'serial_gateway') and self.serial_gateway and self.serial_gateway.is_open:
                self.serial_gateway.close()
                time.sleep(2)
            
            self.serial_gateway = serial.serial_for_url(
                'COM3',
                baudrate=115200,
                timeout=1,
                write_timeout=2,
                do_not_open=True  # Configurar antes de abrir
            )
            
            # CONFIGURAR MANUALMENTE ANTES DE ABRIR
            self.serial_gateway.dtr = False
            self.serial_gateway.rts = False

            self.serial_gateway.open()
            time.sleep(2)  # Esperar a que se estabilice

            self.gatewayLog.appendPlainText("Conectado a COM3 - Configuración manual")
            
            # LIMPIAR BUFFERS
            time.sleep(1)
            self.serial_gateway.reset_input_buffer()
            self.serial_gateway.reset_output_buffer()
            
            # INICIAR LECTOR
            self.iniciar_lector_gateway(self.serial_gateway)
            
        except Exception as e:
            self.gatewayLog.appendPlainText(f" Error COM3: {e}")
            self.serial_gateway = None

            # Intentar reconexión automática después de 5 segundos
            QtCore.QTimer.singleShot(5000, self.init_serial)

    
    def iniciar_lector_gateway(self, serial_object):
        """Iniciar el hilo de lectura para el Gateway"""
        try:
            # SOLO DETENER SI EXISTE 
            if hasattr(self, 'reader_gateway') and self.reader_gateway:
                self.reader_gateway.stop()
            
            # CREAR NUEVO LECTOR
            self.reader_gateway_thread = QThread()
            self.reader_gateway = SerialReader(ser=serial_object)
            self.reader_gateway.moveToThread(self.reader_gateway_thread)

            # CONECTAR SEÑALES BÁSICAS
            self.reader_gateway_thread.started.connect(self.reader_gateway.run)
            self.reader_gateway.data_received.connect(self.on_gateway_data)
            self.reader_gateway.finished.connect(self.reader_gateway_thread.quit)

            # INICIAR
            self.reader_gateway_thread.start()
            self.gatewayLog.appendPlainText(" Lector Gateway iniciado")
            
        except Exception as e:
            self.gatewayLog.appendPlainText(f" Error iniciando lector Gateway: {e}")
    
    def on_gateway_data(self, data_bytes):
        try:
            for byte in data_bytes:
                # 1) INICIO DE LOG  (STX = 0x02)
                if byte == 0x02:       # STX (start log)
                    self.log_buffer = ""
                    self.en_log = True
                    continue

                # 2) FIN DE LOG (ETX = 0x03)
                if byte == 0x03 and getattr(self, "en_log", False):   # ETX
                    self.en_log = False
                    # Mostrar log en verde
                    self.gatewayLog.appendPlainText(self.log_buffer)
                    continue

                # Si estamos dentro de un log → acumular caracteres
                if getattr(self, "en_log", False):
                    # ignorar CR/LF
                    if byte not in (0x0D, 0x0A):
                        self.log_buffer += chr(byte)
                    continue

                # 3) INICIO MLLP (0x0B) HL7 PURO
                if byte == 0x0B:      # VT
                    self.en_hl7 = True
                    self.hl7_buffer = ""
                    continue

                # 4) FIN MLLP (0x1C) + CR
                if byte == 0x1C and getattr(self, "en_hl7", False):
                    # Al terminar FS, esperamos CR
                    self.en_hl7 = False

                    # Procesar el mensaje completo
                    hl7 = self.hl7_buffer.strip()
                    if hl7:
                        self.gatewayLog.appendPlainText("=== HL7 RECIBIDO ===")
                        self.gatewayLog.appendPlainText(hl7)
                        self.gatewayLog.appendPlainText("====================")
                        self.procesar_mensaje_hl7(hl7)

                    self.hl7_buffer = ""
                    continue

                # Si estamos en HL7 → añadir caracteres
                if getattr(self, "en_hl7", False):
                    if byte not in (0x0D,):   # ignorar CR dentro del mensaje
                        self.hl7_buffer += chr(byte)
                    continue

                # 5) CUALQUIER OTRA COSA → ignorar
                # Nada: fuera de logs y fuera de HL7
                pass

        except Exception as e:
            self.gatewayLog.appendPlainText(f"ERROR en on_gateway_data: {e}")

    # ======================================================================
    #   MOSTRAR SENSORES ORU CUANDO CORRESPONDA
    # ======================================================================
    def actualizar_visibilidad_oru(self):
        visible = self.comboTipoMensaje.currentText() == "ORU_R01"
        self.comboNumSensORU.setEnabled(visible)
        self.widgetORUSens.setVisible(visible)

    # ======================================================================
    #   ACTUALIZAR ERROR GLOBAL
    # ======================================================================
    def actualizar_error_global(self):
        if self.comboErrorGlobal.currentText() == "SI":
            self.comboCodErrorGlobal.setEnabled(True)
        else:
            self.comboCodErrorGlobal.setEnabled(False)

    # ======================================================================
    #   CREAR LISTA DE SELECTORES ORU DINÁMICOS
    # ======================================================================
    def crear_selectores_oru(self):
        while self.layoutORUSens.count():
            w = self.layoutORUSens.takeAt(0).widget()
            if w:
                w.deleteLater()

        self.entradas_oru = []

        n = int(self.comboNumSensORU.currentText())

        for i in range(n):
            cont = QtWidgets.QWidget()
            lay = QtWidgets.QHBoxLayout(cont)

            label = QtWidgets.QLabel(f"Sensor ORU {i+1}:")
            combo = QtWidgets.QComboBox()
            combo.addItems(["Acelerómetro", "Termómetro", "Pulsioxímetro", "Tensiómetro"])

            combo.currentIndexChanged.connect(self.actualizar_vista_previa)

            umMin = QtWidgets.QLineEdit()
            umMin.setPlaceholderText("Min")
            umMin.setFixedWidth(60)

            umMax = QtWidgets.QLineEdit()
            umMax.setPlaceholderText("Max")
            umMax.setFixedWidth(60)

            lay.addWidget(label)
            lay.addWidget(combo)
            lay.addWidget(QtWidgets.QLabel("Min:"))
            lay.addWidget(umMin)
            lay.addWidget(QtWidgets.QLabel("Max:"))
            lay.addWidget(umMax)
            lay.addStretch()

            self.layoutORUSens.addWidget(cont)
            self.entradas_oru.append((combo, umMin, umMax))

        self.actualizar_visibilidad_oru()
        self.actualizar_vista_previa()

    # ============================================================
    # GENERAR MENSAJE HL7
    # ============================================================
    def generar_HL7(self, modo):
        # ========== MSH FIJO ==========
        ts = datetime.now().strftime("%Y%m%d%H%M%S")

        MSH = [
            "MSH",
            "^~\&",
            "GRUPO_PRACTICAS5^^",
            "", "", "",
            ts,
            "",
            "Z^CTL^Z_CONTROL",
            "MESSAGE_ID_SENDER_1",
            "T",
            "2.6",
            "", "",
            "AL",
            "AL",
            "", "", "", "",
            "UPNA"
        ]
        segmento_MSH = "|".join(MSH)

        # ========== Z ==========
        if modo == "Final":
            segmento_Z = "Z|FINAL"
        else:
            tipo = self.comboTipoMensaje.currentText()
            z3 = self.comboErrorGlobal.currentText()
            z4 = self.comboCodErrorGlobal.currentText() if z3 == "SI" else ""

            loincs = []
            if tipo == "ORU_R01":
                for combo, umMin, umMax in self.entradas_oru:
                    txt = combo.currentText()
                    if txt == "Acelerómetro": loincs.append("80493-0")
                    elif txt == "Termómetro": loincs.append("8310-5")
                    elif txt == "Pulsioxímetro": loincs.append("20564-1")
                    elif txt == "Tensiómetro": loincs.append("55284-4")

                if loincs:
                    z2 = tipo + "^" + "^".join(loincs)  # ORU_R01^20564-1^8310-5
                else:
                    z2 = tipo
            else:
                z2 = tipo

            partes = ["Z", "INICIO", z2, z3]
            if z4: partes.append(z4)

            segmento_Z = "|".join(partes)
            #self.gatewayLog.appendPlainText(f" Generado segmento Z: {segmento_Z}")

        return segmento_MSH + "\x0D" + segmento_Z + "\x0D"

    # ============================================================
    # ENVIAR HL7 AL GATEWAY
    # ============================================================
    def enviar(self, modo):
        # Cerrar todas las ventanas existentes primero
        self.cerrar_ventanas_sensores()

        # Verificar conexiones
        mensaje = self.generar_HL7(modo)
        self.previewText.setPlainText(mensaje)

        # Mostrar en el log
        self.gatewayLog.appendPlainText("=" * 60)
        self.gatewayLog.appendPlainText(f" ENVIANDO MENSAJE {modo.upper()} AL GATEWAY")
        self.gatewayLog.appendPlainText("=" * 60)
        self.gatewayLog.appendPlainText(mensaje)
        self.gatewayLog.appendPlainText("=" * 60)

        if modo == "Inicio":
            self.comunicacion_activa = True
            self.esperando_ack = True
            self.mensaje_pendiente_ack = mensaje
            self.crear_ventanas_sensores(mensaje)
        elif modo == "Final":
            self.comunicacion_activa = False
            self.esperando_ack = False

        self.enviar_con_worker(mensaje)
 
            
    def enviar_con_worker(self, mensaje):
        """Enviar mensaje usando worker paralelo"""
        if not self.serial_gateway or not self.serial_gateway.is_open:
            self.gatewayLog.appendPlainText(" ERROR: Gateway no conectado")
            return

        # Crear worker y thread para envío
        self.send_thread = QThread()
        self.send_worker = SerialSender(self.serial_gateway, mensaje)
        self.send_worker.moveToThread(self.send_thread)

        # Conectar señales
        self.send_thread.started.connect(self.send_worker.run)
        self.send_worker.sent.connect(self.on_message_sent)
        self.send_worker.error.connect(self.on_send_error)
        self.send_worker.finished.connect(self.send_thread.quit)
        self.send_worker.finished.connect(self.send_worker.deleteLater)
        self.send_thread.finished.connect(self.send_thread.deleteLater)

        # Iniciar thread
        self.send_thread.start()

    def on_message_sent(self, bytes_written):
        #self.gatewayLog.appendPlainText(f" {bytes_written} bytes enviados a Gateway")
        self.gatewayLog.appendPlainText(" Mensaje enviado correctamente")

    def on_send_error(self, error_msg):
        self.gatewayLog.appendPlainText(f" {error_msg}")
    
    # ============================================================
    # MOSTRAR VENTANAS DE SENSORES
    # ============================================================
    def crear_ventanas_sensores(self, mensaje_Z):
        # Cerrar anteriores
        try:
            for w in self.sensor_windows:
                w.close()
        except:
            pass

        self.sensor_windows.clear()

        self.sensor_windows = []

        tipo_mensaje = self.comboTipoMensaje.currentText()

        # Solo generamos ventanas si el tipo de mensaje es ORU_R01
        if tipo_mensaje != "ORU_R01":
            return

        # Crear tantas ventanas como sensores ORU se hayan configurado
        for i, (combo, umMin, umMax) in enumerate(self.entradas_oru, start=1):
            tipo_sensor = combo.currentText()
            err_solicitado = self.comboErrorGlobal.currentText()
            cod_error = self.comboCodErrorGlobal.currentText()

            if err_solicitado == "SI":
                err_text = f"Error solicitado: {cod_error}"
            else:
                err_text = "Error solicitado: Ninguno"

            try:
                um_min_val = float(umMin.text()) if umMin.text() != "" else None
                um_max_val = float(umMax.text()) if umMax.text() != "" else None
            except:
                um_min_val = um_max_val = None

            ventana = SensorWindow(
                sensor_id=i,
                tipo_sensor=tipo_sensor,
                mensaje="",
                error_user=err_text,
                error_detect="Detección pendiente",
                umbral_min=um_min_val,
                umbral_max=um_max_val
            )

            self.sensor_windows.append(ventana)
            ventana.show()

    def actualizar_vista_previa(self):
        """Actualizar la vista previa del mensaje HL7 cuando cambia la configuración"""
        try:
            mensaje = self.generar_HL7("Inicio")
            self.previewText.setPlainText(mensaje)
        except Exception as e:
            self.previewText.setPlainText(f"Error generando vista previa: {e}")

    # ============================================================
    # CERRAR VENTANAS DE SENSORES
    # ============================================================
    def cerrar_ventanas_sensores(self):
        """Cierra todas las ventanas de sensores activas"""
        try:            
            # Cerrar ventanas de sensores ORU
            for ventana in self.sensor_windows:
                ventana.close()
        
            # Cerrar ventanas no ORU
            for ventana in self.non_oru_windows:
                ventana.close()
                            
            # Limpiar las listas
            self.sensor_windows.clear()
            self.non_oru_windows.clear()
            
        except Exception as e:
            self.gatewayLog.appendPlainText(f"Error cerrando ventanas: {e}")
   
    # ============================================================
    # CERRAR APLICACIÓN Y HILO
    # ============================================================
    def closeEvent(self, event):
        """Cerrar todo correctamente al salir"""
        try:
            # Cerrar ventanas de sensores ORU
            for ventana in self.sensor_windows:
                try:
                    ventana.close()
                except:
                    pass
                    
            # Cerrar ventanas no ORU
            if hasattr(self, 'non_oru_windows'):
                for ventana in self.non_oru_windows:
                    try:
                        ventana.close()
                    except:
                        pass
            
            # Limpiar las listas
            self.sensor_windows.clear()
            self.non_oru_windows.clear()
            if hasattr(self, 'non_oru_windows'):
                self.non_oru_windows.clear()

            # DETENER LECTOR
            if self.reader_gateway:
                self.reader_gateway.stop()
                
            # CERRAR HILOS
            if hasattr(self, 'reader_gateway_thread') and self.reader_gateway_thread.isRunning():
                self.reader_gateway_thread.quit()
                self.reader_gateway_thread.wait(1000)
            
            # DETENER WORKER DE ENVÍO
            if self.send_worker:
                self.send_worker.stop()
            if self.send_thread and self.send_thread.isRunning():
                self.send_thread.quit()
                self.send_thread.wait(1000)
            
            # CERRAR PUERTO SERIAL
            if self.serial_gateway and self.serial_gateway.is_open:
                self.serial_gateway.close()
                self.gatewayLog.appendPlainText("Puerto COM3 cerrado")
                
        except Exception as e:
            self.gatewayLog.appendPlainText(f"Error en cierre: {e}")
        
        event.accept()

    # ============================================================
    # PROCESAR MENSAJE
    # ============================================================   
    def procesar_mensaje_hl7(self, hl7_str):
        """Procesar mensaje HL7 completo con detección automática de errores"""
        try:
            if not hl7_str or not hl7_str.strip():
                self.gatewayLog.appendPlainText(" Mensaje HL7 vacío")
                return
                            
            self.gatewayLog.appendPlainText("\n" + "="*50)
            self.gatewayLog.appendPlainText(" MENSAJE HL7 COMPLETO RECIBIDO:")
            self.gatewayLog.appendPlainText("="*50)
            self.gatewayLog.appendPlainText(hl7_str)
            self.gatewayLog.appendPlainText("="*50)
            
            # Detectar errores automáticamente
            errores_detectados, errores_OBX = self.detectar_errores_hl7(hl7_str)
            
            # Dividir en segmentos
            segmentos = [s.strip() for s in hl7_str.replace('\n','\r').split('\r') if s.strip()]

            if not segmentos:
                self.gatewayLog.appendPlainText("No se pudieron extraer segmentos HL7")
                return
                
            # Procesar segmento MSH
            msh_segment = segmentos[0]
            msh_segment = msh_segment.strip()
            if not msh_segment.startswith('MSH'):
                self.gatewayLog.appendPlainText(" Segmento MSH no encontrado")
                return
                
            campos_msh = msh_segment.split('|')
            if len(campos_msh) < 9:
                self.gatewayLog.appendPlainText(" Segmento MSH incompleto")
                return
                
            tipo_mensaje = campos_msh[8]
            self.gatewayLog.appendPlainText(f"Tipo de mensaje: {tipo_mensaje}")
            
            # Mensajes ORU
            if "R01" in tipo_mensaje or "ORU" in tipo_mensaje :
                self.procesar_ORU(segmentos, errores_detectados, errores_OBX)

            # Mensajes ADT
            elif "ADT" in tipo_mensaje or "A05" in tipo_mensaje:
                self.procesar_mensaje_no_ORU(segmentos, tipo_mensaje, errores_detectados, errores_OBX)

            # Mensajes OMD
            elif "OMD" in tipo_mensaje or "O03" in tipo_mensaje:
                self.procesar_mensaje_no_ORU(segmentos, tipo_mensaje, errores_detectados, errores_OBX)

            else:
                self.gatewayLog.appendPlainText(f" Tipo de mensaje no soportado: {tipo_mensaje}")
                self.gatewayLog.appendPlainText("Comunicación detenida: mensaje no ORU")
                self.enviar_ACK(hl7_str, "200", "R01")
                self.comunicacion_activa = False
                self.esperando_ack = False
                
        except Exception as e:
            self.gatewayLog.appendPlainText(f" ERROR en procesar_mensaje_hl7: {e}")

    # ============================================================
    # DETECTAR ERRORES HL7
    # ============================================================    
    def detectar_errores_hl7(self, hl7_message):
        """Detectar errores automáticamente en el mensaje HL7 recibido

        Devuelve:
        - errores  (MSH, PID, OBR, versión, tipo…)
        - errores_OBX(solo OBX para 101)
        """
        errores = []
        errores_OBX = {}
        
        segmentos = hl7_message.split('\r')

        if not segmentos:
            return [], {}
            
        # Buscar segmento MSH
        msh_segment = None
        for seg in segmentos:
            if seg.startswith('MSH'):
                msh_segment = seg
                break

        # VALIDACIÓN: Si no hay MSH, no se puede procesar
        if msh_segment is None:
            self.gatewayLog.appendPlainText(" ERROR: Segmento MSH no encontrado en el mensaje")
            return [], {}
            
        campos_msh = msh_segment.split('|')
            
        # 1. Verificar versión HL7 (campo 12)
        if len(campos_msh) > 11:
            version = campos_msh[11].strip()
            if version not in ['2.6']:
                errores.append("ERR-203: Versión HL7 inválida")
            
        # 2. Verificar tipo de mensaje (campo 9)
        if len(campos_msh) > 8:
            tipo_mensaje = campos_msh[8].strip()
            
            # Extraer las partes del tipo de mensaje (formato: AAA^BBB^CCC)
            partes_tipo_mensaje = tipo_mensaje.split('^')

            # Verificar que tenga al menos la primera parte
            if len(partes_tipo_mensaje) >=3:
                tipo_mensaje_base = partes_tipo_mensaje[0]
            
                # ERROR 200: Verificar que el tipo base sea uno de los válidos
                tipos_base_validos = ['ORU', 'OMD', 'ADT']

                tercera_parte = partes_tipo_mensaje[2]
                # Extraer la parte antes del _
                parte_antes_guion = tercera_parte.split('_')[0] if '_' in tercera_parte else tercera_parte
            
                # Verificar que coincida con los tipos base válidos
                if tipo_mensaje_base not in tipos_base_validos or parte_antes_guion not in tipos_base_validos:
                    errores.append("ERR-200: Tipo de mensaje no soportado")
                
                # Códigos válidos: R01 (ORU), A05 (ADT), O03 (OMD)
                codigos_evento_validos = ['R01', 'A05', 'O03']
                segunda_parte = partes_tipo_mensaje[1]
                parte_despues_guion=tercera_parte.split('_')[1] if '_' in tercera_parte else tercera_parte
        
                if segunda_parte not in codigos_evento_validos or parte_despues_guion not in codigos_evento_validos:
                    errores.append("ERR-201: Código de evento no soportado")
                        
        # 3. Verificar processing id (campo 11)
        if len(campos_msh) > 10:
            processing_id = campos_msh[10].strip()
            if processing_id not in ['T', 'D', 'P']:
                errores.append("ERR-202: Processing ID no soportado")
        
        # 4. Detectar error 101 (Required Field Missing)
        errores_globales_101, errores_sensor_101 = self.detectar_error_101(hl7_message)
        errores.extend(errores_globales_101)

        if tipo_mensaje.startswith("ORU"):
            errores_OBX.update(errores_sensor_101)
        else:
            errores_OBX = {}   # Nunca incluir errores de sensores en ADT u OMD
        
        return errores, errores_OBX
    
    # ============================================================
    # DETECTAR ERRORES HL7
    # ============================================================  
    
    def detectar_error_101(self, hl7_message):
        """
        Detecta exclusivamente error 101 (Required Field Missing).
        Devuelve:
            - errores_globales_101  → errores en MSH, PID, OBR, PV1, AL1, ORC, TQ1, ODS, NTE
            - errores_por_sensor_101 → dict { loinc : [errores] }
        """
        errores_globales_101 = []
        errores_sensor_101 = {}

        # INTENTAR DIFERENTES SEPARADORES DE SEGMENTO
        segmentos = hl7_message.split('\r')

        if len(segmentos) == 1 or (len(segmentos) == 2 and segmentos[1] == ''):
            segmentos = hl7_message.split('\n')
        
        # Limpiar segmentos vacíos
        segmentos = [seg.strip() for seg in segmentos if seg.strip()]

        # ============================================================
        # MSH (global)
        # ============================================================ 

        # MSH - Segmento especial con delimitador en MSH-1
        msh = next((s for s in segmentos if s.startswith("MSH")), None)
        if msh:
            # Verificar que el segmento MSH esté bien formado
            if '|' not in msh:
                errores_globales_101.append("ERR-101: MSH mal formado (sin delimitadores)")
                return errores_globales_101, errores_sensor_101
            
            # MSH-1: El delimitador de campo debe estar presente inmediatamente después de "MSH"
            if len(msh) < 4:  # "MSH" + al menos un carácter para el delimitador
                errores_globales_101.append("ERR-101: Falta MSH-1 (delimitador de campo)")
            
            elif msh[3] != '|':  # El cuarto carácter debe ser '|'
                errores_globales_101.append("ERR-101: MSH-1 inválido (delimitador de campo)")
            
            # Ahora dividir en campos
            campos_msh = msh.split('|')
                
            # Verificar campos obligatorios
            # MSH-1 ya está verificado arriba
            
            # MSH-2 (caracteres de codificación) - índice 1 en array
            if len(campos_msh) < 2 or campos_msh[1].strip() == "":
                errores_globales_101.append("ERR-101: Falta MSH-2 (caracteres de codificación)")
            
            # MSH-3 (sending application) - índice 2 en array
            if len(campos_msh) < 3 or campos_msh[2].strip() == "":
                errores_globales_101.append("ERR-101: Falta MSH-3 (sending application)")

            # MSH-7 (timestamp) - índice 6 en array
            if len(campos_msh) < 7 or campos_msh[6].strip() == "":
                errores_globales_101.append("ERR-101: Falta MSH-7 (timestamp)")
            
            # MSH-9 (tipo mensaje) - índice 8 en array
            if len(campos_msh) < 9 or campos_msh[8].strip() == "":
                errores_globales_101.append("ERR-101: Falta MSH-9 (tipo de mensaje)")
            
            # MSH-10 (ID mensaje) - índice 9 en array
            if len(campos_msh) < 10 or campos_msh[9].strip() == "":
                errores_globales_101.append("ERR-101: Falta MSH-10 (ID del mensaje)")
            
            # MSH-11 (processing ID) - índice 10 en array
            if len(campos_msh) < 11 or campos_msh[10].strip() == "":
                errores_globales_101.append("ERR-101: Falta MSH-11 (processing ID)")
            
            # MSH-12 (versión HL7) - índice 11 en array
            if len(campos_msh) < 12 or campos_msh[11].strip() == "":
                errores_globales_101.append("ERR-101: Falta MSH-12 (versión HL7)")

            # MSH-15 (Accept Acknowledgment Type) - índice 14 en array
            if len(campos_msh) < 15 or campos_msh[14].strip() == "":
                errores_globales_101.append("ERR-101: Falta MSH-15 (Accept Acknowledgment Type)")

            # MSH-16 (Application Acknowledgment Type) - índice 15 en array
            if len(campos_msh) < 16 or campos_msh[15].strip() == "":
                errores_globales_101.append("ERR-101: Falta MSH-16 (Application Acknowledgment Type)")
            
            # MSH-21 (Message Profile Identifier) - índice 20 en array
            if len(campos_msh) < 21 or campos_msh[20].strip() == "":
                errores_globales_101.append("ERR-101: Falta MSH-21 (Message Profile Identifier)")

        # ============================================================
        # PID (global)
        # ============================================================
        pid = next((s for s in segmentos if s.startswith("PID")), None)
        if pid:
            campos_pid = pid.split('|')
            
            # PID-3 (índice 3 en array) - identificador paciente
            if len(campos_pid) <= 3 or campos_pid[3].strip() == "":
                errores_globales_101.append("ERR-101: Falta PID-3 (identificador paciente)")
                
            # PID-5 (índice 5 en array) - nombre paciente
            if len(campos_pid) <= 5 or campos_pid[5].strip() == "":
                errores_globales_101.append("ERR-101: Falta PID-5 (nombre paciente)")
            
        # ============================================================
        # OBR (global)
        # ============================================================
        obr = next((s for s in segmentos if s.startswith("OBR")), None)
        if obr:
        
            campos_obr = obr.split('|')
            # OBR-1 (índice 1 en array) - OBR set ID
            if len(campos_obr) <= 1 or campos_obr[1].strip() == "":
                errores_globales_101.append("ERR-101: Falta OBR-1 (OBR set ID)")
            
            # OBR-2 (índice 2 en array) - placer order number
            if len(campos_obr) <= 2 or campos_obr[2].strip() == "":
                errores_globales_101.append("ERR-101: Falta OBR-2 (placer order number)")
            
            # OBR-3 (índice 3 en array) - filler order number
            if len(campos_obr) <= 3 or campos_obr[3].strip() == "":
                errores_globales_101.append("ERR-101: Falta OBR-3 (filler order number)")
            
            # OBR-4 (índice 4 en array) - servicio solicitado
            if len(campos_obr) <= 4 or campos_obr[4].strip() == "":
                errores_globales_101.append("ERR-101: Falta OBR-4 (servicio solicitado)")

        # ============================================================
        # OBX (por sensor)
        # ============================================================
        obx_count = 0
        for seg in segmentos:
            if not seg or not seg.startswith("OBX"):
                continue
            
            obx_count += 1
            campos = seg.split('|')

            # OBX-1 (OBX set ID) - índice 1
            if len(campos) < 2 or campos[1].strip() == "":
                errores_sensor_101[loinc].append("ERR-101: Falta OBX-1 (OBX set ID)")

            # OBX-3 (Extraer identificador de observación)- índice 3
            loinc = ""
            if len(campos) > 3:
                loinc = campos[3].split('^')[0].strip() if '^' in campos[3] else campos[3].strip()
                    
            # Si no hay Observation Identifier, error global
            if not loinc:
                errores_globales_101.append("ERR-101: Falta OBX-3 (Observation Identifier) en OBX número " + str(obx_count))
                continue
            
            # Inicializar lista para este Observation Identifier
            if loinc not in errores_sensor_101:
                errores_sensor_101[loinc] = []
    
            # OBX-4 (Observation Sub-ID) - índice 4
            if len(campos) < 5 or campos[4].strip() == "":
                errores_sensor_101[loinc].append("ERR-101: Falta OBX-4 (Observation Sub-ID)")
            
            # OBX-11 (Observation Result Status) - índice 11
            if len(campos) < 12 or campos[11].strip() == "":
                errores_sensor_101[loinc].append("ERR-101: Falta OBX-11 (Observation Result Status)")
        
        # ============================================================
        # PV1 (global)
        # ============================================================
        pv1 = next((s for s in segmentos if s.startswith("PV1")), None)
        if pv1:
        
            campos_pv1 = pv1.split('|')
            # PV1-2 (índice 2 en array) - patient class
            if len(campos_pv1) <= 2 or campos_pv1[2].strip() == "":
                errores_globales_101.append("ERR-101: Falta PV1-2 (patient class)")

        # ============================================================
        # AL1 (global)
        # ============================================================
        al1 = next((s for s in segmentos if s.startswith("AL1")), None)
        if al1:
        
            campos_al1 = al1.split('|')
            # AL1-1 (índice 1 en array) - AL1 set ID
            if len(campos_al1) <= 1 or campos_al1[1].strip() == "":
                errores_globales_101.append("ERR-101: Falta AL1-1 (AL1 set ID)")
            # AL1-3 (índice 3 en array) - Allergen description
            if len(campos_al1) <= 3 or campos_al1[3].strip() == "":
                errores_globales_101.append("ERR-101: Falta AL1-3 (Allergen description)")
        
        # ============================================================
        # ORC (global)
        # ============================================================
        orc = next((s for s in segmentos if s.startswith("ORC")), None)
        if orc:
        
            campos_orc = orc.split('|')
            # ORC-1 (índice 1 en array) - Order control
            if len(campos_orc) <= 1 or campos_orc[1].strip() == "":
                errores_globales_101.append("ERR-101: Falta ORC-1 (Order control)")

        # ============================================================
        # ODS (global)
        # ============================================================
        ods = next((s for s in segmentos if s.startswith("ODS")), None)
        if ods:
        
            campos_ods = ods.split('|')
            # ODS-1 (índice 1 en array) - Type
            if len(campos_ods) <= 1 or campos_ods[1].strip() == "":
                errores_globales_101.append("ERR-101: Falta ODS-1 (Type)")
            # ODS-3 (índice 3 en array) - Diet, supplement, preference
            if len(campos_ods) <= 3 or campos_ods[3].strip() == "":
                errores_globales_101.append("ERR-101: Falta ODS-3 (Diet, supplement, preference)")

        # ============================================================
        # NTE (global)
        # ============================================================
        nte = next((s for s in segmentos if s.startswith("NTE")), None)
        if nte:
        
            campos_nte = nte.split('|')
            # NTE-1 (índice 1 en array) - NTE set ID
            if len(campos_nte) <= 1 or campos_nte[1].strip() == "":
                errores_globales_101.append("ERR-101: Falta NTE-1 (NTE set ID)")

        return errores_globales_101, errores_sensor_101
    
    # ============================================================
    # EXTRAER CÓDIGO ERROR
    # ============================================================  
    def extraer_codigo_error(self, texto_error):
        """Extraer código numérico del error"""
        import re
        
        # Mapear descripciones específicas a códigos según la nueva lógica
        if "Versión HL7 inválida" in texto_error or "203" in texto_error:
            return "203"
        elif "Código de evento no soportado" in texto_error or "201" in texto_error:
            return "201"
        elif "Processing ID no soportado" in texto_error or "202" in texto_error:
            return "202"
        #elif "Tipo de mensaje no soportado" in texto_error or "200" in texto_error:
        #    return "200"
        elif "ERR-200" in texto_error or "Tipo" in texto_error and "soport" in texto_error:
            return "200"
        elif "Falta un campo requerido" in texto_error or "101" in texto_error:
            return "101"
 
    # ============================================================
    # PROCESAR MENSAJES ORU
    # ============================================================
    def procesar_ORU(self, segmentos, errores_detectados=None, errores_OBX=None):
        """Entry point del procesamiento ORU. Se divide en dos rutas limpias."""
        try:
            self.gatewayLog.appendPlainText("ORU_R01 recibido del sensor")

            mensaje_completo = '\r'.join(segmentos)

            # Normalizar delimitadores
            segmentos_norm = [
                s.strip()
                for s in mensaje_completo.replace('\n', '\r').replace('\x0D', '\r').split('\r')
                if s.strip()
            ]

            # Errores detectados  → NO graficar
            hay_errores_OBX = any(len(lista) > 0 for lista in errores_OBX.values())
            if errores_detectados or hay_errores_OBX:
                self.manejar_ORU_con_errores(
                    mensaje_completo,
                    segmentos_norm,
                    errores_detectados,
                    errores_OBX
                )
                return

            # Sin errores → GRAFICAR
            self.manejar_ORU_sin_errores(
                mensaje_completo,
                segmentos_norm
            )

        except Exception as e:
            self.gatewayLog.appendPlainText(f"ERROR procesar_ORU: {e}")

    def manejar_ORU_con_errores(self, mensaje_completo, segmentos_norm, errores_detectados, errores_OBX):
        """Gestión de un ORU con errores. NO se grafica."""
        self.gatewayLog.appendPlainText("Se detectaron errores en ORU. No se graficará.")

        # Mostrar errores en ventanas
        self.actualizar_errores_en_sensores(errores_detectados, errores_OBX, mensaje_completo)

        # Determinar código para ACK
        codigo = None
        if errores_detectados:
            codigo = self.extraer_codigo_error(errores_detectados[0])
        else:
            for loinc, lista in errores_OBX.items():
                if lista:
                    codigo = self.extraer_codigo_error(lista[0])
                    break

        # Enviar ACK con error
        tipo_mensaje = self.obtener_tipo_mensaje_ack(mensaje_completo)
        self.enviar_ACK(mensaje_completo, codigo, tipo_mensaje)

    def manejar_ORU_sin_errores(self, mensaje_completo, segmentos_norm):
        """Procesa el mensaje ORU cuando no tiene errores → SÍ graficar."""
        
        self.gatewayLog.appendPlainText("ORU sin errores. Procesando OBX para gráfica.")

        # Limpiar errores en ventanas
        self.actualizar_errores_en_sensores([], {}, mensaje_completo)

        # Extraer OBX
        for segmento in segmentos_norm:
            if segmento.startswith("OBX"):
                campos = segmento.split('|')
                if len(campos) >= 6:
                    loinc_bruto = campos[3].strip()
                    loinc = loinc_bruto.split("^")[0].strip()   # <-- FIJO
                    valor = campos[5].strip()
                    self.actualizar_graficas(loinc, valor)

        # ACK OK
        tipo_mensaje = self.obtener_tipo_mensaje_ack(mensaje_completo)
        self.gatewayLog.appendPlainText("Enviando ACK positivo (AA)")
        self.enviar_ACK(mensaje_completo, None, tipo_mensaje)
    
    # ============================================================
    # PROCESAR MENSAJE NO ORU - ADT ODM
    # ============================================================
    def procesar_mensaje_no_ORU(self, segmentos, tipo_mensaje, errores_detectados=None, errores_OBX=None):
        """Procesar mensajes que no son ORU (ADT, OMD)"""
        try:            
            mensaje_completo = '\r'.join(segmentos)
            
            errores_detectados, errores_OBX = self.detectar_errores_hl7(mensaje_completo)
            
            # DETERMINAR MENSAJE DE ERROR
            if errores_detectados:
                error_texto = errores_detectados[0]
                codigo_error = self.extraer_codigo_error(error_texto)
                self.gatewayLog.appendPlainText(f" Código error extraído: {codigo_error}")
            else:
                error_texto = "Ningún error detectado"
                codigo_error = None

            # MOSTRAR VENTANA EMERGENTE
            if not hasattr(self, "sensor_windows"):
                self.sensor_windows = []
            ventana = NonORUWindow(tipo_mensaje, mensaje_completo, error_texto)
            
            # Mostrar errores en ventana
            if errores_detectados:
                error_msg = "ERRORES GLOBALES:\n" + "\n".join(errores_detectados)
                ventana.lblError.setText(error_msg)
                ventana.lblError.setStyleSheet(
                    "color:red; font-weight:bold; background:#ffe6e6; padding:8px; border:1px solid red;"
                )
            
            self.non_oru_windows.append(ventana)
            ventana.show()
            ventana.raise_()
            ventana.activateWindow()

            # Enviar ACK
            tipo_ack = self.obtener_tipo_mensaje_ack(mensaje_completo)
            self.gatewayLog.appendPlainText(f" Tipo ACK determinado: {tipo_ack}")

            if codigo_error:
                self.enviar_ACK(mensaje_completo, codigo_error, tipo_ack)
            else:
                self.enviar_ACK(mensaje_completo, None, tipo_ack)

            # Mensajes NO ORU siempre terminan la comunicación
            self.comunicacion_activa = False

            self.gatewayLog.appendPlainText(" Comunicación finalizada")

        except Exception as e:
            self.gatewayLog.appendPlainText(f" ERROR procesando mensaje {tipo_mensaje}: {e}")

    # ============================================================
    # ENVIAR ACK
    # ============================================================  
    def enviar_ACK(self, mensaje_original, error_detectado=None, x_type="R01"):
        """Enviar ACK según el formato requerido por el sensor"""
        try:
            # Extraer información del mensaje original si está disponible
            segmentos = mensaje_original.split('\r')
            if segmentos and segmentos[0].startswith('MSH'):
                campos_msh = segmentos[0].split('|')
                if len(campos_msh) > 9:
                    msh_3_incoming = campos_msh[2]
                    msh_10_incoming = campos_msh[9]
                
                ts = datetime.now().strftime("%Y%m%d%H%M%S")
                
                if error_detectado:
                    # ACK CON ERROR
                    ack = (
                        f"MSH|^~\\&|GRUPO_PRACTICAS5^^||{msh_3_incoming}||{ts}||ACK^{x_type}^ACK|ACK_{msh_10_incoming}|T|2.6||||||||UPNA\r"
                        f"MSA|AR|{msh_10_incoming}\r"
                        f"ERR|||{error_detectado}|E\r"
                    )
                    self.gatewayLog.appendPlainText(f" Enviando ACK CON ERROR al gateway: {error_detectado}")
                else:
                    # ACK CORRECTO
                    ack = (
                        f"MSH|^~\\&|GRUPO_PRACTICAS5^^||{msh_3_incoming}||{ts}||ACK^{x_type}^ACK|ACK_{msh_10_incoming}|T|2.6||||||||UPNA\r"
                        f"MSA|AA|{msh_10_incoming}\r"
                    )
                    self.gatewayLog.appendPlainText(" Enviando ACK CORRECTO al gateway")
                
                self.enviar_con_worker(ack)
                
        except Exception as e:
            self.gatewayLog.appendPlainText(f" ERROR en enviar_ACK: {e}")
    
    # ============================================================
    # OBTENER MENSAJE ACK
    # ============================================================
    def obtener_tipo_mensaje_ack(self, mensaje_original):
        """Determinar el tipo de mensaje para el ACK"""
        try:
            segmentos = mensaje_original.split('\r')
            if segmentos and segmentos[0].startswith('MSH'):
                campos_msh = segmentos[0].split('|')
                if len(campos_msh) > 8:
                    tipo_completo = campos_msh[8]
                    if "ORU" in tipo_completo:
                        return "R01"
                    elif "ADT" in tipo_completo:
                        return "A05"
                    elif "OMD" in tipo_completo:
                        return "O03"
        except:
            pass
        return "R01"  # Valor por defecto

    # ============================================================
    # ACTUALIZAR GRÁFICAS
    # ============================================================    
    def actualizar_graficas(self, loinc, valor):
        """Actualizar gráficas en ventanas de sensores"""        
        try:
            for ventana in self.sensor_windows:
                tipo = ventana.tipo_sensor
                codigo = loinc.split("^")[0].strip()

                if tipo == "Acelerómetro" and codigo == "80493-0":
                    valor_num = float(valor)
                    ventana.update_graph(valor_num)
                elif tipo == "Termómetro" and codigo == "8310-5":
                    valor_num = float(valor.replace(',', '.'))
                    ventana.update_graph(valor_num)
                elif tipo == "Pulsioxímetro" and codigo == "20564-1":
                    valor_num = float(valor.replace(',', '.'))
                    ventana.update_graph(valor_num)
                elif tipo == "Tensiómetro" and codigo == "55284-4":

                    # Para tensiómetro, procesar el formato "120/80"
                    if "/" in valor:
                        try:
                            sistolica, diastolica = map(float, valor.split("/"))
                            ventana.update_graph(f"{sistolica}/{diastolica}")
                        except ValueError:
                            self.gatewayLog.appendPlainText(f" Formato de tensión inválido: {valor}")
                    else:
                        # Si no tiene formato correcto, intentar convertir a float
                        try:
                            valor_num = float(valor)
                            ventana.update_graph(valor_num)
                        except ValueError:
                            self.gatewayLog.appendPlainText(f" Valor de tensión no numérico: {valor}")
                    
        except Exception as e:
            self.gatewayLog.appendPlainText(f" Error actualizando gráficas: {e}")

    # ============================================================
    # ACTUALIZAR ERRORES EN SENSORES
    # ============================================================ 
    def actualizar_errores_en_sensores(self, errores_detectados, errores_OBX, mensaje_recibido=None):
        """
        Muestra errores:
            - globales → en todas las ventanas
            - por sensor → solo al sensor correspondiente
        """
        for ventana in self.sensor_windows:
            if mensaje_recibido:
                ventana.set_message(mensaje_recibido)
            
            loinc_sensor = None
            tipo = ventana.tipo_sensor

            # Mapeo tipo → LOINC
            if tipo == "Acelerómetro": loinc_sensor = "80493-0"
            elif tipo == "Termómetro": loinc_sensor = "8310-5"
            elif tipo == "Pulsioxímetro": loinc_sensor = "20564-1"
            elif tipo == "Tensiómetro": loinc_sensor = "55284-4"

            msg = ""

            # 1) Errores globales
            if errores_detectados:
                msg += " ERRORES GLOBALES:\n" + "\n".join(errores_detectados)

            # 2) Errores específicos de sensor
            if loinc_sensor in errores_OBX and errores_OBX[loinc_sensor]:
                if msg != "":
                    msg += "\n\n"
                msg += " ERRORES DEL SENSOR:\n" + "\n".join(errores_OBX[loinc_sensor])

            ventana.set_error_detect(msg if msg else "Ningún error detectado")


        for ventana in self.non_oru_windows:  # Solo NonORUWindow
            if errores_detectados:
                msg = "ERRORES GLOBALES:\n" + "\n".join(errores_detectados)

                if hasattr(ventana, 'set_error_detect'):
                    ventana.set_error_detect(msg)
                elif hasattr(ventana, 'lblError'):
                    ventana.lblError.setText(msg)

if __name__ == "__main__":
    app = QtWidgets.QApplication([])
    win = MainWindow()
    win.show()
    app.exec_()
