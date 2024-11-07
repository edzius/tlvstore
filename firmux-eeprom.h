#ifndef __FIRMUX_EEPROM_H
#define __FIRMUX_EEPROM_H

enum tlv_code {
	EEPROM_ATTR_NONE,

	/* Product related attributes */
	EEPROM_ATTR_PRODUCT_ID,                 /* char*, ASCII */
	EEPROM_ATTR_PRODUCT_NAME,               /* char*, ASCII */
	EEPROM_ATTR_SERIAL_NO,                  /* char*, ASCII */

	/* PCB related attributes */
	EEPROM_ATTR_PCB_NAME = 16,              /* char*, "FooBar" */
	EEPROM_ATTR_PCB_REVISION,               /* char*, "0002" */
	EEPROM_ATTR_PCB_PRDATE,                 /* time_t, seconds */
	EEPROM_ATTR_PCB_PRLOCATION,             /* char*, "Kaunas" */
	EEPROM_ATTR_PCB_SN,                     /* char*, "" */

	/* MAC infoformation */
	EEPROM_ATTR_MAC = 128,
	EEPROM_ATTR_MAC_FIRST = EEPROM_ATTR_MAC,
	EEPROM_ATTR_MAC_1 = EEPROM_ATTR_MAC,
	EEPROM_ATTR_MAC_2,
	EEPROM_ATTR_MAC_3,
	EEPROM_ATTR_MAC_4,
	EEPROM_ATTR_MAC_5,
	EEPROM_ATTR_MAC_6,
	EEPROM_ATTR_MAC_7,
	EEPROM_ATTR_MAC_8,
	EEPROM_ATTR_MAC_9,
	EEPROM_ATTR_MAC_10,
	EEPROM_ATTR_MAC_11,
	EEPROM_ATTR_MAC_12,
	EEPROM_ATTR_MAC_13,
	EEPROM_ATTR_MAC_14,
	EEPROM_ATTR_MAC_15,
	EEPROM_ATTR_MAC_16,
	EEPROM_ATTR_MAC_LAST = EEPROM_ATTR_MAC_16,

	EEPROM_ATTR_EMPTY = 0xFF,
};

struct tlv_property {
	enum tlv_code tlvp_id;
	const char *tlvp_name;
	ssize_t (*tlvp_parse)(void **data_out, void *data_in, size_t size_in);
	ssize_t (*tlvp_format)(void **data_out, void *data_in, size_t size_in);
};

int tlv_eeprom_prop_check(char *key, char *val);
int tlv_eeprom_update(struct tlv_store *tlvs, char *key, char *val);
int tlv_eeprom_dump(struct tlv_store *tlvs, char *key);
int tlv_eeprom_export(struct tlv_store *tlvs, char *key, char *fname);
int tlv_eeprom_import(struct tlv_store *tlvs, char *key, char *fname);
void tlv_eeprom_list(void);

#endif /* __FIRMUX_EEPROM_H */
