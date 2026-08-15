#!/usr/bin/env bash
set -euo pipefail

# Ayarlar config/validator.env'den gelir, yoksa eski sabit değerlere düşer.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
if [ -f "$REPO_DIR/config/validator.env" ]; then
    # shellcheck disable=SC1091
    . "$REPO_DIR/config/validator.env"
fi

VALIDATOR_USER="${VALIDATOR_USER:-$(id -un)}"
VALIDATOR_HOME="${VALIDATOR_HOME:-$HOME}"
VALIDATOR_FIFO="${VALIDATOR_FIFO:-/tmp/bus_payment_control}"
LOGDIR="${VALIDATOR_LOG_DIR:-$VALIDATOR_HOME/logs}"

export VALIDATOR_FIFO
export VALIDATOR_API_URL VALIDATOR_CARD_DATA VALIDATOR_ASSETS VALIDATOR_FARE

mkdir -p "$LOGDIR"
touch "$LOGDIR/start_all.log" "$LOGDIR/card_gui.service.log" "$LOGDIR/card_reader_live.log"
chown -R "$VALIDATOR_USER:$VALIDATOR_USER" "$LOGDIR" || true

echo "$(date +%FT%T) [start_all] starting" >> "$LOGDIR/start_all.log"

# 1) eski prosesleri kapat
pkill -f IDTechSDK_Demo 2>/dev/null || true
pkill -f run_ctls_read.exp 2>/dev/null || true
pkill -f card_gui.py 2>/dev/null || true
sleep 1

# 2) FIFO'yu yeniden oluştur
rm -f "$VALIDATOR_FIFO" 2>/dev/null || true
mkfifo "$VALIDATOR_FIFO"
chown "$VALIDATOR_USER:$VALIDATOR_USER" "$VALIDATOR_FIFO" || true
echo "$(date +%FT%T) [start_all] fifo ready" >> "$LOGDIR/start_all.log"

# 3) ÖNCE GUI'yi başlat (tek okuyucu bu olacak!)
DISPLAY="${DISPLAY:-:0}" XAUTHORITY="${XAUTHORITY:-$VALIDATOR_HOME/.Xauthority}" \
    /usr/bin/python3 "$REPO_DIR/src/gui/card_gui.py" >> "$LOGDIR/card_gui.service.log" 2>&1 &

# 4) GUI ayağa kalksın diye az bekle
sleep 1

# 5) kart okuyucu/expect
# Yönlendirme sudo'dan etkilenmez, o yüzden log'a tee ile yazılıyor (SC2024).
sudo -E /usr/bin/expect -f "$REPO_DIR/src/reader/run_ctls_read.exp" 2>&1 \
    | tee -a "$LOGDIR/card_reader_live.log"
