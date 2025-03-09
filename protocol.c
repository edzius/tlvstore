
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "protocol.h"

static struct tlv_protocol *tp;

int tlvp_register(struct tlv_protocol *tlvp)
{
	ldebug("Registering protocol: %s", tlvp->name);

	tp = tlvp;

	return 0;
}

struct tlv_protocol *tlvp_init(struct tlv_device *tlvd)
{
	struct tlv_protocol *tlvp;

	ldebug("Initialising storage protocol: %s", tp->name);

	tlvp = malloc(sizeof(*tlvp));
	if (!tlvp) {
		perror("malloc() failed");
		return NULL;
	}

	memcpy(tlvp, tp, sizeof(*tlvp));

	tlvp->priv = tlvp->init(tlvd);
	if (!tlvp->priv) {
		lerror("Failed to initialize '%s' protocol", tp->name);
		free(tlvp);
		return NULL;
	}

	return tlvp;
}

void tlvp_free(struct tlv_protocol *tlvp)
{
	ldebug("Releasing protocol storage: %s", tlvp->name);
	tlvp->free(tlvp->priv);
	free(tlvp);
}

void tlvp_eeprom_list(struct tlv_protocol *tlvp)
{
	tlvp->list();
}

int tlvp_eeprom_check(struct tlv_protocol *tlvp, char *key, char *val)
{
	if (!key)
		return -1;

	return tlvp->check(key, val);
}

int tlvp_eeprom_update(struct tlv_protocol *tlvp, char *key, char *val)
{
	if (!key || !val)
		return -1;

	return tlvp->update(tlvp->priv, key, val);
}

int tlvp_eeprom_dump(struct tlv_protocol *tlvp, char *key)
{
	return tlvp->dump(tlvp->priv, key);
}

int tlvp_eeprom_save(struct tlv_protocol *tlvp, char *key, char *fname)
{
	if (!key || !fname)
		return -1;

	return tlvp->save(tlvp->priv, key, fname);
}

int tlvp_eeprom_load(struct tlv_protocol *tlvp, char *key, char *fname)
{
	if (!key || !fname)
		return -1;

	return tlvp->load(tlvp->priv, key, fname);
}
