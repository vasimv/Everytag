#!/bin/sh

ADDR=`echo "$1"| tr '[:upper:]' '[:lower:]'`
AUTH="$2"
FILE="$3"

if [ "$ADDR" = "" -o "$AUTH" = "" -o "$FILE" = "" ]; then
  echo "Usage: ./flash_beacon.sh MAC_ADDR AUTH_KEY FILE_UPLOAD"
  exit 1;
fi

echo "BLE MAC: $ADDR"
echo "AUTH: $AUTH"
echo "FILE: $FILE"
echo ""
echo "Requires sudo for for mcumgr"
sudo echo ""

echo "Switching to mcumgr mode with write_beacon.py"
python3 ./write_beacon.py  -i "$ADDR" -a "$AUTH"
echo ""

sleep 10
HCIDEV=`sudo hcitool dev | tail -n 1 |awk '{print $1;}'|sed s/'^.*\([0-9]\)'/'\1'/`

echo "Checking image list"
sudo ~/go/bin/mcumgr -i "$HCIDEV" --conntype ble --connstring "peer_id=$ADDR" image list
echo ""

echo "Erasing unused image (if any)"
sudo ~/go/bin/mcumgr -i "$HCIDEV" --conntype ble --connstring "peer_id=$ADDR" image erase
echo ""

echo "Uploading new image"
sudo ~/go/bin/mcumgr -i "$HCIDEV" --conntype ble --connstring "peer_id=$ADDR" image upload "$FILE"
echo ""

echo "Confirming new image"
HASH=`sudo ~/go/bin/mcumgr -i "$HCIDEV" --conntype ble --connstring "peer_id=$ADDR" image list | grep hash |tail -n 1 | awk '{print $2;}'`
sudo ~/go/bin/mcumgr -i "$HCIDEV" --conntype ble --connstring "peer_id=$ADDR" image confirm "$HASH"
echo ""

#echo "Resetting beacon"
#sudo ~/go/bin/mcumgr -i "$HCIDEV" --conntype ble --connstring "peer_id=$ADDR" reset
#echo ""
