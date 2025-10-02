#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include <stdlib.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/bluetooth/controller.h>

#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/nvs.h>
#include "settings.h"
#include "myboards.h"
#include "public_keys.h"

#define MAX_KEYS_IN_MEMORY 40

uint8_t authCode[8] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};

int flagFmdn = 0;
int flagAirtag = 0;
int multPeriod = 2;
int txPower = 2;
int changeInterval = 6000;
int needsReset = 0;
int pauseUpload = 0;
int statusFlags = 0x458000;
int64_t timeOffset = 0;
int accelThreshold = 800;

// Airtag keys
int numKeys = ARRAY_SIZE(default_airtag_key);
unsigned char keys_storage[MAX_KEYS_IN_MEMORY][28];
unsigned char (*currentKeys)[28] = default_airtag_key;
// Google FMDN key
unsigned char fmdnKey[20];

// MAC address in setting mode (if first and last bytes are zero - then use from UICR)
uint8_t settingsMAC[6] = {0, 0x41, 0x42, 0x43, 0x44, 0};

static struct nvs_fs fs;

#define NVS_PARTITION		storage_partition
#define NVS_PARTITION_DEVICE	FIXED_PARTITION_DEVICE(NVS_PARTITION)
#define NVS_PARTITION_OFFSET	FIXED_PARTITION_OFFSET(NVS_PARTITION)

// Service and Characteristics UUIDs
static const struct bt_uuid_128 fmdn_uuid = BT_UUID_INIT_128(
  // 8c5debdb-ad8d-4810-a31f-53862e79ee77
  BT_UUID_128_ENCODE(0x8c5debdb, 0xad8d, 0x4810, 0xa31f, 0x53862e79ee77)
);

static const struct bt_uuid_128 switchAirtag_uuid = BT_UUID_INIT_128(
  // 8c5debdc-ad8d-4810-a31f-53862e79ee77
  BT_UUID_128_ENCODE(0x8c5debdc, 0xad8d, 0x4810, 0xa31f, 0x53862e79ee77)
);

static const struct bt_uuid_128 period_uuid = BT_UUID_INIT_128(
  // 8c5debdd-ad8d-4810-a31f-53862e79ee77
  BT_UUID_128_ENCODE(0x8c5debdd, 0xad8d, 0x4810, 0xa31f, 0x53862e79ee77)
);

static const struct bt_uuid_128 key_uuid = BT_UUID_INIT_128(
  // 8c5debde-ad8d-4810-a31f-53862e79ee77
  BT_UUID_128_ENCODE(0x8c5debde, 0xad8d, 0x4810, 0xa31f, 0x53862e79ee77)
);

static const struct bt_uuid_128 auth_uuid = BT_UUID_INIT_128(
  // 8c5debdf-ad8d-4810-a31f-53862e79ee77
  BT_UUID_128_ENCODE(0x8c5debdf, 0xad8d, 0x4810, 0xa31f, 0x53862e79ee77)
);

static const struct bt_uuid_128 changeInterval_uuid = BT_UUID_INIT_128(
  // 8c5debe0-ad8d-4810-a31f-53862e79ee77
  BT_UUID_128_ENCODE(0x8c5debe0, 0xad8d, 0x4810, 0xa31f, 0x53862e79ee77)
);

static const struct bt_uuid_128 txPower_uuid = BT_UUID_INIT_128(
  // 8c5debe1-ad8d-4810-a31f-53862e79ee77
  BT_UUID_128_ENCODE(0x8c5debe1, 0xad8d, 0x4810, 0xa31f, 0x53862e79ee77)
);

static const struct bt_uuid_128 fmdnKey_uuid = BT_UUID_INIT_128(
  // 8c5debe2-ad8d-4810-a31f-53862e79ee77
  BT_UUID_128_ENCODE(0x8c5debe2, 0xad8d, 0x4810, 0xa31f, 0x53862e79ee77)
);

static const struct bt_uuid_128 timeOffset_uuid = BT_UUID_INIT_128(
  // 8c5debe3-ad8d-4810-a31f-53862e79ee77
  BT_UUID_128_ENCODE(0x8c5debe3, 0xad8d, 0x4810, 0xa31f, 0x53862e79ee77)
);

