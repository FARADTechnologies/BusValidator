#!/usr/bin/env bash
set -euo pipefail

LOGDIR=/home/atilhan/logs
mkdir -p "$LOGDIR"
touch "$LOGDIR/start_all.log" "$LOGDIR/card_gui.service.log" "$LOGDIR/card_reader_live.log"
chown -R atilhan:atilhan "$LOGDIR" || true

echo "$(date +%FT%T) [start_all] starting" >> "$LOGDIR/start_all.log"

# 1) eski prosesleri kapat
pkill -f IDTechSDK_Demo 2>/dev/null || true
pkill -f run_ctls_read.exp 2>/dev/null || true
pkill -f card_gui.py 2>/dev/null || true
sleep 1

# 2) FIFO'yu yeniden oluştur
rm -f /tmp/bus_payment_control 2>/dev/null || true
mkfifo /tmp/bus_payment_control
chown atilhan:atilhan /tmp/bus_payment_control || true
echo "$(date +%FT%T) [start_all] fifo ready" >> "$LOGDIR/start_all.log"

# 3) ÖNCE GUI'yi başlat (tek okuyucu bu olacak!)
DISPLAY=:0 XAUTHORITY=/home/atilhan/.Xauthority \
    /usr/bin/python3 /home/atilhan/card_gui.py >> "$LOGDIR/card_gui.service.log" 2>&1 &

# 4) GUI ayağa kalksın diye az bekle
sleep 1

# 5) kart okuyucu/expect
sudo /usr/bin/expect -f /home/atilhan/run_ctls_read.exp >> "$LOGDIR/card_reader_live.log" 2>&1

