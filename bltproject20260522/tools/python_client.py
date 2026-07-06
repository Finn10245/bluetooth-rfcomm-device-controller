import serial
import time

# 請根據實際產生的 rfcomm 裝置路徑修改

SERIAL_PORT = '/dev/rfcomm0'

def crc16_modbus(data: bytearray) -> int:
    crc = 0xFFFF
    for pos in data:
        crc ^= pos
        for _ in range(8):
            if (crc & 1) != 0:
                crc >>= 1
                crc ^= 0xA001
            else:
                crc >>= 1
    return crc

def send(ser, cmd, payload):
    frame = bytearray([0x55, 0xAA])
    frame.append(1 + len(payload))   # LEN = CMD + PAYLOAD
    frame.append(cmd)
    frame.extend(payload)

    # ★ 正確：只對 CMD + PAYLOAD 計算 CRC
    crc_data = bytearray([cmd]) + bytearray(payload)
    crc = crc16_modbus(crc_data)

    frame.append(crc & 0xFF)         # CRC low byte
    frame.append((crc >> 8) & 0xFF)  # CRC high byte

    ser.write(frame)
    print(f"Sent: {frame.hex().upper()}")

if __name__ == '__main__':
    try:
        ser = serial.Serial(SERIAL_PORT, 9600, timeout=1)

        print("1. 發送 HELLO 握手...")
        send(ser, 0x01, [])
        time.sleep(0.5)

        print("2. 控制 GPIO HIGH...")
        send(ser, 0x10, [0x01])
        time.sleep(1)

        print("3. 控制 GPIO LOW...")
        send(ser, 0x10, [0x00])

        ser.close()
    except Exception as e:
        print(f"錯誤: {e}")
