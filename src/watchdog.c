#include <stdlib.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/reboot.h>

#include "watchdog.h"


#define WDT_FEED_TRIES 5

/*
 * To use this sample the devicetree's /aliases must have a 'watchdog0' property.
 */
#if DT_HAS_COMPAT_STATUS_OKAY(st_stm32_window_watchdog)
#define WDT_MAX_WINDOW  100U
#elif DT_HAS_COMPAT_STATUS_OKAY(nordic_nrf_wdt)
/* Nordic supports a callback, but it has 61.2 us to complete before
 * the reset occurs, which is too short for this sample to do anything
 * useful.  Explicitly disallow use of the callback.
 */
#define WDT_ALLOW_CALLBACK 0
#elif DT_HAS_COMPAT_STATUS_OKAY(raspberrypi_pico_watchdog)
#define WDT_ALLOW_CALLBACK 0
#elif DT_HAS_COMPAT_STATUS_OKAY(gd_gd32_wwdgt)
#define WDT_MAX_WINDOW 24U
#define WDT_MIN_WINDOW 18U
#define WDG_FEED_INTERVAL 12U
#elif DT_HAS_COMPAT_STATUS_OKAY(intel_tco_wdt)
#define WDT_ALLOW_CALLBACK 0
#define WDT_MAX_WINDOW 3000U
#elif DT_HAS_COMPAT_STATUS_OKAY(nxp_fs26_wdog)
#define WDT_MAX_WINDOW  1024U
#define WDT_MIN_WINDOW  320U
#define WDT_OPT 0
#define WDG_FEED_INTERVAL (WDT_MIN_WINDOW + ((WDT_MAX_WINDOW - WDT_MIN_WINDOW) / 4))
#endif

#ifndef WDT_ALLOW_CALLBACK
#define WDT_ALLOW_CALLBACK 1
#endif

#ifndef WDT_MAX_WINDOW
#define WDT_MAX_WINDOW  10000U
#endif

#ifndef WDT_MIN_WINDOW
#define WDT_MIN_WINDOW  0U
#endif

#ifndef WDG_FEED_INTERVAL
#define WDG_FEED_INTERVAL 50U
#endif

#ifndef WDT_OPT
#define WDT_OPT WDT_OPT_PAUSE_HALTED_BY_DBG
#endif

#if DT_NODE_EXISTS(DT_ALIAS(watchdog0))
#define WDT_NAME DT_ALIAS(watchdog0)
#else
#define WDT_NAME DT_NODELABEL(wdt)
#endif

const struct device *const wdt = DEVICE_DT_GET(WDT_NAME);

int wdt_channel_id;

void my_wdt_init()
{
	int err;
	
	printk("Initializing watchdog\n");

	if (!device_is_ready(wdt))
	{
		printk("%s: device not ready.\n", wdt->name);
		return;
	}

	struct wdt_timeout_cfg wdt_config = {
		/* Reset SoC when watchdog timer expires. */
		.flags = WDT_FLAG_RESET_SOC,

		/* Expire watchdog after max window */
		.window.min = WDT_MIN_WINDOW,
		.window.max = WDT_MAX_WINDOW,
	};

	wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	if (wdt_channel_id == -ENOTSUP)
	{
		/* IWDG driver for STM32 doesn't support callback */
		printk("Callback support rejected, continuing anyway\n");
		wdt_config.callback = NULL;
		wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
	}
	if (wdt_channel_id < 0)
	{
		printk("Watchdog install error\n");
		return;
	}

	err = wdt_setup(wdt, WDT_OPT);
	if (err < 0)
	{
		printk("Watchdog setup error\n");
		return;
	}
}

void my_wdt_disable() {
	wdt_disable(wdt);
}

void my_wdt_feed() {
	wdt_feed(wdt, wdt_channel_id);
}

