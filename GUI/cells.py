import sys
from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QHBoxLayout, QLabel, QProgressBar
from PyQt5.QtCore import Qt, QRect, QRectF
import numpy as np
from PyQt5.QtGui import *
from math import trunc

class BatteryCellWidget(QWidget):
    def __init__(self):
        super().__init__()

        # Initialize 96 battery cells
        self.cells = [{'voltage': 0, 'max_voltage': 3.7, 'min_voltage': 3.0} for _ in range(96)]

        # Layouts
        main_layout = QVBoxLayout()
        # group_layouts = [QHBoxLayout() for _ in range(6)]

        # Create 96 progress bars and labels
        self.progress_bars = np.zeros(96)
        self.voltage_labels = np.zeros(96)

        # # Add group layouts to main layout
        # for group_layout in group_layouts:
        #     main_layout.addLayout(group_layout)

        self.setLayout(main_layout)
        
    def paintEvent(self, event):  

        qp = QPainter()
        qp.begin(self)           
        for i in range(96):
            width = 10
            spacing = 10
            height = 20
            x = ((i // 16) * width + (i // 16) * spacing)
            y = (trunc((i / 16)) * height + trunc(i / 16) * spacing)
            
            rect1 = QRectF(x, y, width, height)
            rect2 = QRectF(5, 5, 5, 5)
            
            qp.setPen(Qt.black)
            qp.drawRoundedRect(rect1, 2, 2) 
            qp.setPen(Qt.black)
            qp.setBrush(Qt.green)
            # qp.drawRoundedRect(rect2, 2, 2)  

        qp.end()

    def update_voltage(self, cell_index, voltage, max_voltage, min_voltage):
        if 0 <= cell_index < 96:
            # Update cell data
            cell = self.cells[cell_index]
            cell['voltage'] = voltage
            cell['max_voltage'] = max_voltage
            cell['min_voltage'] = min_voltage

            # Calculate charge state
            charge_state = (voltage - min_voltage) / (max_voltage - min_voltage) * 100

            # Update progress bar and label
            self.progress_bars[cell_index] = charge_state
            self.voltage_labels[cell_index] = voltage
            self.repaint()
        else:
            print("Invalid cell index")
            
    def update_voltage_module(self, module_idx, voltages, max_voltage, min_voltage):
        if 0 <= module_idx < 6:
            # Update cell data
            for idx, voltage in enumerate(voltages):
                cell_index = 16 * module_idx + idx
                self.cells[cell_index]['voltage'] = voltage
                self.cells[cell_index]['max_voltage'] = max_voltage
                self.cells[cell_index]['min_voltage'] = min_voltage

                # Calculate charge state
                charge_state = (voltage - min_voltage) / (max_voltage - min_voltage) * 100

                # Update progress bar and label
                self.progress_bars[cell_index] = charge_state
                self.voltage_labels[cell_index] = voltage
            self.repaint()
        else:
            print("Invalid cell index")

if __name__ == "__main__":
    app = QApplication(sys.argv)
    widget = BatteryCellWidget()
    widget.show()
    sys.exit(app.exec_())