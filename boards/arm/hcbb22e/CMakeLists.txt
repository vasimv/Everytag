# SPDX-License-Identifier: Apache-2.0

# The nrf52dk_nrf52805 board mirrors the nRF52 DK hardware. This needs
# to be considered by certain system initialization functionality residing
# in system_nrf52805.c and SoC dependent routines in nrfx_coredep.h.
zephyr_compile_definitions(DEVELOP_IN_NRF52832)
zephyr_compile_definitions(NRFX_COREDEP_DELAY_US_LOOP_CYCLES=3)

zephyr_library()