static const struct bt_uuid_128 settingsMAC_uuid = BT_UUID_INIT_128(
  // 8c5debe4-ad8d-4810-a31f-53862e79ee77
  BT_UUID_128_ENCODE(0x8c5debe4, 0xad8d, 0x4810, 0xa31f, 0x53862e79ee77)
);

static const struct bt_uuid_128 status_uuid = BT_UUID_INIT_128(
  // 8c5debe5-ad8d-4810-a31f-53862e79ee77
  BT_UUID_128_ENCODE(0x8c5debe5, 0xad8d, 0x4810, 0xa31f, 0x53862e79ee77)
);

static const struct bt_uuid_128 accel_uuid = BT_UUID_INIT_128(
  // 8c5debe6-ad8d-4810-a31f-53862e79ee77
  BT_UUID_128_ENCODE(0x8c5debe6, 0xad8d, 0x4810, 0xa31f, 0x53862e79ee77)
);

// 5cfce313-a7e3-45c3-933d-418b8100da7f
#define BEACON_SERVICE_UUID_VAL BT_UUID_128_ENCODE(0x5cfce313, 0xa7e3, 0x45c3, 0x933d, 0x418b8100da7f)

static const struct bt_uuid_128 beacon_svc_uuid = BT_UUID_INIT_128(BEACON_SERVICE_UUID_VAL);

// Advertisement Data in settings mode
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_LIMITED | BT_LE_AD_NO_BREDR)),
   	BT_DATA_BYTES(BT_DATA_UUID16_ALL,
		      BT_UUID_16_ENCODE(BT_UUID_UDS_VAL)),
};

int connectedGatt = 0;
int authorizedGatt = 0;
int allowedChange = 0;
int keysReceived = 0;
int updateKeysAtDisconnect = 0;

#define ID_fmdn_NVS 0x01
#define ID_airtag_NVS 0x02
#define ID_period_NVS 0x03
#define ID_changeInterval_NVS 0x04
#define ID_key_NVS 0x05
#define ID_auth_NVS 0x06
#define ID_power_NVS 0x07
#define ID_fmdnKey_NVS 0x08
#define ID_timeOffset_NVS 0x09
#define ID_settingsMAC_NVS 0x0a
#define ID_status_NVS 0x0b
#define ID_accel_NVS 0x0c

