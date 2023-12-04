import socket
import struct
import random
import time

def generate_test_data():
    # Generate 16 unsigned shorts for cell voltages (0-65535)
    voltages = ([random.randint(3200, 4200) for _ in range(16)])
    # Generate 8 unsigned shorts for temperatures (0-65535)
    temperatures = [random.randint(200, 1000) for _ in range(8)]
    return voltages, temperatures

def send_udp_packet(ip, port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    while True:
        voltages, temperatures = generate_test_data()
        zero = [0]
        # Pack the data into binary format
        packet = struct.pack('1H16H8H', *zero, *voltages, *temperatures)
        sock.sendto(packet, (ip, port))
        print(f"Sent packet to {ip}:{port}")

        # Wait for 1 second before sending the next packet
        # time.sleep(1)

if __name__ == "__main__":
    ip = "127.0.0.1"  # Replace with the desired IP
    port = 10244          # Replace with the desired port
    send_udp_packet(ip, port)