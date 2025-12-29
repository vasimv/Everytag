# Everytag
The firmware emulates Apple Airtag (up to 40 public keys rotating at default 10 minutes interval) and Google Find My Device (just one non-rotating key at the moment). It works on nRF52/54 chips and uses Zephyr. I've tried to optimize power consumption as much as could (microamperes range). MCU's watchdog is used to be sure it always works until battery dies.

One of biggest advantages of the firmware is ability to reconfigure all of its settings via BLE (python script is included), so you can change keys, transmitting power, broadcasting interval and such without reflashing. To minimize power consumption, the firmware accepts connections to change settings only for 2 seconds every minute and has simple password protection.

As beta test feature, it counts current time (if your board has 32.768kHz crystal) and saves current time to the flash periodically, so clock won't drift too much even if it gets reboot.

Also, it is possible to do OTA firmware update on nRF52/54 chips with at least 512KB flash/RRAM memory through mcumgr protocol. That includes nRF52832 (512KB version), nRF52833, nRF54L15. OTA won't work on nRF52805, nRF52810 but you can still change settings through BLE.

## Credits and useful links
Big thanks to OpenHaystack project for their great work: https://github.com/seemoo-lab/openhaystack

The firmware originated as a fork of macless-haystack project and has some of its code: https://github.com/dchristl/macless-haystack

To query locations you can use FindMy scripts: https://github.com/biemster/FindMy

To create Google FMDN key and query locations, use this: https://github.com/leonboe1/GoogleFindMyTools

## Hardware

The firmware has support (see boards/ subdirectory) and tested on these boards and beacons: NRF52DK (nRF52832), NRF54L15DK (nRF54L15), KKM C2 (nRF52805), KKM K4P (K5) with accelerometer (nRF52833), KKM P1 (nRF52810), KKM P11 (nRF52810), Fanstel NRF52805EVM (nRF52805), Minew HCB22E (nRF52832). It has no support for buttons and second/other LEDs, it'll start right after flashing without ability to shutdown (except my own board with BQ25121A, see below) only one LED will blink on the start.

Additionally, i've included my design of beacon that has Qi/WPC wireless charging support (nRF52833, BQ51013B, BQ25121A), see hardware/ subdirectory. The goal was to make small and very thin beacon (50x20x2mm with LIR2016 li-ion battery). You can order these assembled on PCBWay https://www.pcbway.com/project/shareproject/Small_and_thin_50x20x2mm_Bluetooth_beacon_with_Qi_WPC_wireless_charging_f97170e6.html (around 35 USD per board).

## Compilation
Zephyr-based nRF Connect SDK 2.8.0 is required to compile the firmware.

myboards.h file has compilation-time settings (like how often switch to settings mode, etc). 

In the Visual Studio Code - click on nrfconnect plugin button, create build configuration for your board. Use following configuration files:

prj-smpsrv.conf - debug (through J-Link RTT interface) version with mcumgr support (for 512KB or bigger flash chips). You can use JlinkRTTViewer to connect and see debug messages on its console.

prj-smpsrv-lowpower.conf - no debug version for lowest power consumption and memory usage, with mcumgr support

prj.conf - debug version without mcumgr support (no mcuboot) for small memory chips - nRF52805, nRF52810, nRF52811

prj-lowpower.conf - no debug version for these chips

For nRF52DK/nrf52832 and nRF54L15DK - use nrf52832dk.overlay / nrf54l15dk.overlay files as "Extra Devicetree overlays"

## Running
It is recommended to erase all flash memory before first run to reset NVS storage where the firmware stores its settings.

By default, the firmware will start broadcasting dummy iBeacon packets with 7 seconds interval at very low power (-8dBm) with TX power field set as battery voltage (like 31 is 3.1 volts). This broadcast will stop as soon as you load Apple Airtag and/or Google FMDN key.

The beacon will use default MAC address (from MCU's registers programmed at factory) for iBeacon and settings broadcasts. You can figure out the address via debug console or using an BLE scanning app (nrfconnect from Nordic is quite good and free) on a smartphone.

## Changing settings via BLE
Every minute it will switch into settings mode for 2 seconds (configurable) and settings (including keys) can be changed by conn_beacon.py script (requires python 3). Note, bluez isn't really perfect stack to work with BLE and the script may have troubles while connecting to the beacon. To fix that, you may try to disable/enable bluetooth or even reboot your computer.

The script is used for:
1. Changing its settings and/or writing current time to the beacon
2. Reading current time from the beacon
3. Switching the beacon into mcumgr OTA mode for one minute

All of these requires password authentication (default password is "abcdefgh", can be changed via settings mode too)

Note, after loading an Airtag key, MAC address in settings mode will be changed, it is recommended to set a specific MAC address for the settings mode when loading an Airtag key. Otherwise, you will have to figure out current MAC address via nrfconnect app or in a similar way.

Examples:
`python3 conn_beacon.py -i e7:93:3a:cc:f6:61 -c e7:93:3a:cc:f6:61 -a abcdefgh -n h45edc78 -f f1e101731f1e1b312272f812e521293a3f484414 -k ~/airtags/016WX1_keyfile -p 2 -g 1 -t 1 -w 1 -d 2`

This will connect to beacon with E7:93:3A:CC:F6:61 MAC address, change password to "h45edc78" load airtag key from 016WX1_keyfile, load FMDN key "f1e101731f1e1b312272f812e521293a3f484414", set maximum TX power (+4 or +8 dBm), allow broadcasting both Airtag and FMDN, switching every 2 seconds. You can omit some settings you don't want to change, only MAC address (-i) and authorization code (-a) is required.

`python3 conn_beacon.py -i e7:93:3a:cc:f6:61 -a h45edc78 -w 1`

This will connect to the beacon and synchronize its clock to current time.

`python3 conn_beacon.py -i e7:93:3a:cc:f6:61 -a h45edc78 -r 1`

This will connect to the beacon and read current time

To use OTA update feature, see flash_beacon.sh and flash_list.sh.
