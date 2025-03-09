# This SoC has duplicate unit addresses for some peripherals, such as
# Clock and power and bprot.
list(APPEND EXTRA_DTC_FLAGS "-Wno-unique_unit_address_if_enabled")
