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
    int (*update)(void *sp, char *key, char *val);
    int (*dump)(void *sp, char *key);
    int (*save)(void *sp, char *key, char *fname);
    int (*load)(void *sp, char *key, char *fname);
};

int tlvp_register(struct tlv_protocol *tlvp);
void tlvp_unregister(void);
struct tlv_protocol *tlvp_init(struct tlv_device *tlvd, int force);
void tlvp_free(struct tlv_protocol *tlvp);
void tlvp_eeprom_list(struct tlv_protocol *tlvp);
int tlvp_eeprom_check(struct tlv_protocol *tlvp, char *key, char *val);
int tlvp_eeprom_update(struct tlv_protocol *tlvp, char *key, char *val);
int tlvp_eeprom_dump(struct tlv_protocol *tlvp, char *key);
int tlvp_eeprom_save(struct tlv_protocol *tlvp, char *key, char *fname);
int tlvp_eeprom_load(struct tlv_protocol *tlvp, char *key, char *fname);

#endif /* __TLV_PROTOCOL_H */
