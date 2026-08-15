#!/usr/bin/env python3
import sys
import os
import time
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
                             QLabel, QFrame, QHBoxLayout, QDesktopWidget)
from PyQt5.QtCore import Qt, QTimer, QPoint, QThread, pyqtSignal, QObject
from PyQt5.QtGui import QPainter, QPalette, QColor, QPixmap

# FIFO/Command emitter
class CommandEmitter(QObject):
    command_signal = pyqtSignal(str)

class FifoReader(QThread):
    def __init__(self, fifo_path, emitter):
        super().__init__()
        self.fifo_path = fifo_path
        self.emitter = emitter
        self.running = True

    def run(self):
        while self.running:
            try:
                with open(self.fifo_path, 'r') as fifo:
                    while self.running:
                        line = fifo.readline()
                        if not line:
                            break
                        command = line.strip()
                        if command:
                            print(f"Komut alındı: {command}")
                            self.emitter.command_signal.emit(command)
            except Exception as e:
                if self.running:
                    print(f"FIFO okuma hatası: {e}")
                time.sleep(0.5)

    def stop(self):
        self.running = False
        self.wait()

# Rotated widget
class RotatedWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.original_widget = BusPaymentScreen()
        self.original_widget.setParent(self)

    def resizeEvent(self, event):
        size = event.size()
        self.original_widget.resize(size.width(), size.height())
        self.original_widget.move(0, 0)

    def paintEvent(self, event):
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        width = self.width()
        height = self.height()
        painter.translate(width / 2, height / 2)
        painter.rotate(90)
        painter.translate(-height / 2, -width / 2)
        self.original_widget.render(painter, QPoint(0, 0))

