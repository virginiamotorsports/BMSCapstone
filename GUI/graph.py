
from PyQt5.QtWidgets import QApplication, QMainWindow, QVBoxLayout, QWidget, QLabel, QGridLayout, QHBoxLayout, QProgressBar
from matplotlib.backends.backend_qt5agg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure
from PyQt5.QtCore import Qt

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
            self.axes.set_ylim(2.5, 3.3)
        self.axes.set_xlabel("Time (s)")
        self.draw()