import sys
from PyQt5.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QLabel
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import socket
import threading

class LiveGraph(FigureCanvas):
    def __init__(self, parent=None, width=5, height=4, dpi=100):
        fig = Figure(figsize=(width, height), dpi=dpi)
        self.axes = fig.add_subplot(111)
        FigureCanvas.__init__(self, fig)
        self.setParent(parent)

    def update_graph(self, data):
        self.axes.clear()
        self.axes.plot(data)
        self.draw()

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()

        self.setWindowTitle("Battery Monitoring System")
        self.setGeometry(100, 100, 800, 600)

        layout = QVBoxLayout()
        self.cell_voltage_graph = LiveGraph(self, width=5, height=4)
        self.cell_temperature_graph = LiveGraph(self, width=5, height=4)
        layout.addWidget(self.cell_voltage_graph)
        layout.addWidget(self.cell_temperature_graph)

        self.error_states = [QLabel(f"Error State {i+1}: OFF", self) for i in range(6)]
        for label in self.error_states:
            layout.addWidget(label)

        self.info_label = QLabel("Info: Waiting for data...", self)
        layout.addWidget(self.info_label)

        widget = QWidget()
        widget.setLayout(layout)
        self.setCentralWidget(widget)

        self.udp_thread = threading.Thread(target=self.udp_listener)
        self.udp_thread.daemon = True
        self.udp_thread.start()

    def udp_listener(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(('0.0.0.0', 10244))

        while True:
            data, addr = sock.recvfrom(1024)  # buffer size is 1024 bytes
            self.process_data(data)

    def process_data(self, data):
        # Process your data here and update graphs
        # For example, split the data into voltage and temperature readings
        # This is a placeholder for your actual data processing logic
        voltage_data = [0] * 16  # Replace with actual data
        temperature_data = [0] * 8  # Replace with actual data

        # Update the GUI elements
        self.cell_voltage_graph.update_graph(voltage_data)
        self.cell_temperature_graph.update_graph(temperature_data)
        # Update error states and info label as required

if __name__ == '__main__':
    app = QApplication(sys.argv)
    main = MainWindow()
    main.show()
    sys.exit(app.exec_())