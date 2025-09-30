#!/usr/bin/python3
import simplepyble
import time
import argparse
import array
import sys
from datetime import datetime
import binascii
 
def gen_array(num, elem):
  arr = []
  for i in range(num):
    arr.append(elem)
  return arr

ap = argparse.ArgumentParser()

ap.add_argument("-i", "--macid", required=True, help="BLE MAC address")
ap.add_argument("-a", "--auth", required=True, help="Authentication code")
ap.add_argument("-n", "--newauth", required=False, help="New Authentication code")
ap.add_argument("-c", "--newmacid", required=False, help="new MAC address for settings mode")
ap.add_argument("-g", "--fmdn", required=False, help="Google FMDN broadcast setting (0 or 1)")
ap.add_argument("-t", "--airtag", required=False, help="Airtag broadcast setting (0 or 1)")
ap.add_argument("-d", "--delay", required=False, help="Delay between broadcasts (1, 2 or 4)")
ap.add_argument("-r", "--readtime", required=False, help="Read current time on beacon (1 as argument)")
ap.add_argument("-w", "--writetime", required=False, help="Write current time to beacon (1 as argument)")
ap.add_argument("-l", "--interval", required=False, help="Interval between keys change in seconds")
ap.add_argument("-s", "--statusbyte", required=False, help="Set status byte behaviour (hexadecimal), default is 438000")
ap.add_argument("-m", "--movethreshold", required=False, help="Set threshold for movement detection (0 - turn off accelerometer)")
ap.add_argument("-p", "--txpower", required=False, help="TX Power for broadcasts (0 - low power, 1 - normal power, 2 - high power")
ap.add_argument("-k", "--keyfile", required=False, help="Binary public keys file(*_keyfile from generate_keys.py)")
ap.add_argument("-f", "--fmdnkey", required=False, help="Google FMDN key (hex bytearray)")

args = vars(ap.parse_args())

adapters = simplepyble.Adapter.get_adapters()
adapter = adapters[0]

print("Waiting for connect, place device close to the computer. If it will not detect any device in 3 minutes - restart bluetooth")
Connected = False
while not Connected:
  adapter.scan_for(50)
  peripherals = adapter.scan_get_results()
  for peripheral in peripherals:
    if peripheral.address() == args['macid'].upper():
      print(f"Found, trying to connect to {peripheral.address()}")
      try:
        peripheral.connect()
        Connected = True
      except KeyboardInterrupt:
        sys.exit(1)
      except:
        Connected = False
        print(f"Connect failed, returning to scan")
      p = peripheral
    else:
      print(f"Found other device: {peripheral.address()}")

print("Connected")
services = p.services()
service_characteristic_pair = []
c = []
#controlService = "8c5debdf-ad8d-4810-a31f-53862e79ee77"
controlService = "5cfce313-a7e3-45c3-933d-418b8100da7f"
#for service in services:
#  if service.uuid() == controlService:
#    for characteristic in service.characteristics():
#      c.append(characteristic.uuid())

# Authentication (sending auth key first)
arrauth = array.array("B", args['auth'].encode())
auth = bytes(arrauth)
# Auth
peripheral.write_request(controlService, "8c5debdf-ad8d-4810-a31f-53862e79ee77", auth)

falseint = bytes([0, 0, 0, 0])
trueint = bytes([0x01, 0, 0, 0])

# New public keys for airtag
if 'keyfile' in args and args['keyfile'] != None:
  zeroKey = bytes(gen_array(14,0))
  f = open(args['keyfile'], "rb")
  # Read first byte (number of keys)
  nKeys = f.read(1)[0]
  if nKeys > 40:
    nKeys = 40
  print("Reading %d keys" % (nKeys))
  for i in range(nKeys * 2):
    peripheral.write_request(controlService, "8c5debde-ad8d-4810-a31f-53862e79ee77", f.read(14))
  if (nKeys < 40):
    peripheral.write_request(controlService, "8c5debde-ad8d-4810-a31f-53862e79ee77", zeroKey)
    peripheral.write_request(controlService, "8c5debde-ad8d-4810-a31f-53862e79ee77", zeroKey)

