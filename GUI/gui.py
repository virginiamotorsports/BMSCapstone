import sys
from PyQt5.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QLabel
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
import socket
import numpy as np
import threading
import struct



class LiveGraph(FigureCanvas):
    def __init__(self, parent=None, width=5, height=4, dpi=100):
        fig = Figure(figsize=(width, height), dpi=dpi)
        self.axes = fig.add_subplot(111)
        box = self.axes.get_position()
        self.axes.set_position([box.x0, box.y0, box.width * 0.8, box.height])
        FigureCanvas.__init__(self, fig)
        self.setParent(parent)

    def update_graph(self, data, labels=""):
        self.axes.clear()
        rate = 1
        times = (np.arange(0, data[0].size) - data[0].size) * (1/rate)
        for idx, item in enumerate(data):
            self.axes.plot(times, item)
        self.axes.legend(labels, loc='center left', bbox_to_anchor=(1, 0.5))
        self.draw()
        
def append_to_ring_buffer(ring_buffer, new_data, max_size):
    ring_buffer = np.concatenate((ring_buffer, new_data.T), axis=1)
    if len(ring_buffer[0]) > max_size:
        # Remove the oldest element
        ring_buffer = ring_buffer[:, 1:]
    return ring_buffer

class MainWindow(QMainWindow):
    
    def __init__(self):
        super().__init__()

        self.setWindowTitle("Battery Monitoring System")
        self.setGeometry(100, 100, 800, 600)

        layout = QVBoxLayout()
        self.cell_voltage_graph = LiveGraph(self, width=5, height=4)
        self.cell_temperature_graph = LiveGraph(self, width=5, height=4)
        self.ring_buffer_size = 10
        self.voltage_buffer = np.empty((16, 0), dtype=np.float32)
        self.temp_buffer = np.empty((8, 0), dtype=np.float32)
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
        
        data_format = "16H8H"
        unpacked_data = struct.unpack(data_format, data)
        
        self.voltage_buffer = append_to_ring_buffer(self.voltage_buffer, np.array(unpacked_data[0:16], ndmin=2) / 100, self.ring_buffer_size)
        self.temp_buffer = append_to_ring_buffer(self.temp_buffer, np.array(unpacked_data[16:24], ndmin=2) / 100, self.ring_buffer_size)
        
        # Update the GUI elements
        cell_labels = [f"Cell {i}" for i in range(16)]
        temp_labels = [f"Cell {i}" for i in range(8)]
        
        self.cell_voltage_graph.update_graph(self.voltage_buffer, cell_labels)
        self.cell_temperature_graph.update_graph(self.temp_buffer, temp_labels)
        # Update error states and info label as required

if __name__ == '__main__':
    app = QApplication(sys.argv)
    main = MainWindow()
    main.show()
    sys.exit(app.exec_())