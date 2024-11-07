#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "log.h"
#include "tlv.h"
#include "firmux-eeprom.h"

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

static struct tlv_property *tlv_eeprom_prop_find(char *key)
{
	struct tlv_property *tlvp;

	tlvp = &tlv_properties[0];
	while (tlvp->tlvp_name) {
		if (!strcmp(tlvp->tlvp_name, key))
			break;
		tlvp++;
	}

	return tlvp;
}

int tlv_eeprom_prop_check(char *key, char *val)
{
	struct tlv_property *tlvp;

	tlvp = tlv_eeprom_prop_find(key);
	if (!tlvp || !tlvp->tlvp_id)
		return -1;

	if (!val)
		return 0;

	return tlvp->tlvp_parse(NULL, val, strlen(val)) == -1;
}

int tlv_eeprom_update(struct tlv_store *tlvs, char *key, char *val)
{
	struct tlv_property *tlvp;
	void *data;
	ssize_t size;
	int ret;

	tlvp = tlv_eeprom_prop_find(key);
	if (!tlvp || !tlvp->tlvp_id) {
		lerror("Invalid TLV property '%s'", key);
		return -1;
	}

	size = tlvp->tlvp_parse(&data, val, strlen(val));
	if (size < 0) {
		lerror("Failed TLV property parse, size %zu", strlen(val));
		return -1;
	}

	ret = tlvs_set(tlvs, tlvp->tlvp_id, size, data);
	free(data);
	return ret;
}

int tlv_eeprom_dump(struct tlv_store *tlvs, char *key)
{
	struct tlv_property *tlvp;
	char *val;
	void *data;
	ssize_t size, len;

	tlvp = tlv_eeprom_prop_find(key);
	if (!tlvp || !tlvp->tlvp_id) {
		lerror("Invalid TLV property '%s'", key);
		return -1;
	}

	size = tlvs_get(tlvs, tlvp->tlvp_id, 0, NULL);
	if (size < 0) {
		lerror("Failed TLV property '%s' get", key);
		return -1;
	}

	data = malloc(size);
	if (!data) {
		perror("malloc() failed");
		return -1;
	}

	if (tlvs_get(tlvs, tlvp->tlvp_id, size, data) < 0) {
		free(data);
		return -1;
	}

	len = tlvp->tlvp_format((void **)&val, data, size);
	if (len < 0) {
		lerror("Failed TLV property format, size %zu", size);
		free(data);
		return -1;
	}

	tlvp_dump(tlvp->tlvp_name, val, len);

	free(val);

	return 0;
}

int tlv_eeprom_export(struct tlv_store *tlvs, char *key, char *fname)
{
	struct tlv_property *tlvp;
	FILE *fp;
	void *data, *fdata;
	ssize_t size, fsize;

	tlvp = tlv_eeprom_prop_find(key);
	if (!tlvp || !tlvp->tlvp_id) {
		lerror("Invalid TLV property '%s'", key);
		return -1;
	}

	fp = fopen(fname, "wb");
	if (!fp) {
		perror("fopen() failed");
		return -1;
	}

	size = tlvs_get(tlvs, tlvp->tlvp_id, 0, NULL);
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

	if (tlvs_get(tlvs, tlvp->tlvp_id, size, data) < 0) {
		fclose(fp);
		free(data);
		return -1;
	}

	fsize = tlvp->tlvp_format(&fdata, data, size);
	free(data);
	if (fsize < 0) {
		lerror("Failed TLV property format, size %zu", size);
		fclose(fp);
		free(fdata);
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

int tlv_eeprom_import(struct tlv_store *tlvs, char *key, char *fname)
{
	struct tlv_property *tlvp;
	struct stat st;
	FILE *fp;
	void *fdata, *data = NULL;
	ssize_t fsize, size;
	int ret;

	tlvp = tlv_eeprom_prop_find(key);
	if (!tlvp || !tlvp->tlvp_id) {
		lerror("Invalid TLV property '%s'", key);
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

	size = tlvp->tlvp_parse(&data, fdata, fsize);
	free(fdata);
	if (size < 0) {
		lerror("Failed TLV property parse, size %zu", fsize);
		return -1;
	}

	ret = tlvs_set(tlvs, tlvp->tlvp_id, size, data);
	free(data);
	return ret;
}

void tlv_eeprom_list(void)
{
	struct tlv_property *tlvp;

	tlvp = &tlv_properties[0];
	while (tlvp->tlvp_name) {
		printf("%s\n", tlvp->tlvp_name);
		tlvp++;
	}
}