// GAP callbacks
static void connected(struct bt_conn *conn, uint8_t err) {
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  allowedChange = 0;
  if (err) {
    printk("Connection failed (err 0x%02x)\n", err);
  } else {
    connectedGatt = 1;
    printk("Connected %s\n", addr);
  }
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
  connectedGatt = 0;
  printk("Disconnected %s (reason 0x%02x)\n", addr, reason);
  // Write keys to NVS
  if (updateKeysAtDisconnect) {
    printk("Writing public keys to NVS memory\n");
    // Mark end of keys array with zero'ed key
    if (numKeys < sizeof(keys_storage))
      memset(keys_storage[numKeys], 0, 28);
    (void) nvs_write(&fs, ID_key_NVS, keys_storage, sizeof(keys_storage));
    updateKeysAtDisconnect = 0;
  }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

// Check if public key is empty (zeros)
int check_empty_key(unsigned char *key) {
  for (int j = 0; j < 28; j++)
    if (key[j] != 0)
      return 0;
  return 1;
}

// GATT Access Callbacks
static ssize_t chrc_write_auth(struct bt_conn *conn,
                              const struct bt_gatt_attr *attr, const void *buf,
                              uint16_t len, uint16_t offset, uint8_t flags) {
  printk("chrc_write_auth %04x %02x\n", len, ((uint8_t *)buf)[0]);
  // Update for auth code (after successful auth)
  if (allowedChange && (len == sizeof(authCode))) {
    printk("Updating auth code\n");
    memcpy(authCode, buf, sizeof(authCode));
    (void) nvs_write(&fs, ID_auth_NVS, authCode, sizeof(authCode));
    return len;
  }
  if ((len == sizeof(authCode)) && !memcmp(authCode, (uint8_t *)buf, sizeof(authCode))) {
    printk("Authentication code is right, allowing changes\n");
    pauseUpload = 1;
    allowedChange = 1;
    updateKeysAtDisconnect = 0;
    authorizedGatt = 1;
    keysReceived = 0;
  } else {
    printk("Authentication code is invalid, not allowing changes\n");
    allowedChange = 0;
    needsReset = 0;
    pauseUpload = 0;
  };
  return len;
}

static ssize_t chrc_write_key(struct bt_conn *conn,
                              const struct bt_gatt_attr *attr, const void *buf,
                              uint16_t len, uint16_t offset, uint8_t flags) {
  printk("chrc_write_key %04x %02x\n", len, ((uint8_t *)buf)[0]);

  if (!allowedChange || (len != 14) || (keysReceived >= (2 * ARRAY_SIZE(keys_storage))))
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  
  memcpy(keys_storage[keysReceived / 2] + 14 * (keysReceived % 2), buf, 14);
  keysReceived++;
  if (keysReceived % 2 == 0) {
    numKeys = keysReceived / 2;
    currentKeys = keys_storage;
    needsReset = 1;
    updateKeysAtDisconnect = 1;
    pauseUpload = 0;
  } 
  return len;
}

static ssize_t chrc_write_period(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
  printk("chrc_write_period %04x %02x\n", len, ((uint8_t *)buf)[0]);
  if (len != sizeof(int32_t))
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  int32_t var = *((uint32_t *)buf);
  if (allowedChange && ((var == 1) || (var == 2) || (var == 4) || (var == 8))) {
    multPeriod = var;
    (void) nvs_write(&fs, ID_period_NVS, &multPeriod, sizeof(multPeriod));
    needsReset = 1;
    pauseUpload = 0;
  }
  return len; 
}

static ssize_t chrc_write_fmdn(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
  printk("chrc_write_fmdn %04x %02x\n", len, ((uint8_t *)buf)[0]);
  if (len != sizeof(int32_t))
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  int32_t var = *((uint32_t *)buf);
  if (allowedChange) {
    flagFmdn = var;
    (void) nvs_write(&fs, ID_fmdn_NVS, &flagFmdn, sizeof(flagFmdn));
    needsReset = 1;
    pauseUpload = 0;
  }
  return len;
}

static ssize_t chrc_write_airtag(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
  printk("chrc_write_airtag %04x %02x\n", len, ((uint8_t *)buf)[0]);
  if (len != sizeof(int32_t))
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  int32_t var = *((uint32_t *)buf);
  if (allowedChange) {
    flagAirtag = var;
    (void) nvs_write(&fs, ID_airtag_NVS, &flagAirtag, sizeof(flagAirtag));
    needsReset = 1;
    pauseUpload = 0;
  }
  return len;
}

static ssize_t chrc_write_changeInterval(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
  printk("chrc_write_changeInterval %04x %d\n", len, ((int32_t *)buf)[0]);
  if (len != sizeof(int32_t))
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  int32_t var = *((uint32_t *)buf);
  if (allowedChange && (var > 29) && (var < 7201)) {
    changeInterval = var;
    (void) nvs_write(&fs, ID_changeInterval_NVS, &changeInterval, sizeof(changeInterval));
    needsReset = 1;
    pauseUpload = 0;
  }
  return len;
}

static ssize_t chrc_write_status(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
  printk("chrc_write_status %04x %04x\n", len, ((int32_t *)buf)[0]);
  if (len != sizeof(int32_t))
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  if (allowedChange) {
    statusFlags = *((int32_t *)buf);
    (void) nvs_write(&fs, ID_status_NVS, &statusFlags, sizeof(statusFlags));
    pauseUpload = 0;
  }
  return len;
}

static ssize_t chrc_write_accel(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr, const void *buf,
                                uint16_t len, uint16_t offset, uint8_t flags) {
  printk("chrc_write_accel %04x %04x\n", len, ((int32_t *)buf)[0]);
  int32_t var = *((uint32_t *)buf);
  if (len != sizeof(int32_t))
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  if (allowedChange && (var >= 0) && (var < 16384)) {
    accelThreshold = *((int32_t *)buf);
    (void) nvs_write(&fs, ID_accel_NVS, &accelThreshold, sizeof(accelThreshold));
    needsReset = 1;
    pauseUpload = 0;
  }
  return len;
}

static ssize_t chrc_write_txPower(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
  printk("chrc_write_txPower %04x %02x\n", len, ((uint8_t *)buf)[0]);
  if (len != sizeof(int32_t))
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  int32_t var = *((uint32_t *)buf);
  if (allowedChange && (var >= 0) && (var <= 2)) {
    txPower = var;
    (void) nvs_write(&fs, ID_power_NVS, &txPower, sizeof(txPower));
    pauseUpload = 0;
  }
  return len;
}

int64_t timeValue;

// Read current time on the beacon
static ssize_t chrc_read_timeOffset(struct bt_conn *conn,
                              const struct bt_gatt_attr *attr, void *buf,
                              uint16_t len, uint16_t offset) {
  printk("chrc_read_timeOffset %04x\n", len);
  if (len < sizeof(int64_t))
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  if (allowedChange) {
    const int64_t *value = attr->user_data;
    timeValue = get_time();
    pauseUpload = 0;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(*value));
  }
  return 0;
}

