#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "log.h"
#include "tlv.h"
#include "char.h"
#include "protocol.h"
#include "firmux-eeprom.h"

#define EEPROM_MAGIC "FXTLVEE"
#define EEPROM_VERSION 1

static int tlvp_dump(const char *key, void *val, int len)
{
	printf("%s=%s\n", key, (char *)val);
}

static ssize_t tlvp_copy(void **data_out, void *data_in, size_t size_in)
{
	if (!data_out)
		return size_in;

	*data_out = malloc(size_in);
	if (!*data_out)
		return -1;

	memcpy(*data_out, data_in, size_in);
	return size_in;
}

static ssize_t tlvp_parse_date_triplet(void **data_out, void *data_in, size_t size_in)
{
	unsigned char *buf;
	unsigned char year, month, day;

	if (sscanf(data_in, "%hhu-%hhu-%hhu", &year, &month, &day) < 3)
		return -1;

	if (!data_out)
		return 3;

	buf = malloc(3);
	if (!buf)
		return -1;

	buf[0] = year;
	buf[1] = month;
	buf[2] = day;
	*data_out = buf;
	return 3;
}

static ssize_t tlvp_format_date_triplet(void **data_out, void *data_in, size_t size_in)
{
#define DATE_STR_LEN 9
	unsigned char *buf;
	char *line;
	int cnt;

	if (!data_out)
		return DATE_STR_LEN;

	line = malloc(DATE_STR_LEN);
	if (!line)
		return -1;

	buf = data_in;
	cnt = snprintf(line, DATE_STR_LEN, "%u-%u-%u", buf[0], buf[1], buf[2]);
	*data_out = line;
	return cnt;
}

static ssize_t tlvp_parse_device_mac(void **data_out, void *data_in, size_t size_in, const char *param)
{
	char *buf;
	unsigned char oct[6];
	int cnt, len;

	cnt = sscanf(data_in, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
		     &oct[0], &oct[1], &oct[2], &oct[3], &oct[4], &oct[5]);
	if (cnt < sizeof(oct))
		return -1;

	len = sizeof(oct) + (param ? (strlen(param) + 1) : 0);
	if (!data_out)
		return len;

	buf = malloc(len);
	if (!buf)
		return -1;

	memcpy(buf, oct, sizeof(oct));
	if (param && strlen(param))
		strcpy((char *)buf + sizeof(oct), param);

	*data_out = buf;
	return len;
}

