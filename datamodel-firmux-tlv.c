#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_LZMA_H
#include <lzma.h>
#endif

#include "crc.h"

#include "log.h"
#include "utils.h"
#include "tlv.h"
#include "char.h"
#include "protocol.h"
#include "datamodel-firmux-tlv.h"

#define EEPROM_MAGIC "FXDMTLV"
#define EEPROM_VERSION 1

#ifndef TLVS_DEFAULT_COMPRESSION
#define TLVS_DEFAULT_COMPRESSION (9 | LZMA_PRESET_EXTREME)
#endif

static int data_dump(const char *key, void *val, int len, enum tlv_spec type)
{
	int i;

	if (type == INPUT_SPEC_TXT) {
		printf("%s=%s\n", key, (char *)val);
	} else if (type == INPUT_SPEC_BIN) {
		printf("%s[%d] {", key, len);
		for (i = 0; i < len; i++) {
			if (!(i % 16)) {
				putchar('\n');
				putchar(' ');
			}
			printf("%02x ", 0xFF & ((char *)val)[i]);
		}
		puts("\n}");
	}
}

static ssize_t tlvp_parse_device_mac(void **data_out, void *data_in, size_t size_in, const char *param)
{
	char *extra_buf;
	int extra_len, len;

	len = aparse_mac_address(data_out, data_in, size_in);
	if (len < -1)
		return len;

	extra_len = len + (param ? (strlen(param) + 1) : 0);
	if (!data_out)
		return extra_len;

	if (param && strlen(param)) {
		extra_buf = realloc(*data_out, extra_len);
		if (!extra_buf) {
			perror("realloc() failed");
			free(*data_out);
			return -1;
		}
		strcpy((char *)extra_buf + len, param);
		*data_out = extra_buf;
		len = extra_len;
	}

	return len;
}

static ssize_t tlvp_format_device_mac(void **data_out, void *data_in, size_t size_in, char **param)
{
	static char extra[16];
	int extra_len = size_in - 6;

	if (param) {
		if (extra_len < 0)
			extra_len = 0;
		else if (extra_len >= sizeof(extra))
			extra_len = sizeof(extra) - 1;
		if (extra_len)
			strncpy(extra, (char *)data_in + 6, extra_len);
		extra[extra_len] = '\0';
		*param = extra;
	}

	return aformat_mac_address(data_out, data_in, size_in);
}

static ssize_t tlvp_input_bin(void **data_out, void *data_in, size_t size_in)
{
	return acopy_data(data_out, data_in, size_in);
}

static ssize_t tlvp_output_bin(void **data_out, void *data_in, size_t size_in)
{
	return acopy_data(data_out, data_in, size_in);
}

#ifdef HAVE_LZMA_H
static ssize_t tlvp_compress_bin(void **data_out, void *data_in, size_t size_in)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret ret;
	uint32_t preset = TLVS_DEFAULT_COMPRESSION;
	unsigned char *out_tmp, *out_buf;
	size_t out_next, out_size = size_in;
	size_t size_out = 0;

	if (!data_out)
		return out_size;

	out_buf = malloc(out_size);
	if (!out_buf)
		return -1;

	ret = lzma_easy_encoder(&strm, preset, LZMA_CHECK_CRC64);
	if (ret != LZMA_OK) {
		ldebug("Failed to initialize LZMA encoder, error code: %d", ret);
		free(out_buf);
		return -1;
	}

	strm.next_in = data_in;
	strm.avail_in = size_in;
	strm.next_out = out_buf;
	strm.avail_out = out_size;

	while (1) {
		if (strm.avail_out == 0) {
			out_next = out_size * 2;
			out_tmp = realloc(out_buf, out_next);
			if (!out_tmp) {
				perror("realloc() failed");
				lzma_end(&strm);
				free(out_buf);
				return -1;
			}
			out_buf = out_tmp;
			strm.next_out = out_buf + out_size;
			strm.avail_out = out_size;
			out_size = out_next;
		}

		ret = lzma_code(&strm, strm.avail_in ? LZMA_RUN : LZMA_FINISH);
		if (ret == LZMA_STREAM_END) {
			size_out = out_size - strm.avail_out;
			break;
		}
		if (ret != LZMA_OK) {
			lerror("LZMA compression error: %d", ret);
			lzma_end(&strm);
			free(out_buf);
			return -1;
		}
	}

	lzma_end(&strm);

	out_tmp = realloc(out_buf, size_out);
	if (!out_tmp) {
		perror("realloc() failed");
		free(out_buf);
		return -1;
	}

	*data_out = out_tmp;
	return size_out;
}