// Write current time to the beacon (recalculate timeOffset)
static ssize_t chrc_write_timeOffset(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
  printk("chrc_write_timeOffset %04x %04x\n", len, *((uint32_t *)buf));
  if (len != sizeof(int64_t))
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  int64_t newTime = *((int64_t *) buf);
  if (allowedChange) {
    timeOffset = newTime - k_uptime_seconds();
    (void) nvs_write(&fs, ID_timeOffset_NVS, &newTime, sizeof(newTime));
    pauseUpload = 0;
  }
  return len;
}

// Change MAC address in settings mode
static ssize_t chrc_write_settingsMAC(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
  printk("chrc_write_settingsMAC %04x %02x:%02x:%02x:%02x:%02x:%02x\n", len,
          ((uint8_t *)buf)[5], ((uint8_t *)buf)[4], ((uint8_t *)buf)[3],
          ((uint8_t *)buf)[2], ((uint8_t *)buf)[1], ((uint8_t *)buf)[0]);
  if (len != sizeof(settingsMAC))
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  if (allowedChange) {
    memcpy(settingsMAC, buf, sizeof(settingsMAC));
    (void) nvs_write(&fs, ID_settingsMAC_NVS, &settingsMAC, sizeof(settingsMAC));
    pauseUpload = 0;
  }
  return len;
}

// Write new key for Google FMDN broadcast
static ssize_t chrc_write_fmdnKey(struct bt_conn *conn,
                               const struct bt_gatt_attr *attr, const void *buf,
                               uint16_t len, uint16_t offset, uint8_t flags) {
  printk("chrc_write_fmdnKey %04x\n", len);
  if (len != sizeof(fmdnKey))
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  if (allowedChange) {
    memcpy(fmdnKey, buf, sizeof(fmdnKey));
    (void) nvs_write(&fs, ID_fmdnKey_NVS, &fmdnKey, sizeof(fmdnKey));
    needsReset = 1;
    pauseUpload = 0;
  }
  return len;
}