class BusPaymentScreen(QWidget):
    def __init__(self):
        super().__init__()

        # spinner
        self.spinner_chars = ['⠋','⠙','⠹','⠸','⠼','⠴','⠦','⠧','⠇','⠏']
        self.spinner_index = 0
        self.spinner_timer = QTimer(self)
        self.spinner_timer.setInterval(120)
        self.spinner_timer.timeout.connect(self._advance_spinner)

        # emitter / fifo
        self.emitter = CommandEmitter()
        self.emitter.command_signal.connect(self.process_command)

        # settings
        self.default_fare = "₼0.50"
        self.fare_amount = self.default_fare
        self.success_text = "Ödəmə Uğurludur"
        self.failure_text = "Ödəmə Uğursuzdur"
        self.success_color = "#28a745"
        self.failure_color = "#dc3545"
        self.success_icon = "✓"
        self.failure_icon = "✗"
        self.card_visible = False
        self.last_status = None  # None / "success" / "fail"

        # UI
        self.initUI()

        # card hide timer
        self.card_timer = QTimer(self)
        self.card_timer.setSingleShot(True)
        self.card_timer.timeout.connect(self._hide_card_after_timeout)
        self.card_timeout_ms = 5000  # 5 seconds

        # FIFO setup
        self.setup_fifo()

    def initUI(self):
        main_layout = QVBoxLayout(self)
        main_layout.setSpacing(0)
        main_layout.setContentsMargins(0, 0, 0, 0)

        # header
        header_frame = QFrame()
        header_frame.setFixedHeight(80)
        header_frame.setStyleSheet("background-color: #007bff;")
        header_layout = QHBoxLayout(header_frame)
        header_layout.setContentsMargins(0, 0, 0, 0)
        header_label = QLabel("YERI Validator")
        header_label.setAlignment(Qt.AlignCenter)
        header_label.setStyleSheet("color: white; font-size: 28px; font-weight: bold;")
        header_layout.addWidget(header_label)
        main_layout.addWidget(header_frame)

        # content
        content_frame = QFrame()
        content_frame.setStyleSheet("background-color: white;")
        content_layout = QVBoxLayout(content_frame)
        content_layout.setSpacing(15)
        content_layout.setContentsMargins(20, 20, 20, 20)
        content_layout.addSpacing(20)

        # fare label (or PAN when shown)
        self.fare_label = QLabel(self.fare_amount)
        self.fare_label.setAlignment(Qt.AlignCenter)
        self.fare_label.setStyleSheet("font-size: 40px; font-weight: bold; color: #333;")
        content_layout.addWidget(self.fare_label)

        # expiry label
        self.expiry_label = QLabel("")
        self.expiry_label.setAlignment(Qt.AlignCenter)
        self.expiry_label.setStyleSheet("font-size: 24px; color: #333;")
        content_layout.addWidget(self.expiry_label)

        # credit-card image (visible when idle showing fare)
        self.card_img = QLabel()
        self.card_img.setAlignment(Qt.AlignCenter)
        img_path = "/home/atilhan/images/credit-card.png"
        if os.path.exists(img_path):
            pix = QPixmap(img_path).scaled(300, 190, Qt.KeepAspectRatio, Qt.SmoothTransformation)
            self.card_img.setPixmap(pix)
        else:
            self.card_img.setText("")  # no image
        content_layout.addWidget(self.card_img)
        self.card_img.setVisible(True)

        # spinner + processing text
        self.spinner_label = QLabel("")
        self.spinner_label.setAlignment(Qt.AlignCenter)
        self.spinner_label.setStyleSheet("font-size: 36px; color: #666;")
        self.spinner_label.setVisible(False)
        content_layout.addWidget(self.spinner_label)

        self.processing_label = QLabel("") 
        self.processing_label.setAlignment(Qt.AlignCenter)
        self.processing_label.setStyleSheet("font-size: 18px; color: #666;")
        self.processing_label.setVisible(False)
        content_layout.addWidget(self.processing_label)

        # status icon and label
        status_layout = QHBoxLayout()
        status_layout.setSpacing(20)
        status_layout.addStretch()

        self.status_icon = QLabel(self.success_icon)
        self.status_icon.setAlignment(Qt.AlignCenter)
        self.status_icon.setFixedSize(100, 100)
        self.status_icon.setStyleSheet(f"""
            background-color: {self.success_color};
            border-radius: 50px;
            color: white;
            font-size: 60px;
            font-weight: bold;
            border: none;
        """)
        self.status_icon.setVisible(False)
        status_layout.addWidget(self.status_icon)
        status_layout.addStretch()
        content_layout.addLayout(status_layout)

        self.status_label = QLabel("")
        self.status_label.setAlignment(Qt.AlignCenter)
        self.status_label.setStyleSheet("font-size: 24px; color: #333;")
        self.status_label.setVisible(False)
        content_layout.addWidget(self.status_label)

        content_layout.addSpacing(20)
        main_layout.addWidget(content_frame)

        # initial state: show fare + card image
        self._hide_card_after_timeout()

    # spinner functions
    def _start_spinner(self):
        self.spinner_index = 0
        self.spinner_label.setText(self.spinner_chars[self.spinner_index])
        self.spinner_label.setVisible(True)
        self.processing_label.setText("Prosess Emal edilir")
        self.processing_label.setVisible(True)
        self.spinner_timer.start()

    def _stop_spinner(self):
        try:
            self.spinner_timer.stop()
        except:
            pass
        self.spinner_label.setVisible(False)
        self.processing_label.setVisible(False)

    def _advance_spinner(self):
        self.spinner_index = (self.spinner_index + 1) % len(self.spinner_chars)
        self.spinner_label.setText(self.spinner_chars[self.spinner_index])

    # show/hide card
    def _show_card(self, pan, expiry):
        self.card_visible = True
        # hide idle card image
        self.card_img.setVisible(False)
        self.set_pan_number(pan)
        self.set_expiry_date(expiry)

        # if we already have last_status apply, otherwise spinner
        if self.last_status == "success":
            self._apply_status(True)
            self._stop_spinner()
        elif self.last_status == "fail":
            self._apply_status(False)
            self._stop_spinner()
        else:
            self.status_icon.setVisible(False)
            self.status_label.setVisible(False)
            self._start_spinner()

        # timer reset
        self.card_timer.start(self.card_timeout_ms)

    def _hide_card_after_timeout(self):
        self.card_visible = False
        self.last_status = None
        self._stop_spinner()
        self.fare_label.setText(self.default_fare)
        self.expiry_label.setText("")
        self.status_icon.setVisible(False)
        self.status_label.setVisible(False)
        # show idle card image
        self.card_img.setVisible(True)

    def set_pan_number(self, pan):
        self.fare_label.setText(f"{pan}")

    def set_expiry_date(self, expiry):
        if len(expiry) == 4:
            self.expiry_label.setText(f"Expiry: {expiry[2:]}/{expiry[:2]}")
        else:
            self.expiry_label.setText(f"Expiry: {expiry}")

    def _apply_status(self, success):
        # stop spinner
        self._stop_spinner()

        if success:
            self.status_icon.setStyleSheet(f"""
                background-color: {self.success_color};
                border-radius: 50px;
                color: white;
                font-size: 60px;
                font-weight: bold;
                border: none;
            """)
            self.status_icon.setText(self.success_icon)
            self.status_label.setText(self.success_text)
            self.last_status = "success"
        else:
            self.status_icon.setStyleSheet(f"""
                background-color: {self.failure_color};
                border-radius: 50px;
                color: white;
                font-size: 60px;
                font-weight: bold;
                border: none;
            """)
            self.status_icon.setText(self.failure_icon)
            self.status_label.setText(self.failure_text)
            self.last_status = "fail"

        self.status_icon.setVisible(True)
        self.status_label.setVisible(True)

        # restart card timer so it will hide after timeout
        if self.card_visible:
            self.card_timer.start(self.card_timeout_ms)

    # FIFO setup
    def setup_fifo(self):
        self.fifo_path = "/tmp/bus_payment_control"
        try:
            if not os.path.exists(self.fifo_path):
                os.mkfifo(self.fifo_path)
        except Exception as e:
            print(f"FIFO oluşturma hatası: {e}")
        self.fifo_reader = FifoReader(self.fifo_path, self.emitter)
        self.fifo_reader.start()

    def process_command(self, command):
        print(f"Komut işleniyor: {command}")

        if command.startswith("PAN:"):
            parts = command.split(';')
            pan = parts[0].split(':', 1)[1] if ':' in parts[0] else ""
            expiry = ""
            if len(parts) > 1 and parts[1].startswith("EXP:"):
                expiry = parts[1].split(':', 1)[1]
            self.last_status = None
            self._show_card(pan, expiry)
            return

        # status token: "0" = success, "1" = fail
        if command == "0" or command == "1":
            success = (command == "0")
            self.last_status = "success" if success else "fail"
            if self.card_visible:
                self._apply_status(success)
            return

        if command == "exit":
            self.close_app()
            return

        print("Bilinmeyen komut formatı:", command)

    def keyPressEvent(self, event):
        if event.key() == Qt.Key_Escape:
            self.close_app()
        elif event.key() == Qt.Key_1:
            self.last_status = "fail"
            if self.card_visible:
                self._apply_status(False)
        elif event.key() == Qt.Key_0:
            self.last_status = "success"
            if self.card_visible:
                self._apply_status(True)

    def close_app(self):
        print("Uygulama kapatılıyor...")
        if hasattr(self, 'fifo_reader'):
            self.fifo_reader.stop()
        QApplication.quit()

class RotatedMainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Bus Payment")
        self.setWindowFlags(Qt.FramelessWindowHint)
        self.rotated_widget = RotatedWidget(self)
        self.setCentralWidget(self.rotated_widget)
        screen = QDesktopWidget().screenGeometry()
        self.setFixedSize(screen.width(), screen.height())
        self.show()
        self.rotated_widget.resize(self.size())

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if hasattr(self, 'rotated_widget'):
            self.rotated_widget.resize(self.size())

    def closeEvent(self, event):
        if hasattr(self, 'rotated_widget') and hasattr(self.rotated_widget, 'original_widget'):
            if hasattr(self.rotated_widget.original_widget, 'fifo_reader'):
                self.rotated_widget.original_widget.fifo_reader.stop()
        QApplication.quit()
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyle('Fusion')
    palette = QPalette()
    palette.setColor(QPalette.Window, QColor(255, 255, 255))
    app.setPalette(palette)

    window = RotatedMainWindow()
    window.show()

    print("Uygulama çalışıyor. Kontroller:")
    print("  echo 'PAN:7123456;EXP:2405' > /tmp/bus_payment_control  -> Kart ve expiry göster")
    print("  echo '0' > /tmp/bus_payment_control  -> API başarılı (success)")
    print("  echo '1' > /tmp/bus_payment_control  -> API başarısız (fail)")
    print("  echo 'exit' > /tmp/bus_payment_control -> Kapat")

    sys.exit(app.exec_())