static ssize_t tlvp_decompress_bin(void **data_out, void *data_in, size_t size_in)
{
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_ret ret;
	unsigned char *out_tmp, *out_buf;
	size_t out_next, out_size = size_in * 4;
	size_t size_out = 0;

	if (!data_out)
		return out_size;

	out_buf = malloc(out_size);
	if (!out_buf)
		return -1;

	ret = lzma_auto_decoder(&strm, UINT64_MAX, 0);
	if (ret != LZMA_OK) {
		ldebug("Failed to initialize LZMA decoder, error code: %d", ret);
		free(out_buf);
		return -1;
	}

	strm.next_in = data_in;
	strm.avail_in = size_in;
	strm.next_out = out_buf;
	strm.avail_out = out_size;

	while (1) {
		if (strm.avail_out == 0) {
			out_next = out_size * 2;
			out_tmp = realloc(out_buf, out_next);
			if (!out_tmp) {
				perror("realloc() failed");
				lzma_end(&strm);
				free(out_buf);
				return -1;
			}
			out_buf = out_tmp;
			strm.next_out = out_buf + out_size;
			strm.avail_out = out_size;
		}

		ret = lzma_code(&strm, LZMA_RUN);
		if (ret == LZMA_STREAM_END) {
			size_out = out_size - strm.avail_out;
			break;
		}
		if (ret != LZMA_OK) {
			/* Error occurred */
			ldebug("LZMA decompression error: %d", ret);
			lzma_end(&strm);
			free(out_buf);
			return -1;
		}
	}

	lzma_end(&strm);

	out_tmp = realloc(out_buf, size_out);
	if (!out_tmp) {
		free(out_buf);
		return -1;
	}

	*data_out = out_tmp;
	return size_out;
}
#else
static ssize_t tlvp_compress_bin(void **data_out, void *data_in, size_t size_in)
{
	return acopy_data(data_out, data_in, size_in);
}

static ssize_t tlvp_decompress_bin(void **data_out, void *data_in, size_t size_in)
{
	return acopy_data(data_out, data_in, size_in);
}
#endif

static struct tlv_property tlv_properties[] = {
	{ "PRODUCT_ID", EEPROM_ATTR_PRODUCT_ID, INPUT_SPEC_TXT, acopy_data, acopy_data },
	{ "PRODUCT_NAME", EEPROM_ATTR_PRODUCT_NAME, INPUT_SPEC_TXT, acopy_data, acopy_data },
	{ "SERIAL_NO", EEPROM_ATTR_SERIAL_NO, INPUT_SPEC_TXT, acopy_data, acopy_data },
	{ "PCB_NAME", EEPROM_ATTR_PCB_NAME, INPUT_SPEC_TXT, acopy_data, acopy_data },
	{ "PCB_REVISION", EEPROM_ATTR_PCB_REVISION, INPUT_SPEC_TXT, acopy_data, acopy_data },
	{ "PCB_PRDATE", EEPROM_ATTR_PCB_PRDATE, INPUT_SPEC_TXT, aparse_byte_triplet, aformat_byte_triplet },
	{ "PCB_PRLOCATION", EEPROM_ATTR_PCB_PRLOCATION, INPUT_SPEC_TXT, acopy_data, acopy_data },
	{ "PCB_SN", EEPROM_ATTR_PCB_SN, INPUT_SPEC_TXT, acopy_data, acopy_data },
	{ "XTAL_CALDATA", EEPROM_ATTR_XTAL_CAL_DATA, INPUT_SPEC_BIN, tlvp_input_bin, tlvp_output_bin },
	{ "RADIO_CALDATA", EEPROM_ATTR_RADIO_CAL_DATA, INPUT_SPEC_BIN, tlvp_compress_bin, tlvp_decompress_bin },
	{ "RADIO_BRDDATA", EEPROM_ATTR_RADIO_BOARD_DATA, INPUT_SPEC_BIN, tlvp_compress_bin, tlvp_decompress_bin },
	{ NULL, EEPROM_ATTR_NONE, INPUT_SPEC_NONE, NULL, NULL, }
};

