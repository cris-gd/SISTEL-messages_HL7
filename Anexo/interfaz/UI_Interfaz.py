from PyQt5 import QtWidgets, QtCore, QtGui

# ============================================================
# USER INTERFACE
# ============================================================
class Ui_MainWindow(object):
    def setupUi(self, MainWindow):
        MainWindow.setObjectName("MainWindow")
        MainWindow.resize(1000, 600)
        MainWindow.setStyleSheet("background-color:white;")

        # Widget central
        self.centralwidget = QtWidgets.QWidget(MainWindow)
        self.mainLayout = QtWidgets.QVBoxLayout(self.centralwidget)

        # Control superior (INICIO / FIN)
        self.groupTop = QtWidgets.QGroupBox("Control general")
        topLayout = QtWidgets.QHBoxLayout(self.groupTop)

        # Botones principales para iniciar y finalizar envío de mensajes
        self.btnInicio = QtWidgets.QPushButton("INICIO")
        self.btnFin = QtWidgets.QPushButton("FIN")

        topLayout.addWidget(self.btnInicio)
        topLayout.addWidget(self.btnFin)
        topLayout.addStretch()

        self.mainLayout.addWidget(self.groupTop)

        # Configuración mensaje Z
        self.groupZ = QtWidgets.QGroupBox("Configuración del mensaje Z")
        zLayout = QtWidgets.QGridLayout(self.groupZ)

        # Tipo de mensaje (ORU, ADT, OMD)
        zLayout.addWidget(QtWidgets.QLabel("Tipo de mensaje:"), 0, 0)
        self.comboTipoMensaje = QtWidgets.QComboBox()
        self.comboTipoMensaje.addItems(["ORU_R01", "ADT_A05", "OMD_O03"])
        zLayout.addWidget(self.comboTipoMensaje, 0, 1)

        # Selector de si existe error en el mensaje Z
        zLayout.addWidget(QtWidgets.QLabel("¿Error?:"), 1, 0)
        self.comboErrorGlobal = QtWidgets.QComboBox()
        self.comboErrorGlobal.addItems(["NO", "SI"])
        zLayout.addWidget(self.comboErrorGlobal, 1, 1)

        # Código de error (solo habilitado si SI)
        zLayout.addWidget(QtWidgets.QLabel("Tipo error:"), 2, 0)
        self.comboCodErrorGlobal = QtWidgets.QComboBox()
        self.comboCodErrorGlobal.addItems(["101","200", "201", "202", "203"])
        self.comboCodErrorGlobal.setEnabled(False)
        zLayout.addWidget(self.comboCodErrorGlobal, 2, 1)

        # Nº sensores para mensejes ORU
        zLayout.addWidget(QtWidgets.QLabel("Número de sensores:"), 3, 0)
        self.comboNumSensORU = QtWidgets.QComboBox()
        self.comboNumSensORU.addItems(["1", "2", "3", "4"])
        zLayout.addWidget(self.comboNumSensORU, 3, 1)

        # Selector dinámico de tipos de sensores ORU
        self.widgetORUSens = QtWidgets.QWidget()
        self.layoutORUSens = QtWidgets.QVBoxLayout(self.widgetORUSens)
        zLayout.addWidget(self.widgetORUSens, 4, 0, 1, 2)

        self.mainLayout.addWidget(self.groupZ)

        # Previsualización del mensaje Z a enviar
        self.groupPreview = QtWidgets.QGroupBox("Vista previa del mensaje Z")
        layoutPrev = QtWidgets.QVBoxLayout(self.groupPreview)

        # Cuadro donde se coloca el mensaje Z
        self.previewText = QtWidgets.QPlainTextEdit()
        self.previewText.setReadOnly(True)
        self.previewText.setStyleSheet("background:#f8f8f8; font-family: Consolas;")
        layoutPrev.addWidget(self.previewText)

        self.mainLayout.addWidget(self.groupPreview)

        # GATEWAY LOG
        self.groupGatewayLog = QtWidgets.QGroupBox("Gateway Log")
        layoutLog = QtWidgets.QVBoxLayout(self.groupGatewayLog)

        # Cuadro donde se muestra todo lo recibido por el serial
        self.gatewayLog = QtWidgets.QPlainTextEdit()
        self.gatewayLog.setReadOnly(True)
        self.gatewayLog.setStyleSheet("background:white; color:blue; font-family: Consolas;")
        layoutLog.addWidget(self.gatewayLog)

        self.mainLayout.addWidget(self.groupGatewayLog)

        MainWindow.setCentralWidget(self.centralwidget)

        self.retranslateUi(MainWindow)

    # ============================================================
    # TEXTOS GENERALES
    # ============================================================
    def retranslateUi(self, MainWindow):
        MainWindow.setWindowTitle("Interfaz HL7 - Ventana Principal")
