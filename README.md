Bluetooth RFCOMM Device Controller (epoll + HSM + GPIO + CRC16)

這是一個以 Linux RFCOMM (Bluetooth) 為基礎的裝置控制示範專案，使用：
- **epoll**：建立非阻塞（Non-blocking）I/O 事件迴圈
- **自訂封包格式**：STX、LEN、CMD、Payload、CRC16-Modbus
- **Hierarchical State Machine (HSM)**：嚴謹處理連線、握手、命令狀態切換
- **libgpiod**：控制 Linux GPIO
- **CRC16-Modbus**：確保終端封包傳輸完整性

此專案展示了完整的「藍牙連線 → 封包解析 → 狀態機 → GPIO 控制」流程，適合作為系統程式、嵌入式、IoT、Linux daemon 的作品集。

---

## 專案目錄結構（Directory Structure）

```text
project/
│
├── src/
│   ├── main.c
│   ├── rfcomm_epoll.c
│   ├── app_hsm.c
│   ├── gpio.c
│   ├── crc16.c
│   ├── log.c
│   └── *.h
│
├── tools/
│   └── python_client.py
│
├── diagrams/
│   ├── Architecture Diagram.png
│   └── HSM State Machine.png
│
├── LICENSE
└── README.md
```

---

## 系統架構圖（Architecture Diagram）

```text

┌────────────────────────────────────┐
│        Bluetooth RFCOMM            │ 
│   (AF_BLUETOOTH / SOCK_STREAM)     │   
└──────────┬─────────────────────────┘
   EPOLL   │
           │
           ▼
┌────────────────────────────────────┐
│   RX Buffer                        │              
│ (STX / LEN / CMD)                  │              
└──────────┬─────────────────────────┘
           ▼
┌────────────────────────────────────┐
│  Frame Parser                      │              
│  CRC16-Modbus                      │              
└──────────┬─────────────────────────┘
           ▼
┌────────────────────────────────────┐
│   HSM State Machine                |
│  (Disconnected / Handshake / Idle) |
└──────────┬─────────────────────────┘
           ▼
┌────────────────────────────────────┐
│      GPIO                          │              
│   (libgpiod)                       │              
└──────────┬─────────────────────────┘
```

## 封包格式（Protocol）

```text

[0] 0x55          STX1
[1] 0xAA          STX2
[2] LEN           CMD + PAYLOAD 長度
[3] CMD           指令代碼
[4..] PAYLOAD     資料
[N-2] CRC_LO      CRC16-Modbus (低位元)
[N-1] CRC_HI      CRC16-Modbus (高位元)
```

範例：HELLO 封包

`55 AA 01 01 CRC_LO CRC_HI`

範例：

`SET_GPIO(1)` 封包為 `55 AA 02 10 01 CRC_LO CRC_HI`

---

## 支援指令（Commands）

|CMD    |名稱                   |說明                          |
| :---  | :---                  | :---                         |
|0x01   |HELLO                  |Client → Server 握手封包      |
|0x81   |HELLO_ACK              |Server → Client 回應握手      |
|0x10   |SET_GPIO               |GPIO（payload = 0x00 或 0x01）|
---

HSM 狀態圖（State Machine）

```text
┌──────────────┐
│ DISCONNECTED │
└───────┬──────┘
 EVT_LINK_UP
        ▼
┌──────────────┐
│  HANDSHAKE   │
│  等待 HELLO  │ 
└───────┬──────┘
 HELLO 收到     
        ▼
┌──────────────┐
│   CMD_IDLE   │           
│  處理命令    │            
└──────────────┘

```

## 編譯方式（Build for **Manjaro / Arch Linux**）

- `libgpiod`：Linux GPIO 控制
- `bluez-libs`：BlueZ 藍牙協議（提供 RFCOMM 支援）

```bash
sudo pacman -S libgpiod bluez-libs
```

## 編譯指令
```bash
make
```

## 執行方式（Run）

```bash
sudo ./blt_hsm
```

程式啟動後將依序執行：
1. 建立 RFCOMM server（預設 Channel 3）
2. 等待 client 連線
3. 進入握手流程（HELLO → HELLO_ACK）
4. 進入命令模式（接收 SET_GPIO）

### 測試方式（Testing）

1. 使用 Linux rfcomm client 建立通道
```bash
sudo rfcomm connect /dev/rfcomm0 <server-mac> 3
```
2. 使用 Python 發送測試封包可以使用以下手稿透過產生的虛擬序列發送控制指令：


```python
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

```