static struct tlv_group tlv_groups[] = {
	{ "MAC_ADDR", EEPROM_ATTR_MAC_FIRST, EEPROM_ATTR_MAC_LAST, INPUT_SPEC_TXT, tlvp_parse_device_mac, tlvp_format_device_mac },
	{ NULL, EEPROM_ATTR_NONE, EEPROM_ATTR_NONE, INPUT_SPEC_NONE, NULL, NULL }
};

static struct tlv_property *firmux_tlv_prop_find(char *key)
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

static struct tlv_group *firmux_tlv_param_find(char *key, char **param)
{
	struct tlv_group *tlvg;

	*param = NULL;

	tlvg = &tlv_groups[0];
	while (tlvg->tlvg_pattern) {
		if (!strncmp(tlvg->tlvg_pattern, key, strlen(tlvg->tlvg_pattern))) {
			*param = key + strlen(tlvg->tlvg_pattern);
			break;
		}
		tlvg++;
	}

	if (!*param || **param == '\0')
		return NULL;

	/* Skip the separator character */
	(*param)++;

	if (!tlvg->tlvg_pattern)
		return NULL;

	return tlvg;
}

static enum tlv_code firmux_tlv_param_slot(struct tlv_store *tlvs, struct tlv_group *tlvg, char *param, int exact)
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

static int firmux_tlv_prop_check(char *key, char *in)
{
	struct tlv_property *tlvp;
	struct tlv_group *tlvg;
	char *param;
	char *val = NULL;
	size_t len = 0;
	int ret = -1;

	if (in && in[0] == '@') {
		val = afread(in + 1, &len);
	} else if (in) {
		val = in;
		len = strlen(in);
	}

	tlvg = firmux_tlv_param_find(key, &param);
	if (tlvg) {
		if (!val)
			return 0;

		ret = tlvg->tlvg_parse(NULL, val, len, param) == -1;
		goto out;
	}

	tlvp = firmux_tlv_prop_find(key);
	if (tlvp) {
		if (!val)
			return 0;

		ret = tlvp->tlvp_parse(NULL, val, len) == -1;
		goto out;
	}

out:
	if (in && in[0] == '@')
		free(val);

	return ret;
}

static struct tlv_property *firmux_tlv_prop_format(struct tlv_field *tlv, char **key)
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

static struct tlv_group *firmux_tlv_param_format(struct tlv_field *tlv, char **key)
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

static int firmux_tlv_prop_store(void *sp, char *key, char *in)
{
	struct tlv_store *tlvs = (struct tlv_store *)sp;
	struct tlv_property *tlvp;
	struct tlv_group *tlvg;
	enum tlv_code code;
	char *param;
	char *val;
	void *data = NULL;
	ssize_t size, len;
	int ret = -1;

	if ((tlvg = firmux_tlv_param_find(key, &param))) {
		code = firmux_tlv_param_slot(tlvs, tlvg, param, 0);
		if (code == EEPROM_ATTR_NONE) {
			ldebug("Failed TLV param '%s' slot lookup", param);
			return -1;
		}
	} else if ((tlvp = firmux_tlv_prop_find(key))) {
		code = tlvp->tlvp_id;
	} else {
		ldebug("Invalid TLV property '%s'", key);
		return -1;
	}

	if (!in)
		return -1;

	if (in[0] == '@') {
		val = afread(in + 1, &len);
		if (!val) {
			lerror("Failed to read file '%s'", in + 1);
			return -1;
		}
	} else {
		val = in;
		len = strlen(in);
	}

	if (tlvg) {
		size = tlvg->tlvg_parse(&data, val, len, param);
		if (size < 0) {
			lerror("Failed TLV param '%s' parse, size %zu", param, len);
			goto fail;
		}
	} else if (tlvp) {
		size = tlvp->tlvp_parse(&data, val, len);
		if (size < 0) {
			lerror("Failed TLV property parse, size %zu", len);
			goto fail;
		}
	}

	ret = tlvs_set(tlvs, code, size, data);

fail:
	if (data)
		free(data);
	if (in[0] == '@')
		free(val);
	return ret;
}

