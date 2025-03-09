#ifndef __TLV_PROTOCOL_H
#define __TLV_PROTOCOL_H

#include "char.h"

struct tlv_protocol {
    const char *name;
    void *priv;

    void *(*init)(struct tlv_device *tlvd);
    void (*free)(void *spriv);
    void (*list)(void);
    int (*check)(char *key, char *val);
    int (*update)(void *spriv, char *key, char *val);
    int (*dump)(void *spriv, char *key);
    int (*save)(void *spriv, char *key, char *fname);
    int (*load)(void *spriv, char *key, char *fname);
};

int tlvp_register(struct tlv_protocol *tlvp);
struct tlv_protocol *tlvp_init(struct tlv_device *tlvd);
void tlvp_free(struct tlv_protocol *tlvp);
void tlvp_eeprom_list(struct tlv_protocol *tlvp);
int tlvp_eeprom_check(struct tlv_protocol *tlvp, char *key, char *val);
int tlvp_eeprom_update(struct tlv_protocol *tlvp, char *key, char *val);
int tlvp_eeprom_dump(struct tlv_protocol *tlvp, char *key);
int tlvp_eeprom_save(struct tlv_protocol *tlvp, char *key, char *fname);
int tlvp_eeprom_load(struct tlv_protocol *tlvp, char *key, char *fname);

#endif /* __TLV_PROTOCOL_H */
