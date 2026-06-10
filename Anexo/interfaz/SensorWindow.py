from PyQt5 import QtWidgets, QtCore
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import matplotlib.pyplot as plt
from datetime import datetime
import numpy as np


class SensorWindow(QtWidgets.QMainWindow):
    def __init__(self, sensor_id, tipo_sensor, mensaje="", error_user="", error_detect="", umbral_min = None, umbral_max = None,parent=None):
        super().__init__(parent)

        self.sensor_id = sensor_id
        self.tipo_sensor = tipo_sensor

        self.umbral_min = umbral_min
        self.umbral_max = umbral_max

        self.datos = []  # Almacenar datos para la gráfica
        self.tiempos = []  # Almacenar timestamps
        self.max_puntos = 80  # Máximo de puntos en gráfica

        # Configuración ventana
        self.setWindowTitle(f"Sensor {sensor_id} - {tipo_sensor}")
        self.resize(800, 600)

        widget = QtWidgets.QWidget()
        self.setCentralWidget(widget)
        mainLayout = QtWidgets.QHBoxLayout(widget)

        # 
        leftCol = QtWidgets.QVBoxLayout()

        # Información del sensor
        groupInfo = QtWidgets.QGroupBox("Información del Sensor")
        infoLayout = QtWidgets.QFormLayout(groupInfo)
        infoLayout.addRow("ID:", QtWidgets.QLabel(f"Sensor {sensor_id}"))
        infoLayout.addRow("Tipo:", QtWidgets.QLabel(tipo_sensor))
        infoLayout.addRow("Estado:", QtWidgets.QLabel(" Activo"))
        leftCol.addWidget(groupInfo)

        # Mensaje generado
        groupMsg = QtWidgets.QGroupBox("Mensaje detectado")
        msgLayout = QtWidgets.QVBoxLayout(groupMsg)
        self.txtMensaje = QtWidgets.QTextEdit()
        self.txtMensaje.setReadOnly(True)
        self.txtMensaje.setText(mensaje)
        msgLayout.addWidget(self.txtMensaje)

        # Error solicitado
        groupErrUser = QtWidgets.QGroupBox("Error solicitado")
        errUserLayout = QtWidgets.QVBoxLayout(groupErrUser)
        self.lblErrorSolicitado = QtWidgets.QLabel(error_user)
        errUserLayout.addWidget(self.lblErrorSolicitado)

        # Detección error
        groupErrDet = QtWidgets.QGroupBox("Detección de error")
        errDetLayout = QtWidgets.QVBoxLayout(groupErrDet)
        self.lblErrorDet = QtWidgets.QLabel(error_detect)
        errDetLayout.addWidget(self.lblErrorDet)

        # Columna izquierda: información mensajes
        leftCol.addWidget(groupMsg)
        leftCol.addWidget(groupErrUser)
        leftCol.addWidget(groupErrDet)
        leftCol.addStretch()

        # Columna derecha: gráfica
        self.rightColWidget = QtWidgets.QWidget()
        rightCol = QtWidgets.QVBoxLayout(self.rightColWidget)

        # Mensaje de aviso superación umbral
        self.toast = QtWidgets.QLabel(self)
        self.toast.setStyleSheet("""
            background-color: rgba(255, 200, 200, 230);
            color: red;
            border: 2px solid red;
            border-radius: 8px;
            padding: 8px;
            font-weight: bold;
        """)
        self.toast.setAlignment(QtCore.Qt.AlignCenter)
        self.toast.hide()   # Ocultarlo por defecto

        self.canvas = FigureCanvas(Figure(figsize=(5, 3)))
        self.ax = self.canvas.figure.subplots()

        rightCol.addWidget(self.canvas)

        # Ajuste ejes gráfica según tipo sensor
        self.configurar_grafica(tipo_sensor)
        
        # Layout de las dos columnas
        mainLayout.addLayout(leftCol, 40)
        mainLayout.addWidget(self.rightColWidget, 60)

    # ---------------------------------------------------------
    # Actualizar detección de error
    # ---------------------------------------------------------
    def set_error_detect(self, error_text):
        self.lblErrorDet.setText(error_text)
        
        # Cambiar color según si hay error o no
        if "Ningún error detectado" in error_text:
            self.lblErrorDet.setStyleSheet("color: green; font-weight: bold; background-color: #E6FFE6; padding: 8px; border: 2px solid green; border-radius: 5px;")
        elif  "ERR-" in error_text or "ERROR" in error_text.upper():
            self.lblErrorDet.setStyleSheet("color: red; font-weight: bold; background-color: #FFE6E6; padding: 8px; border: 2px solid red; border-radius: 5px;")

    # ---------------------------------------------------------
    # Configurar gráfica según tipo de sensor
    # ---------------------------------------------------------
    def configurar_grafica(self, tipo):
        self.ax.clear()

        if tipo == "Acelerómetro":

            self.ax.set_ylim(400, 700)
            self.ax.set_title("Aceleración en Tiempo Real", fontweight='bold')
            self.ax.set_ylabel("Aceleración", fontweight='bold')
            self.ax.set_xlabel("Tiempo (5s)", fontweight='bold')
            self.ax.grid(True, alpha=0.3)
            self.line, = self.ax.plot([], [], 'b-', linewidth=2, label='Aceleración')
            self.ax.legend()

        elif tipo == "Termómetro":
            self.ax.set_ylim(20, 40)
            self.ax.set_title("Temperatura Corporal en Tiempo Real", fontweight='bold')
            self.ax.set_ylabel("Temperatura (°C)", fontweight='bold')
            self.ax.set_xlabel("Tiempo (5s)", fontweight='bold')
            self.ax.grid(True, alpha=0.3)
            self.line, = self.ax.plot([], [], 'b-', linewidth=2, label='Temperatura')
            self.ax.legend()

        elif tipo == "Pulsioxímetro":
            self.ax.set_ylim(60, 100)
            self.ax.set_title("Saturación de Oxígeno en Tiempo Real", fontweight='bold')
            self.ax.set_ylabel("SpO₂ (%)", fontweight='bold')
            self.ax.set_xlabel("Tiempo (5s)", fontweight='bold')
            self.ax.grid(True, alpha=0.3)
            self.line, = self.ax.plot([], [], 'b-', linewidth=2, label='SpO₂')
            self.ax.legend()

        elif tipo == "Tensiómetro":
            self.ax.set_ylim(50, 200)
            self.ax.set_title("Presión Arterial en Tiempo Real", fontweight='bold')
            self.ax.set_ylabel("Presión (mmHg)", fontweight='bold')
            self.ax.set_xlabel("Tiempo (5s)", fontweight='bold')
            self.ax.grid(True, alpha=0.3)
            self.ax.axhline(y=120, color='r', linestyle='--', alpha=0.7, label='Sistólica Normal')
            self.ax.axhline(y=80, color='b', linestyle='--', alpha=0.7, label='Diastólica Normal')
            self.ax.legend()

        # Autoscaling dinámico del eje tiempo
        self.ax.relim()
        self.ax.autoscale()
        self.canvas.draw()

    # ---------------------------------------------------------
    # Configurar aviso de superación umbrales
    # ---------------------------------------------------------
    def mostrar_toast(self, mensaje, duration=3000):
        # duration: milisegundos tras los cuales se oculta el aviso

        self.toast.setText(mensaje)

        self.toast.setStyleSheet("""
            background-color: rgba(255, 200, 200, 230);
            color: red;
            border: 2px solid red;
            border-radius: 8px;
            padding: 8px;
            font-weight: bold;
            font-size: 16px;
        """)

        self.toast.adjustSize()
        
        # Posicionar en esquina superior derecha
        margin = 20
        x = self.width() - self.toast.width() - margin
        y = margin
        self.toast.move(x, y)
        
        self.toast.show()

        # Temporizador para ocultarlo después de X ms
        QtCore.QTimer.singleShot(duration, self.toast.hide)

            
    # ---------------------------------------------------------
    # Actualizar gráfica con datos reales
    # ---------------------------------------------------------   
    def update_graph(self, value):
        try:
            timestamp = datetime.now()

            # Tensiómetro: formato "120/80"
            if self.tipo_sensor == "Tensiómetro" and isinstance(value, str) and "/" in value:

                try:
                    sistolica, diastolica = map(float, value.split("/"))
                    valor_actual = sistolica  # establecer la sistólica como valor para umbrales
                    self.datos.append((sistolica, diastolica))
                    self.tiempos.append(timestamp)

                except ValueError:
                    print(f"Error: Formato de tensión inválido: {value}")
                    return

            # Resto sensores: valor numérico
            else:
                try:
                    valor_num = float(value)
                    valor_actual = valor_num   # establecer valor para umbrales
                    self.datos.append(valor_num)
                    self.tiempos.append(timestamp)

                except ValueError:
                    print(f"Error: Formato de sensor inválido: {value}")
                    return

            # Limitar puntos
            if len(self.datos) > self.max_puntos:
                self.datos = self.datos[-self.max_puntos:]
                self.tiempos = self.tiempos[-self.max_puntos:]

            # Actualizar gráfica
            self.actualizar_grafica()

            # Detección umbrales
            if self.umbral_min is not None or self.umbral_max is not None:

                if self.umbral_min is not None and valor_actual < self.umbral_min:
                    self.mostrar_toast("⚠ Límite inferior superado")
                    return

                if self.umbral_max is not None and valor_actual > self.umbral_max:
                    self.mostrar_toast("⚠ Límite superior superado")
                    return

                if "ERR" not in self.lblErrorDet.text():
                    self.set_error_detect("Ningún error detectado")

        except Exception as e:
            print(f"Error actualizando gráfica: {e}")

    # ---------------------------------------------------------
    # Redibujar gráfica
    # ---------------------------------------------------------  
    def actualizar_grafica(self):
        self.ax.clear()
        self.configurar_grafica(self.tipo_sensor) # Reconfigurar ejes y estilo
        
        if not self.datos:
            return
            
        # Preparar datos
        x_data = range(len(self.datos))
        
        if self.tipo_sensor == "Tensiómetro":
            # Datos para tensiómetro (sistólica y diastólica)
            sistolicas = [d[0] for d in self.datos if isinstance(d, tuple)]
            diastolicas = [d[1] for d in self.datos if isinstance(d, tuple)]
            
            # Dibujar las dos líneas
            self.ax.plot(x_data[-len(sistolicas):], sistolicas, 'r-', linewidth=2, label='Sistólica')
            self.ax.plot(x_data[-len(diastolicas):], diastolicas, 'b-', linewidth=2, label='Diastólica')
            
            # Añadir anotación del último valor
            if sistolicas and diastolicas:
                ultimo_idx = len(sistolicas) - 1
                ultima_sist = sistolicas[-1]
                ultima_diast = diastolicas[-1]
                
                self.ax.annotate(f'S: {ultima_sist}', 
                            xy=(ultimo_idx, ultima_sist), 
                            xytext=(10, 10), textcoords='offset points', 
                            color='red', fontweight='bold', fontsize=9)
                self.ax.annotate(f'D: {ultima_diast}', 
                            xy=(ultimo_idx, ultima_diast), 
                            xytext=(10, -15), textcoords='offset points', 
                            color='blue', fontweight='bold', fontsize=9)
                
        else:
            # Datos para otros sensores
            valores = [d for d in self.datos if not isinstance(d, tuple)]
            self.ax.plot(x_data[-len(valores):], valores, linewidth=2)
            
            # Añadir anotación del último valor
            if valores:
                ultimo_valor = valores[-1]
                self.ax.annotate(f'{ultimo_valor:.1f}', 
                            xy=(len(valores)-1, ultimo_valor), 
                            xytext=(10, 10), textcoords='offset points',
                            fontweight='bold', fontsize=9)
        
        # Ajustar límites del eje X
        self.ax.set_xlim(0, len(self.datos))
        
        self.canvas.draw()
    
    # Actualizar texto del mensaje
    def set_message(self, msg):
        self.txtMensaje.setText(msg)
    
    # Manejar cierre de ventana
    def closeEvent(self, event):
        self.datos.clear()
        self.tiempos.clear()
        event.accept()