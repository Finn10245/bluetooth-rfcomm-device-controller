#!/bin/bash
set -e

APP_NAME="blt_hsm"
INSTALL_PATH="/usr/local/bin/$APP_NAME"
SERVICE_PATH="/etc/systemd/system/$APP_NAME.service"

echo "=== 停止服務 ==="
sudo systemctl stop $APP_NAME || true

echo "=== 停用服務 ==="
sudo systemctl disable $APP_NAME || true

echo "=== 刪除 systemd service ==="
if [ -f "$SERVICE_PATH" ]; then
    sudo rm "$SERVICE_PATH"
    echo "已刪除 $SERVICE_PATH"
else
    echo "找不到 $SERVICE_PATH"
fi

echo "=== 刪除執行檔 ==="
if [ -f "$INSTALL_PATH" ]; then
    sudo rm "$INSTALL_PATH"
    echo "已刪除 $INSTALL_PATH"
else
    echo "找不到 $INSTALL_PATH"
fi

echo "=== 重新載入 systemd ==="
sudo systemctl daemon-reload

echo "=== 檢查是否刪除成功 ==="
if systemctl status $APP_NAME 2>&1 | grep -q "could not be found"; then
    echo "服務已完全刪除"
else
    echo "⚠ 服務仍存在，請檢查 systemctl status $APP_NAME"
fi

echo "=== 解除安裝完成 ==="