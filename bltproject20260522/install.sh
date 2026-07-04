#!/bin/bash
set -e

APP_NAME="blt_hsm"
INSTALL_PATH="/usr/local/bin/$APP_NAME"
SERVICE_PATH="/etc/systemd/system/$APP_NAME.service"

echo "=== 編譯程式 ==="
gcc -o blt_hsm \
    src/main.c \
    src/rfcomm_epoll.c \
    src/app_hsm.c \
    src/log.c \
    src/crc16.c \
    src/gpio.c \
    -lbluetooth -lgpiod

echo "=== 安裝執行檔到 $INSTALL_PATH ==="
sudo cp blt_hsm "$INSTALL_PATH"
sudo chmod +x "$INSTALL_PATH"

echo "=== 安裝 systemd service ==="
sudo bash -c "cat > $SERVICE_PATH" <<EOF
[Unit]
Description=Bluetooth RFCOMM HSM Daemon
After=bluetooth.service

[Service]
ExecStart=$INSTALL_PATH
Restart=always
User=root
Group=root
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

echo "=== 重新載入 systemd ==="
sudo systemctl daemon-reload

echo "=== 啟動服務 ==="
sudo systemctl start $APP_NAME

echo "=== 設定開機自動啟動 ==="
sudo systemctl enable $APP_NAME

echo "=== 安裝完成！ ==="
echo "使用以下指令查看 log："
echo "  journalctl -u $APP_NAME -f"