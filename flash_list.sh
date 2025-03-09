#!/bin/sh

ADDR=`echo "$1"| tr '[:upper:]' '[:lower:]'`
AUTH="$2"

if [ "$ADDR" = "" -o "$AUTH" = "" ]; then
  echo "Usage: ./flash_beacon.sh MAC_ADDR AUTH_KEY FILE_UPLOAD"
  exit 1;
fi

echo "BLE MAC: $ADDR"
echo "AUTH: $AUTH"
echo ""
echo "Requires sudo for for mcumgr"
sudo echo ""

echo "Switching to mcumgr mode with write_beacon.py"
python3 ./conn_beacon.py  -i "$ADDR" -a "$AUTH"
echo ""

sleep 10
HCIDEV=`sudo hcitool dev | tail -n 1 |awk '{print $1;}'|sed s/'^.*\([0-9]\)'/'\1'/`

echo "Checking image list"
sudo ~/go/bin/mcumgr -i "$HCIDEV" --conntype ble --connstring "peer_id=$ADDR" image list
echo ""