// GATT Attributes Table
BT_GATT_SERVICE_DEFINE(
  beacon_svc, BT_GATT_PRIMARY_SERVICE(&beacon_svc_uuid),
  BT_GATT_CHARACTERISTIC(&auth_uuid.uuid,
    BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_WRITE,
    NULL, chrc_write_auth, NULL),
  BT_GATT_CHARACTERISTIC(&key_uuid.uuid,
    BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_WRITE,
    NULL, chrc_write_key, NULL),
  BT_GATT_CHARACTERISTIC(&period_uuid.uuid,
    BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_WRITE,
    NULL, chrc_write_period, NULL),
  BT_GATT_CHARACTERISTIC(&fmdn_uuid.uuid,
    BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_WRITE,
    NULL, chrc_write_fmdn, NULL),
  BT_GATT_CHARACTERISTIC(&switchAirtag_uuid.uuid,
    BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_WRITE,
    NULL, chrc_write_airtag, NULL),
  BT_GATT_CHARACTERISTIC(&changeInterval_uuid.uuid,
    BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_WRITE,
    NULL, chrc_write_changeInterval, NULL),
  BT_GATT_CHARACTERISTIC(&txPower_uuid.uuid,
    BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_WRITE,
    NULL, chrc_write_txPower, NULL),
  BT_GATT_CHARACTERISTIC(&fmdnKey_uuid.uuid,
    BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_WRITE,
    NULL, chrc_write_fmdnKey, NULL),
  BT_GATT_CHARACTERISTIC(&timeOffset_uuid.uuid,
    BT_GATT_CHRC_WRITE | BT_GATT_CHRC_READ,
    BT_GATT_PERM_WRITE | BT_GATT_PERM_READ,
    chrc_read_timeOffset, chrc_write_timeOffset, &timeValue),
  BT_GATT_CHARACTERISTIC(&settingsMAC_uuid.uuid,
    BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_WRITE,
    NULL, chrc_write_settingsMAC, NULL),
  BT_GATT_CHARACTERISTIC(&status_uuid.uuid,
    BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_WRITE,
    NULL, chrc_write_status, NULL),
  BT_GATT_CHARACTERISTIC(&accel_uuid.uuid,
    BT_GATT_CHRC_WRITE,
    BT_GATT_PERM_WRITE,
    NULL, chrc_write_accel, NULL),
);

// Read variable from NVS
int my_nvs_read(int id, void *var, int len) {
  int rc;

  rc = nvs_read(&fs, id, var, len);
	if (rc > 0) {
		printk("Id: %d, val %08x\n", id, *((int *)var));
    return 0;
  } else {
    printk("Id: %d not found\n", id);
    return -1;
  }
}

// Initialize NVS and read settings
int init_settings() {
  struct flash_pages_info info;
  int rc;

	/* define the nvs file system by settings with:
	 *	sector_size equal to the pagesize,
	 *	3 sectors
	 *	starting at NVS_PARTITION_OFFSET
	 */
	fs.flash_device = NVS_PARTITION_DEVICE;
	if (!device_is_ready(fs.flash_device)) {
		printk("Flash device %s is not ready\n", fs.flash_device->name);
		return -1;
	}
	fs.offset = NVS_PARTITION_OFFSET;
	rc = flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
	if (rc) {
		printk("Unable to get page info\n");
		return -1;
	}
  printk("init_settings, NVS size %d, NVS offset %lx\n", info.size, info.start_offset);
	fs.sector_size = info.size;
	fs.sector_count = 3U;

	rc = nvs_mount(&fs);
	if (rc) {
		printk("Flash Init failed\n");
		return -1;
	}

  printk("NVS initalized, reading settings\n");
  my_nvs_read(ID_timeOffset_NVS, &timeOffset, sizeof(timeOffset));
  my_nvs_read(ID_settingsMAC_NVS, &settingsMAC, sizeof(settingsMAC));
  my_nvs_read(ID_fmdn_NVS, &flagFmdn, sizeof(flagFmdn));
  my_nvs_read(ID_airtag_NVS, &flagAirtag, sizeof(flagAirtag));
  my_nvs_read(ID_period_NVS, &multPeriod, sizeof(multPeriod));
  my_nvs_read(ID_power_NVS, &txPower, sizeof(txPower));
  my_nvs_read(ID_status_NVS, &statusFlags, sizeof(statusFlags));
  my_nvs_read(ID_accel_NVS, &accelThreshold, sizeof(accelThreshold));
  my_nvs_read(ID_changeInterval_NVS, &changeInterval, sizeof(changeInterval));
  changeInterval = changeInterval - (changeInterval % 8);
  my_nvs_read(ID_auth_NVS, authCode, sizeof(authCode));

  // Load keys from NVS storage and check for number of keys in it
  if (!my_nvs_read(ID_key_NVS, keys_storage, sizeof(keys_storage))) {
    int i;

    for (i = 0; i < MAX_KEYS_IN_MEMORY; i++)
      if (check_empty_key(keys_storage[i]))
        break;
    printk("Loaded %d keys from NVS storage\n", i);
    numKeys = i;
    currentKeys = keys_storage;
  } else 
    numKeys = 0;

  if (numKeys == 0) {
    int i;
    int sum = 0;

    printk("Zero keys in NVS record, checking default keys from public_keys.h\n");
    numKeys = ARRAY_SIZE(default_airtag_key);
    currentKeys = default_airtag_key;

    for (i = 0; i < sizeof(default_airtag_key); i++)
      sum += default_airtag_key[0][i];

    // If not all bytes in public_keys.h are zero then use the key
    if (sum != 0) {
      printk("Airtag key(s) found in public_keys.h, using it\n");
      flagAirtag = 1;
    }
  }

  // Load Google FMDN key or use default key (from public.h)
  if (my_nvs_read(ID_fmdnKey_NVS, fmdnKey, sizeof(fmdnKey))) {
    printk("No Google FMDN key in NVS, switching to default key\n");
    memcpy(fmdnKey, default_fmdn_key, sizeof(fmdnKey));
  }

  // Check if FMDN key in public_keys.h is not zero then use it
  if (!flagFmdn) {
    int i;
    int sum = 0;

    for (i = 0; i < sizeof(fmdnKey); i++)
      sum += default_fmdn_key[i];

    if (sum != 0) {
      printk("Google FMDN key found in public_keys.h, using it\n");
      flagFmdn = 1;
    }
  }

  return 0;
}

