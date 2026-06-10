from PyQt5 import QtWidgets

class NonORUWindow(QtWidgets.QMainWindow):
    def __init__(self, tipo, mensaje, error_detectado="Ninguno"):
        super().__init__()
        self.setWindowTitle(f"Mensaje {tipo} - Detectado")
        self.resize(600, 400)

        widget = QtWidgets.QWidget()
        self.setCentralWidget(widget)

        layout = QtWidgets.QVBoxLayout(widget)

        # Título
        titulo = QtWidgets.QLabel(f"Mensaje {tipo} recibido")
        titulo.setStyleSheet("font-size:18px; font-weight:bold;")
        layout.addWidget(titulo)

        # Mensaje detectado
        groupMsg = QtWidgets.QGroupBox("Mensaje detectado")
        msgLayout = QtWidgets.QVBoxLayout(groupMsg)
        self.txtMensaje = QtWidgets.QTextEdit()
        self.txtMensaje.setReadOnly(True)
        self.txtMensaje.setText(mensaje)
        msgLayout.addWidget(self.txtMensaje)
        layout.addWidget(groupMsg)

        # Error detectado
        groupErr = QtWidgets.QGroupBox("Detección de error")
        errLayout = QtWidgets.QVBoxLayout(groupErr)
        self.lblError = QtWidgets.QLabel(error_detectado)
        self.lblError.setStyleSheet(
            "color:red; font-weight:bold; background:#ffe6e6; padding:8px; border:1px solid red;"
            if "ERR" in error_detectado
            else "color:green; font-weight:bold; background:#e6ffe6; padding:8px; border:1px solid green;"
        )
        errLayout.addWidget(self.lblError)
        layout.addWidget(groupErr)

        layout.addStretch()