# beacon
if 'fmdn' in args and args['fmdn'] != None:
  val = falseint
  if args['fmdn'] == "0":
    val = falseint
  else:
    val = trueint
  peripheral.write_request(controlService, "8c5debdb-ad8d-4810-a31f-53862e79ee77", val)

# Airtag
if 'airtag' in args and args['airtag'] != None:
  val = falseint
  if args['airtag'] == "0":
    val = falseint
  else:
    val = trueint
  peripheral.write_request(controlService, "8c5debdc-ad8d-4810-a31f-53862e79ee77", val)

# Period multiplier
if 'delay' in args and args['delay'] != None:
  val = trueint
  if args['delay'] == "4":
    val = bytes([0x04, 0, 0, 0])
  elif args['delay'] == "2":
    val = bytes([0x02, 0, 0, 0])
  elif args['delay'] == "8":
    val = bytes([0x08, 0, 0, 0])
  peripheral.write_request(controlService, "8c5debdd-ad8d-4810-a31f-53862e79ee77", val)

# TX Power
if 'txpower' in args and args['txpower'] != None:
  val = trueint
  if args['txpower'] == "2":
    val = bytes([0x02, 0, 0, 0])
  elif args['txpower'] == "0":
    val = bytes([0x00, 0, 0, 0])
  peripheral.write_request(controlService, "8c5debe1-ad8d-4810-a31f-53862e79ee77", val)

# Key change interval (600)
if 'interval' in args and args['interval'] != None:
  valNum = int(args['interval'])
  peripheral.write_request(controlService, "8c5debe0-ad8d-4810-a31f-53862e79ee77", valNum.to_bytes(4, byteorder = 'little'))

# Read and print time on the beacon
if 'readtime' in args and args['readtime'] != None:
  value = peripheral.read(controlService, "8c5debe3-ad8d-4810-a31f-53862e79ee77")
  valueTime = int.from_bytes(value, byteorder='little')
  readdableTime = datetime.utcfromtimestamp(valueTime).strftime('%Y-%m-%d %H:%M:%S')
  print(f"Time on beacon (UTC): {readdableTime} ({valueTime})")

# Write current time to the beacon
if 'writetime' in args and args['writetime'] != None:
  currentTime = time.time()
  value = int(currentTime).to_bytes(8, byteorder='little')
  peripheral.write_request(controlService, "8c5debe3-ad8d-4810-a31f-53862e79ee77", value)
  
# Write new FMDN key to the beacon
if 'fmdnkey' in args and args['fmdnkey'] != None:
  fmdnkey = bytes.fromhex(args['fmdnkey'])
  peripheral.write_request(controlService, "8c5debe2-ad8d-4810-a31f-53862e79ee77", fmdnkey)

# Write accelerometer threshold
if 'movethreshold' in args and args['movethreshold'] != None:
  valNum = int(args['movethreshold'])
  peripheral.write_request(controlService, "8c5debe6-ad8d-4810-a31f-53862e79ee77", valNum.to_bytes(4, byteorder = 'little'))
  
# Write status byte behaviour
if 'statusbyte' in args and args['statusbyte'] != None:
  valNum = int(args['statusbyte'], 16)
  peripheral.write_request(controlService, "8c5debe5-ad8d-4810-a31f-53862e79ee77", valNum.to_bytes(4, byteorder = 'little'))

# New MAC address (in settings mode) 
if 'newmacid' in args and args['newmacid'] != None:
  newmac = binascii.unhexlify(args['newmacid'].replace(':',''))
  # Reverse bytes order
  newmac = newmac[::-1]
  peripheral.write_request(controlService, "8c5debe4-ad8d-4810-a31f-53862e79ee77", newmac)

# New authentication code
if 'newauth' in args and args['newauth'] != None:
  arrauth = array.array("B", args['newauth'].encode())
  auth = bytes(arrauth)
  peripheral.write_request(controlService, "8c5debdf-ad8d-4810-a31f-53862e79ee77", auth)

peripheral.disconnect()