static int firmux_tlv_print_all(struct tlv_store *tlvs)
{
	struct tlv_iterator iter;
	struct tlv_field *tlv;
	struct tlv_property *tlvp;
	struct tlv_group *tlvg;
	enum tlv_spec spec;
	char *key;
	char *val;
	ssize_t len;
	int fail = 0;

	tlvs_iter_init(&iter, tlvs);

	while ((tlv = tlvs_iter_next(&iter)) != NULL) {
		val = NULL;
		key = NULL;

		if ((tlvg = firmux_tlv_param_format(tlv, &key))) {
			len = tlvg->tlvg_format((void **)&val, tlv->value, tlv->length, NULL);
			if (len < 0) {
				lerror("Failed to format TLV param %s", key);
				fail++;
				goto next;
			}
			spec = tlvg->tlvg_spec;
		} else if ((tlvp = firmux_tlv_prop_format(tlv, &key))) {
			len = tlvp->tlvp_format((void **)&val, tlv->value, tlv->length);
			if (len < 0) {
				lerror("Failed to format TLV param %s", key);
				fail++;
				goto next;
			}
			spec = tlvp->tlvp_spec;
		} else {
			lerror("Invalid TLV property type '%i'", tlv->type);
			fail++;
			goto next;
		}

		data_dump(key, val, len, spec);

next:
		free(key);
		if (val)
			free(val);
	}

	return fail;
}

static int firmux_tlv_prop_print(void *sp, char *key, char *out)
{
	struct tlv_store *tlvs = (struct tlv_store *)sp;
	struct tlv_property *tlvp;
	struct tlv_group *tlvg;
	enum tlv_code code;
	enum tlv_spec spec;
	char *param;
	char *val;
	void *data;
	ssize_t size, len;

	if (!key)
		return firmux_tlv_print_all(tlvs);

	if ((tlvg = firmux_tlv_param_find(key, &param))) {
		code = firmux_tlv_param_slot(tlvs, tlvg, param, 1);
		if (code == EEPROM_ATTR_NONE) {
			ldebug("Failed TLV param '%s' slot lookup", param);
			return -1;
		}
	} else if ((tlvp = firmux_tlv_prop_find(key))) {
		code = tlvp->tlvp_id;
	} else {
		ldebug("Invalid TLV property '%s'", key);
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
		spec = tlvg->tlvg_spec;
	} else if (tlvp) {
		len = tlvp->tlvp_format((void **)&val, data, size);
		spec = tlvp->tlvp_spec;
	}
	free(data);
	if (len < 0) {
		lerror("Failed TLV property format, size %zu", size);
		return -1;
	}

	if (out)
		afwrite(out[0] == '@' ? out + 1 : out, val, len);
	else
		data_dump(key, val, len, spec);

	free(val);

	return 0;
}

static void firmux_tlv_prop_list(void)
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

static int firmux_tlv_flush(void *sp)
{
	struct tlv_store *tlvs = sp;
	struct tlv_header *tlvh = tlvs->base - sizeof(*tlvh);

	if (tlvs->dirty) {
		tlvh->len = tlvs_len(sp);
		tlvh->crc = crc_32(tlvs->base, tlvh->len);
		tlvs->dirty = 0;
	}

	return 0;
}

static void firmux_tlv_free(void *sp)
{
	tlvs_free((struct tlv_store *)sp);
}

static void *firmux_tlv_init(struct tlv_device *tlvd, int force)
{
	struct tlv_header *tlvh;
	struct tlv_store *tlvs;
	int empty, len = 0;
	unsigned int crc;

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
	crc = crc_32(tlvd->base + sizeof(*tlvh), tlvh->len);
	if (crc != tlvh->crc) {
		lerror("Invalid storage crc\n");
		return NULL;
	}

	tlvs = tlvs_init(tlvd->base + sizeof(*tlvh), tlvd->size - sizeof(*tlvh));
	if (!tlvs)
		lerror("Failed to initialize TLV store");

	return tlvs;
}

static struct tlv_protocol firmux_tlv_model = {
	.name = "firmux-tlv",
	.def = 1,
	.init = firmux_tlv_init,
	.free = firmux_tlv_free,
	.list = firmux_tlv_prop_list,
	.check = firmux_tlv_prop_check,
	.print = firmux_tlv_prop_print,
	.store = firmux_tlv_prop_store,
	.flush = firmux_tlv_flush,
};

static void __attribute__((constructor)) firmux_tlv_register(void)
{
	tlvp_register(&firmux_tlv_model);
}
