#ifndef __TLV_PROTOCOL_H
#define __TLV_PROTOCOL_H

#include "char.h"

struct tlv_protocol {
	const char *name;
	int def;
	void *priv;

	void *(*init)(struct tlv_device *tlvd, int force);
	void (*free)(void *sp);
	void (*list)(void);
	int (*check)(char *key, char *val);
	int (*print)(void *sp, char *key, char *out);
	int (*store)(void *sp, char *key, char *in);
};

int tlvp_register(struct tlv_protocol *tlvp);
void tlvp_unregister(void);
struct tlv_protocol *tlvp_init(struct tlv_device *tlvd, int force);
void tlvp_free(struct tlv_protocol *tlvp);
void tlvp_eeprom_list(struct tlv_protocol *tlvp);
int tlvp_eeprom_check(struct tlv_protocol *tlvp, char *key, char *val);
int tlvp_eeprom_import(struct tlv_protocol *tlvp, char *key, char *in);
int tlvp_eeprom_export(struct tlv_protocol *tlvp, char *key, char *out);

#endif /* __TLV_PROTOCOL_H */
