
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "log.h"
#include "protocol.h"

struct proto_entry {
	struct tlv_protocol *proto;
	struct proto_entry *next;
};

static struct proto_entry *proto_list;
static struct tlv_protocol *proto_default;

int tlvp_register(struct tlv_protocol *tlvp)
{
	struct proto_entry *entry;

	if (tlvp->def) {
		if (proto_default) {
			lerror("Default protocol already registered");
			return -1;
		}

		ldebug("Registering default protocol: %s", tlvp->name);
		proto_default = tlvp;
		return 0;
	}

	ldebug("Registering protocol: %s", tlvp->name);

	entry = malloc(sizeof(struct proto_entry));
	if (!entry) {
		perror("malloc() failed");
		return -1;
	}

	entry->proto = tlvp;
	entry->next = proto_list;
	proto_list = entry;

	return 0;
}

void tlvp_unregister(void)
{
	struct proto_entry *tmp, *ptr = proto_list;

	while (ptr) {
		tmp = ptr->next;
		free(ptr);
		ptr = tmp;
	}

	proto_list = NULL;
}

struct tlv_protocol *tlvp_init(struct tlv_device *tlvd, int force)
{
	struct proto_entry *entry;
	struct tlv_protocol *tlvp;
	struct tlv_protocol *proto = NULL;
	void *priv = NULL;

	proto = proto_default;
	priv = proto->init(tlvd, force);
	if (!priv && !force) {
		for (entry = proto_list; entry != NULL; entry = entry->next) {
			priv = entry->proto->init(tlvd, 0);
			if (!priv)
				continue;
			proto = entry->proto;
			break;
		}
	}

	if (!priv) {
		lerror("No matching protocol found for storage device");
		return NULL;
	}

	ldebug("Initialising storage protocol: %s", proto->name);

	tlvp = malloc(sizeof(*tlvp));
	if (!tlvp) {
		proto->free(priv);
		perror("malloc() failed");
		return NULL;
	}

	memcpy(tlvp, proto, sizeof(*tlvp));
	tlvp->priv = priv;

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
