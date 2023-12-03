import sys
from PyQt5.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QLabel, QGridLayout, QHBoxLayout, QProgressBar
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
from PyQt5.QtCore import Qt
import time
import socket
import numpy as np
import threading
import struct



class LiveGraph(FigureCanvas):
    def __init__(self, max, min, parent=None, width=5, height=4, dpi=100):
        fig = Figure(figsize=(width, height), dpi=dpi)
        self.axes = fig.add_subplot(111)
        box = self.axes.get_position()
        self.axes.set_position([box.x0, box.y0, box.width * 0.8, box.height])
        FigureCanvas.__init__(self, fig)
        self.setParent(parent)

    def update_graph(self, data, differ, labels=""):
        self.axes.clear()
        rate = 1
        for idx, item in enumerate(data):
            self.axes.plot(-1 * differ, item)
        self.axes.legend(labels, loc='center left', bbox_to_anchor=(1, 0.5))
        if data[0][0] > 5:
            self.axes.set_ylim(10, 100)
        else:
            self.axes.set_ylim(0, 6)
        self.axes.set_xlabel("Time (s)")
        self.draw()

class BatteryCellWidget(QWidget):
    def __init__(self, num_cells=16):
        super().__init__()
        self.num_cells = num_cells
        self.initUI()

    def initUI(self):
        layout = QVBoxLayout()

        self.cell_progress_bars = []
        for i in range(self.num_cells):
            cell_layout = QHBoxLayout()
            label = QLabel(f"Cell {i}")
            progress_bar = QProgressBar()
            progress_bar.setAlignment(Qt.AlignCenter)
            progress_bar.setMaximum(100)  # Assuming voltage is represented as a percentage

            cell_layout.addWidget(label)
            cell_layout.addWidget(progress_bar)
            layout.addLayout(cell_layout)

            self.cell_progress_bars.append(progress_bar)

        self.setLayout(layout)

    def update_cell_voltage(self, voltages):
        for idx, voltage in enumerate(voltages):
            # Assuming voltage is given as a percentage. If it's in another format, convert it here
            self.cell_progress_bars[idx].setValue(int(voltage))
        
            
        
def append_to_ring_buffer(ring_buffer, new_data, max_size):
    ring_buffer = np.concatenate((ring_buffer, new_data.T), axis=1)
    if len(ring_buffer[0]) > max_size:
        # Remove the oldest element
        ring_buffer = ring_buffer[:, 1:]
    return ring_buffer

class MainWindow(QMainWindow):
    
    def __init__(self):
        super().__init__()

        layout = QGridLayout()
        self.cell_voltage_graph = LiveGraph(6, 0, width=5, height=4)
        self.cell_temperature_graph = LiveGraph(100, 20, width=5, height=4)
        self.ring_buffer_size = 10
        self.cell_display = BatteryCellWidget(num_cells=16)
        self.voltage_buffer = np.zeros((16, 1))
        self.temp_buffer = np.zeros((8, 1))
        layout.addWidget(self.cell_voltage_graph, 0, 0)
        # layout.addWidget(self.cell_display, 0, 1)
        layout.addWidget(self.cell_temperature_graph, 2, 0)

        self.error_states = [QLabel(f"Error State {i+1}: OFF", self) for i in range(6)]
        for idx, label in enumerate(self.error_states):
            layout.addWidget(label, idx + 3, 0)

        self.info_label = QLabel("Info: Waiting for data...", self)
        layout.addWidget(self.info_label, 9, 0)

        widget = QWidget()
        widget.setLayout(layout)
        self.setCentralWidget(widget)

        self.udp_thread = threading.Thread(target=self.udp_listener)
        self.udp_thread.daemon = True
        self.udp_thread.start()
        self.time_recieved = 0
        self.diff = np.ones(1) * time.time()
        
        self.setWindowTitle("Battery Monitoring System")
        self.setGeometry(0, 0, 500, 500)
        self.showMaximized()

    def udp_listener(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind(('0.0.0.0', 10244))
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 200)

        while True:
            data, addr = sock.recvfrom(1024)  # buffer size is 1024 bytes
            self.process_data(data)

    def process_data(self, data):
        # Process your data here and update graphs
        # For example, split the data into voltage and temperature readings
        # This is a placeholder for your actual data processing logic
        cur_time = time.time()
        oldest_value = self.diff[-1]
        self.diff += cur_time - self.time_recieved
        self.diff = np.append(self.diff, cur_time - self.time_recieved)
        self.time_recieved = time.time()
        # print(self.diff)
        if self.diff.size > 10:
            self.diff = np.delete(self.diff, 0)
        
        data_format = "1H16H8H" # module number, cell voltages, cell temps
        unpacked_data = struct.unpack(data_format, data)
        
        self.voltage_buffer = append_to_ring_buffer(self.voltage_buffer, np.array(unpacked_data[1:17], ndmin=2) / 100, self.ring_buffer_size)
        self.temp_buffer = append_to_ring_buffer(self.temp_buffer, np.array(unpacked_data[17:25], ndmin=2) / 100, self.ring_buffer_size)
        
        # Update the GUI elements
        cell_labels = [f"Cell {i}" for i in range(16)]
        temp_labels = [f"Cell {i}" for i in range(8)]
        
        self.cell_voltage_graph.update_graph(self.voltage_buffer, self.diff, cell_labels)
        self.cell_temperature_graph.update_graph(self.temp_buffer, self.diff, temp_labels)
        
        # self.cell_display.update_cell_voltage((self.voltage_buffer[:, -1] - 3.2) / (4.2 - 3.2) * 100)
        # Update error states and info label as required

if __name__ == '__main__':
    app = QApplication(sys.argv)
    main = MainWindow()
    main.show()
    sys.exit(app.exec_())