## Bluetooth beacon design with Qi/WPC charging

The goal of the project was to make thin and small BLE beacon that uses Li-Ion battery and can be recharged with wireless charging. Note, it may not work with all Qi/WPC chargers because small receiving charging coil, it is highly recommended to adapt charger by replacing its transmitting coil with small 20mm diameter coil. I've used Wurth 760308101104 coil with Adafruit 2162 Qi/WPC charger module.

Final size with LIR2016 battery - 50x20x2mm in thin variant. If height is not limited, then battery can be placed above the board and size will be 30x20mm. A ferrite gasket 20x15mm plate is strongly recommended to be glued on the receiving coil, i've used Wurth 74274119 in that role. 5x1mm round magnet can be placed in the hole on the board and transmitting antenna, so both coils will self-align while charge.

When placed on a Qi/WPC charger - reset pulse will be generated for the MCU, keep that in mind if you want to use with your own firmwares. BQ25121A is used in I2C-only configuration to simplify PCB design, you have to configure charging current, voltages and such during MCU initialization.

BQ25121A and 0402 sized components are quite hard to solder manually, so you may want to order them assembled! Link on PCBWay: https://www.pcbway.com/project/shareproject/Small_and_thin_50x20x2mm_Bluetooth_beacon_with_Qi_WPC_wireless_charging_f97170e6.html

