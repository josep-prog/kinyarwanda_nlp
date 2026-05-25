#!/usr/bin/env bash
# deploy.sh — Deploy the Kinyarwanda USSD service on an Ubuntu/Debian VPS.
# Run as root or with sudo on a fresh server.
# Tested on Ubuntu 22.04 LTS.
set -euo pipefail

REPO_DIR="/opt/kinyarwanda_ussd"
SERVICE_NAME="kinyarwanda-ussd"
PYTHON_BIN="python3"

echo "==> Installing system packages"
apt-get update -q
apt-get install -y python3 python3-pip python3-venv nginx certbot python3-certbot-nginx

echo "==> Setting up application directory"
mkdir -p "$REPO_DIR"
cp -r . "$REPO_DIR/"

echo "==> Creating Python virtualenv"
cd "$REPO_DIR"
$PYTHON_BIN -m venv venv
./venv/bin/pip install --upgrade pip
./venv/bin/pip install -r requirements.txt

echo "==> Installing systemd service"
cat > /etc/systemd/system/${SERVICE_NAME}.service << EOF
[Unit]
Description=Kinyarwanda AI USSD Service
After=network.target

[Service]
Type=simple
WorkingDirectory=${REPO_DIR}
EnvironmentFile=${REPO_DIR}/.env
ExecStart=${REPO_DIR}/venv/bin/gunicorn --workers 2 --bind 0.0.0.0:\${PORT:-5000} server:app
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable "$SERVICE_NAME"
systemctl start "$SERVICE_NAME"

echo "==> Configuring nginx reverse proxy"
cat > /etc/nginx/sites-available/ussd << EOF
server {
    listen 80;
    server_name _;

    location /ussd {
        proxy_pass http://127.0.0.1:5000;
        proxy_set_header Host \$host;
        proxy_set_header X-Real-IP \$remote_addr;
        proxy_read_timeout 15s;
    }

    location /health {
        proxy_pass http://127.0.0.1:5000;
    }
}
EOF

ln -sf /etc/nginx/sites-available/ussd /etc/nginx/sites-enabled/ussd
nginx -t && systemctl reload nginx

echo ""
echo "==> Done. Service running at http://$(curl -s ifconfig.me)/ussd"
echo "    Register this URL in Africa's Talking dashboard as your USSD callback."
echo "    To add HTTPS run: certbot --nginx -d your-domain.com"