// Authorization callbacks
bool write_authorize (struct bt_conn *conn, const struct bt_gatt_attr *attr) {
  if (authorizedGatt) {
    printk("write_authorize: already authorized\n");
    return true;
  }
  if (!bt_uuid_cmp(attr->uuid, &auth_uuid)) {
    printk("write_authorize: auth_uuid authorized!\n");
    return true;
  }
  printk("write_authorize: no authorization!\n");
  return false;
}

bool read_authorize (struct bt_conn *conn, const struct bt_gatt_attr *attr) {
  return true;
}

static const struct bt_gatt_authorization_cb auth_callbacks = {
  .read_authorize = read_authorize,
  .write_authorize = write_authorize,
};


// Start advertisement for settings
void start_settings_adv() {
  int err;

  connectedGatt = 0;
  authorizedGatt = 0;
  pauseUpload = 0;
  needsReset = 0;
  err = bt_gatt_authorization_cb_register(&auth_callbacks);
  if (err) {
    printk("Error at setting auth callbacks (err %d), probably already set\n", err);
  }
  err = bt_le_adv_start(BT_LE_ADV_PARAM(BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_CONNECTABLE,
                        SETTINGS_ADV_INTERVAL * 1.6 - 20, SETTINGS_ADV_INTERVAL * 1.6 + 20, NULL),
                        ad, ARRAY_SIZE(ad), NULL, 0);
  if (err) {
    printk("Settings Advertising failed to start (err %d)\n", err);
    return;
  }
  printk("Settings Advertising successfully started\n");
}

// Disconnect callback function
void disconnect(struct bt_conn *conn, void *data) {
  printk("Disconnecting\n");
  (void) bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

// Stop advertisements for settings, close all connections
void stop_settings_adv() {
  bt_le_adv_stop();
  bt_conn_foreach(BT_CONN_TYPE_ALL, disconnect, NULL);
}

// Update current time in NVS storage
void store_time() {
  int64_t currentTime = get_time();

  printk("Storing time in NVS: %lld\n", currentTime);
  (void) nvs_write(&fs, ID_timeOffset_NVS, &currentTime, sizeof(currentTime));
}

// Get current time
int64_t get_time() {
  return timeOffset + (int64_t) k_uptime_seconds();
}