static ssize_t tlvp_format_device_mac(void **data_out, void *data_in, size_t size_in, char **param)
{
#define MAC_STR_LEN 18
	static char extra[16];
	unsigned char *buf = data_in;
	char *line;
	int cnt, extra_len = size_in - 6;

	if (param) {
		if (extra_len < 0)
			extra_len = 0;
		else if (extra_len >= sizeof(extra))
			extra_len = sizeof(extra) - 1;
		if (extra_len)
			strncpy(extra, (char *)buf + 6, extra_len);
		extra[extra_len] = '\0';
		*param = extra;
	}

	if (!data_out)
		return MAC_STR_LEN;

	line = malloc(MAC_STR_LEN);
	if (!line)
		return -1;

	cnt = snprintf(line, MAC_STR_LEN, "%02x:%02x:%02x:%02x:%02x:%02x",
		       buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
	*data_out = line;
	return cnt;
}

static struct tlv_property tlv_properties[] = {
	{ EEPROM_ATTR_PRODUCT_ID, "PRODUCT_ID", tlvp_copy, tlvp_copy },
	{ EEPROM_ATTR_PRODUCT_NAME, "PRODUCT_NAME", tlvp_copy, tlvp_copy },
	{ EEPROM_ATTR_SERIAL_NO, "SERIAL_NO", tlvp_copy, tlvp_copy },
	{ EEPROM_ATTR_PCB_NAME, "PCB_NAME", tlvp_copy, tlvp_copy },
	{ EEPROM_ATTR_PCB_REVISION, "PCB_REVISION", tlvp_copy, tlvp_copy },
	{ EEPROM_ATTR_PCB_PRDATE, "PCB_PRDATE", tlvp_parse_date_triplet, tlvp_format_date_triplet },
	{ EEPROM_ATTR_PCB_PRLOCATION, "PCB_PRLOCATION", tlvp_copy, tlvp_copy },
	{ EEPROM_ATTR_PCB_SN, "PCB_SN", tlvp_copy, tlvp_copy },
	{ EEPROM_ATTR_NONE, NULL, NULL, NULL, }
};

static struct tlv_group tlv_groups[] = {
	{ EEPROM_ATTR_MAC_FIRST, EEPROM_ATTR_MAC_LAST, "MAC_ADDR", tlvp_parse_device_mac, tlvp_format_device_mac },
	{ EEPROM_ATTR_NONE, EEPROM_ATTR_NONE, NULL, NULL, NULL }
};

static struct tlv_property *tlv_eeprom_prop_find(char *key)
{
	struct tlv_property *tlvp;

	tlvp = &tlv_properties[0];
	while (tlvp->tlvp_name) {
		if (!strcmp(tlvp->tlvp_name, key))
			break;
		tlvp++;
	}

	if (!tlvp->tlvp_name)
		return NULL;

	return tlvp;
}

static struct tlv_group *tlv_eeprom_param_find(char *key, char **param)
{
	struct tlv_group *tlvg;

	*param = NULL;

	tlvg = &tlv_groups[0];
	while (tlvg->tlvg_pattern) {
		if (!strncmp(tlvg->tlvg_pattern, key, strlen(tlvg->tlvg_pattern))) {
			*param = key + strlen(tlvg->tlvg_pattern) + 1;
			break;
		}
		tlvg++;
	}

	if (!tlvg->tlvg_pattern)
		return NULL;

	return tlvg;
}

static enum tlv_code tlv_eeprom_param_slot(struct tlv_store *tlvs, struct tlv_group *tlvg, char *param, int exact)
{
	enum tlv_code code, slot = EEPROM_ATTR_NONE;
	char *extra;
	void *data = NULL;
	int data_len = 0;
	ssize_t size;

	for (code = tlvg->tlvg_id_first; code <= tlvg->tlvg_id_last; code++) {
		size = tlvs_get(tlvs, code, 0, NULL);
		if (size < 0) {
			if (!exact && (slot == EEPROM_ATTR_NONE))
				slot = code;
			continue;
		}
		if (data_len < size) {
			data_len = size;
			data = realloc(data, size);
			if (!data) {
				perror("realloc() failed");
				break;
			}
		}

		if (tlvs_get(tlvs, code, size, data) < 0) {
			if (!exact && (slot == EEPROM_ATTR_NONE))
				slot = code;
			continue;
		}

		if (tlvg->tlvg_format(NULL, data, size, &extra) < 0)
			continue;

		if (extra && !strcmp(extra, param)) {
			slot = code;
			break;
		}
	}

	if (data)
		free(data);

	return slot;
}

static int tlv_eeprom_prop_check(char *key, char *val)
{
	struct tlv_property *tlvp;
	struct tlv_group *tlvg;
	char *param;

	tlvg = tlv_eeprom_param_find(key, &param);
	if (tlvg) {
		if (!val)
			return 0;

		return tlvg->tlvg_parse(NULL, val, strlen(val), param) == -1;
	}

	tlvp = tlv_eeprom_prop_find(key);
	if (tlvp) {
		if (!val)
			return 0;

		return tlvp->tlvp_parse(NULL, val, strlen(val)) == -1;
	}

	return -1;
}

static int tlv_eeprom_update(void *sp, char *key, char *val)
{
	struct tlv_store *tlvs = (struct tlv_store *)sp;
	struct tlv_property *tlvp;
	struct tlv_group *tlvg;
	enum tlv_code code;
	char *param;
	void *data;
	ssize_t size;
	int ret;

	if ((tlvg = tlv_eeprom_param_find(key, &param))) {
		code = tlv_eeprom_param_slot(tlvs, tlvg, param, 0);
		if (code == EEPROM_ATTR_NONE) {
			ldebug("Failed TLV param '%s' slot lookup", param);
			return -1;
		}
		size = tlvg->tlvg_parse(&data, val, strlen(val), param);
		if (size < 0) {
			lerror("Failed TLV param '%s' parse, size %zu", param, strlen(val));
			return -1;
		}
	} else if ((tlvp = tlv_eeprom_prop_find(key))) {
		code = tlvp->tlvp_id;
		size = tlvp->tlvp_parse(&data, val, strlen(val));
		if (size < 0) {
			lerror("Failed TLV property parse, size %zu", strlen(val));
			return -1;
		}
	} else {
		ldebug("Invalid TLV property '%s'", key);
		return -1;
	}

	ret = tlvs_set(tlvs, code, size, data);
	free(data);
	return ret;
}

static struct tlv_property *tlv_eeprom_prop_format(struct tlv_field *tlv, char **key)
{
	struct tlv_property *tlvp;

	if (key)
		*key = NULL;

	tlvp = &tlv_properties[0];
	while (tlvp->tlvp_name) {
		if (tlvp->tlvp_id == tlv->type)
			break;
		tlvp++;
	}

	if (!tlvp->tlvp_name)
		return NULL;

	if (key) {
		*key = strndup(tlvp->tlvp_name, strlen(tlvp->tlvp_name));
		if (!*key)
			return NULL;
	}

	return tlvp;
}

static struct tlv_group *tlv_eeprom_param_format(struct tlv_field *tlv, char **key)
{
	struct tlv_group *tlvg;
	ssize_t len;
	char *val;
	char *param;

	if (key)
		*key = NULL;

	tlvg = &tlv_groups[0];
	while (tlvg->tlvg_pattern) {
		if (tlvg->tlvg_id_first <= tlv->type && tlvg->tlvg_id_last >= tlv->type)
			break;
		tlvg++;
	}

	if (!tlvg->tlvg_pattern)
		return NULL;

	if (key) {
		if (tlvg->tlvg_format(NULL, tlv->value, tlv->length, &param) < 0)
			return NULL;
		*key = malloc(strlen(tlvg->tlvg_pattern) + strlen(param) + 2);
		if (!*key)
			return NULL;
		sprintf(*key, "%s_%s", tlvg->tlvg_pattern, param);
	}

	return tlvg;
}

static int tlv_eeprom_dump_all(struct tlv_store *tlvs)
{
	struct tlv_iterator iter;
	struct tlv_field *tlv;
	struct tlv_property *tlvp;
	struct tlv_group *tlvg;
	char *key;
	char *val;
	ssize_t len;
	int fail = 0;

	tlvs_iter_init(&iter, tlvs);

	while ((tlv = tlvs_iter_next(&iter)) != NULL) {
		val = NULL;
		key = NULL;

		if ((tlvg = tlv_eeprom_param_format(tlv, &key))) {
			len = tlvg->tlvg_format((void **)&val, tlv->value, tlv->length, NULL);
			if (len < 0) {
				lerror("Failed to format TLV param %s", key);
				fail++;
				goto next;
			}
		} else if ((tlvp = tlv_eeprom_prop_format(tlv, &key))) {
			len = tlvp->tlvp_format((void **)&val, tlv->value, tlv->length);
			if (len < 0) {
				lerror("Failed to format TLV param %s", key);
				fail++;
				goto next;
			}
		} else {
			lerror("Invalid TLV property type '%i'", tlv->type);
			fail++;
			goto next;
		}

		tlvp_dump(key, val, len);

next:
		free(key);
		if (val)
			free(val);
	}

	return fail;
}

static int tlv_eeprom_dump(void *sp, char *key)
{
	struct tlv_store *tlvs = (struct tlv_store *)sp;
	struct tlv_property *tlvp;
	struct tlv_group *tlvg;
	enum tlv_code code;
	void *data;
	char *param;
	char *val;
	ssize_t size, len;

	if (!key)
		return tlv_eeprom_dump_all(tlvs);

	if ((tlvg = tlv_eeprom_param_find(key, &param))) {
		code = tlv_eeprom_param_slot(tlvs, tlvg, param, 1);
		if (code == EEPROM_ATTR_NONE) {
			ldebug("Failed TLV param '%s' slot lookup", param);
			return -1;
		}
	} else if ((tlvp = tlv_eeprom_prop_find(key))) {
		code = tlvp->tlvp_id;
	} else {
		lerror("Invalid TLV property '%s'", key);
		return -1;
	}

	size = tlvs_get(tlvs, code, 0, NULL);
	if (size < 0) {
		lerror("Failed TLV property '%s' get", key);
		return -1;
	}

	data = malloc(size);
	if (!data) {
		perror("malloc() failed");
		return -1;
	}

	if (tlvs_get(tlvs, code, size, data) < 0) {
		free(data);
		return -1;
	}

	if (tlvg) {
		len = tlvg->tlvg_format((void **)&val, data, size, NULL);
	} else if (tlvp) {
		len = tlvp->tlvp_format((void **)&val, data, size);
	}
	free(data);
	if (len < 0) {
		lerror("Failed TLV property format, size %zu", size);
		return -1;
	}

	tlvp_dump(key, val, len);

	free(val);

	return 0;
}

static int tlv_eeprom_export(void *sp, char *key, char *fname)
{
	struct tlv_store *tlvs = (struct tlv_store *)sp;
	struct tlv_property *tlvp;
	struct tlv_group *tlvg;
	enum tlv_code code;
	FILE *fp;
	void *data, *fdata;
	ssize_t size, fsize;
	char *param;

	if ((tlvg = tlv_eeprom_param_find(key, &param))) {
		code = tlv_eeprom_param_slot(tlvs, tlvg, param, 1);
		if (code == EEPROM_ATTR_NONE) {
			ldebug("Failed TLV param '%s' slot lookup", param);
			return -1;
		}
	} else if ((tlvp = tlv_eeprom_prop_find(key))) {
		code = tlvp->tlvp_id;
	} else {
		ldebug("Invalid TLV property '%s'", key);
		return -1;
	}

	fp = fopen(fname, "wb");
	if (!fp) {
		perror("fopen() failed");
		return -1;
	}

	size = tlvs_get(tlvs, code, 0, NULL);
	if (size < 0) {
		lerror("Failed TLV property '%s' get", key);
		fclose(fp);
		return -1;
	}

	data = malloc(size);
	if (!data) {
		perror("malloc() failed");
		fclose(fp);
		return -1;
	}

	if (tlvs_get(tlvs, code, size, data) < 0) {
		fclose(fp);
		free(data);
		return -1;
	}

	if (tlvg) {
		fsize = tlvg->tlvg_format(&fdata, data, size, NULL);
	} else if (tlvp) {
		fsize = tlvp->tlvp_format(&fdata, data, size);
	}
	free(data);
	if (fsize < 0) {
		lerror("Failed TLV property format, size %zu", size);
		fclose(fp);
		return -1;
	}

	if (fwrite(fdata, 1, fsize, fp) != fsize) {
		perror("fwrite() failed");
		fclose(fp);
		free(fdata);
		return -1;
	}

	fclose(fp);
	free(fdata);

	return 0;
}

static int tlv_eeprom_import(void *sp, char *key, char *fname)
{
	struct tlv_store *tlvs = (struct tlv_store *)sp;
	struct tlv_property *tlvp;
	struct tlv_group *tlvg;
	enum tlv_code code;
	struct stat st;
	FILE *fp;
	void *fdata, *data = NULL;
	ssize_t fsize, size;
	char *param;
	int ret;

	if ((tlvg = tlv_eeprom_param_find(key, &param))) {
		code = tlv_eeprom_param_slot(tlvs, tlvg, param, 0);
		if (code == EEPROM_ATTR_NONE) {
			ldebug("Failed TLV param '%s' slot lookup", param);
			return -1;
		}
	} else if ((tlvp = tlv_eeprom_prop_find(key))) {
		code = tlvp->tlvp_id;
	} else {
		ldebug("Invalid TLV property '%s'", key);
		return -1;
	}

	fp = fopen(fname, "rb");
	if (!fp) {
		perror("fopen() failed");
		return -1;
	}

	if (fstat(fileno(fp), &st) != 0) {
		perror("fstat() failed");
		fclose(fp);
		return -1;
	}

	fsize = st.st_size;
	fdata = malloc(fsize);
	if (!fdata) {
		perror("malloc() failed");
		fclose(fp);
		return -1;
	}

	if (fread(fdata, 1, fsize, fp) != fsize) {
		perror("fread() failed");
		fclose(fp);
		free(fdata);
		return -1;
	}

	fclose(fp);

	if (tlvg) {
		size = tlvg->tlvg_parse(&data, fdata, fsize, param);
		if (size < 0) {
			lerror("Failed TLV param '%s' parse, size %zu", param, fsize);
			free(fdata);
			return -1;
		}
	} else if (tlvp) {
		size = tlvp->tlvp_parse(&data, fdata, fsize);
		if (size < 0) {
			lerror("Failed TLV property parse, size %zu", fsize);
			free(fdata);
			return -1;
		}
	}

	ret = tlvs_set(tlvs, code, size, data);
	free(data);
	free(fdata);
	return ret;
}

static void tlv_eeprom_list(void)
{
	struct tlv_property *tlvp;
	struct tlv_group *tlvg;

	tlvp = &tlv_properties[0];
	while (tlvp->tlvp_name) {
		printf("%s\n", tlvp->tlvp_name);
		tlvp++;
	}

	tlvg = &tlv_groups[0];
	while (tlvg->tlvg_pattern) {
		printf("%s*\n", tlvg->tlvg_pattern);
		tlvg++;
	}
}

static void tlv_eeprom_free(void *sp)
{
	tlvs_free((struct tlv_store *)sp);
}

static void *tlv_eeprom_init(struct tlv_device *tlvd, int force)
{
	struct tlv_header *tlvh;
	struct tlv_store *tlvs;
	int empty, len = 0;

	if (tlvd->size <= sizeof(*tlvh)) {
		lerror("Storage is too small %zu/%zu", tlvd->size, sizeof(*tlvh));
		return NULL;
	}

	tlvh = tlvd->base;
	if (!strncmp(tlvh->magic, EEPROM_MAGIC, sizeof(tlvh->magic)) &&
	    tlvh->version == EEPROM_VERSION)
		goto done;

	empty = 1;
	while (len < sizeof(*tlvh)) {
		if (((unsigned char *)tlvd->base)[len] != 0xFF) {
			empty = 0;
			break;
		}
		len++;
	}

	if (empty || force) {
		if (force)
			ldebug("Reinitialising non-empty storage");
		memset(tlvh, 0, sizeof(*tlvh));
		memcpy(tlvh->magic, EEPROM_MAGIC, sizeof(tlvh->magic));
		tlvh->version = EEPROM_VERSION;
	} else {
		ldebug("Unknown storage signature");
		return NULL;
	}

done:
	tlvs = tlvs_init(tlvd->base + sizeof(*tlvh), tlvd->size - sizeof(*tlvh));
	if (!tlvs)
		lerror("Failed to initialize TLV store");

	return tlvs;
}

static struct tlv_protocol firmux_protocol = {
	.name = "firmux-eeprom",
	.def = 1,
	.init = tlv_eeprom_init,
	.free = tlv_eeprom_free,
	.list = tlv_eeprom_list,
	.check = tlv_eeprom_prop_check,
	.update = tlv_eeprom_update,
	.dump = tlv_eeprom_dump,
	.save = tlv_eeprom_export,
	.load = tlv_eeprom_import,
};

static void __attribute__((constructor)) firmux_eeprom_register(void)
{
	tlvp_register(&firmux_protocol);
}
