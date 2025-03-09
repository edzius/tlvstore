
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "tlv.h"
#include "char.h"
#include "protocol.h"

#ifndef TLVS_DEFAULT_FILE
#define TLVS_DEFAULT_FILE NULL
#endif
#ifndef TLVS_DEFAULT_SIZE
#define TLVS_DEFAULT_SIZE 0
#endif

#define OP_LIST 1
#define OP_GET 2
#define OP_SET 3

struct params_list {
	struct params_list *next;
	char *key;
	char *val;
};

static struct params_list *pl;
static struct tlv_device *tlvd;
static struct tlv_protocol *tlvp;
static int op;

int tlvstore_parse_line(char *arg)
{
	char *key, *val;
	struct params_list *pe;

	key = strdup(arg);
	val = strchr(key, '=');
	if (val) {
		*val = 0;
		val++;
	}

	ldebug("Parsed parameter: '%s' = '%s'", key, val);

	if (tlvp_eeprom_check(tlvp, key, op == OP_SET ? val : NULL)) {
		lerror("Invalid EEPROM param '%s'", arg);
		free(key);
		return 1;
	}

	/* Find available param */
	pe = pl;
	while (pe) {
		if (!strcmp(pe->key, key))
			break;
		pe = pe->next;
	}

	if (pe) {
		free(pe->key);
		pe->key = key;
		pe->val = val;
	} else {
		pe = calloc(1, sizeof(*pe));
		pe->key = key;
		pe->val = val;
		pe->next = pl;
		pl = pe;
	}

	return 0;
}

int tlvstore_parse_config(char *arg)
{
	FILE *fp;
	char *fname;
	char line[256];
	int len, fail = 0;

	fname = *arg == '@' ? arg + 1 : arg;
	fp = fopen(fname, "r");
	if (!fp) {
		lerror("Invalid EEPROM config '%s'", arg);
		return 1;
	}

	ldebug("Opened EEPROM config file: %s", fname);

	while (fgets(line, sizeof(line) - 1, fp)) {
		/* Trim the line */
		len = strlen(line);
		while (line[len - 1] <= 32 || line[len - 1] >= 127)
			len--;
		line[len] = 0;
		fail += tlvstore_parse_line(line);
	}

	if (ferror(fp)) {
		lerror("Cannot read EEPROM config file: '%s'", fname);
		fail++;
	}

	fclose(fp);
	return fail;
}

int tlvstore_parse_params(int argc, char *argv[])
{
	int i, fail = 0;
	char *arg;

	ldebug("Parsing %d parameters", argc);
	for (int i = 0; i < argc; i++) {
		arg = argv[i];
		if (arg[0] == '@')
			fail += tlvstore_parse_config(arg);
		else
			fail += tlvstore_parse_line(arg);
	}

	return fail;
}

int tlvstore_export_params(void)
{
	struct params_list *pe = pl;
	int fail = 0;

	if (!pl) {
		ldebug("Exporting all TLV properties");
		fail = tlvp_eeprom_dump(tlvp, NULL);
	} else {
		ldebug("Starting parameters export");
	}

	while (pl) {
		if (pl->val && pl->val[0] == '@') {
			ldebug("Exporting '%s' to file '%s'", pl->key, pl->val+1);
			if (tlvp_eeprom_save(tlvp, pl->key, pl->val+1) < 0) {
				lerror("Failed to export '%s' to '%s'",
					pl->key, pl->val+1);
				fail++;
			}
		} else {
			if (tlvp_eeprom_dump(tlvp, pl->key) < 0) {
				lerror("Failed to dump '%s'", pl->key);
				fail++;
			}
		}
		pl = pl->next;
	}

	if (fail)
		ldebug("Failed TLV export, %i failures", fail);

	return fail;
}

int tlvstore_import_params(void)
{
	struct params_list *pe = pl;
	int fail = 0;

	ldebug("Starting parameters import");
	while (pl) {
		if (pl->val && pl->val[0] == '@') {
			ldebug("Importing '%s' from file '%s'", pl->key, pl->val+1);
			if (tlvp_eeprom_load(tlvp, pl->key, pl->val+1) < 0) {
				lerror("Failed to import '%s' from '%s'",
					pl->key, pl->val+1);
				fail++;
			}
		} else if (pl->val) {
			ldebug("Updating '%s' with value '%s'", pl->key, pl->val);
			if (tlvp_eeprom_update(tlvp, pl->key, pl->val) < 0) {
				lerror("Failed to update '%s'", pl->key);
				fail++;
			}
		}
		pl = pl->next;
	}

	if (fail)
		lerror("Failed TLV import, %i failures", fail);

	return fail;
}

void tlvstore_list_properties(void)
{
	ldebug("Listing TLV properties");
	tlvp_eeprom_list(tlvp);
}

static void tlvstore_usage(void)
{
	fprintf(stderr, "Usage: tlvstore [options] <key>[=@value>] ...\n"
			"  -F, --store-file <file-name>     Storage file path\n"
			"  -S, --store-size <file-size>     Preferred storage file size\n"
			"  -g, --get                        Get specified keys or all keys when no specified\n"
			"  -s, --set                        Set specified keys\n"
			"  -l, --list                       List available keys\n"
	       );
}

static struct option tlvstore_options[] =
{
	{ "store-size",   1, 0, 'S' },
	{ "store-file",   1, 0, 'F' },
	{ "get",          0, 0, 'g' },
	{ "set",          0, 0, 's' },
	{ "list",         0, 0, 'l' },
	{ 0, 0, 0, 0 }
};

int main(int argc, char *argv[])
{
	int opt, index;
	int ret = 1;
	char *store_file = TLVS_DEFAULT_FILE;
	int store_size = TLVS_DEFAULT_SIZE;

	while ((opt = getopt_long(argc, argv, "F:S:hgsl", tlvstore_options, &index)) != -1) {
		switch (opt) {
		case 'F':
			store_file = strdup(optarg);
			break;
		case 'S':
			store_size = atoi(optarg);
			break;
		case 'g':
			op = OP_GET;
			break;
		case 's':
			op = OP_SET;
			break;
		case 'l':
			op = OP_LIST;
			break;
		case 'h':
		default:
			tlvstore_usage();
			exit(EXIT_FAILURE);
		}
	}

	if (!store_file) {
		fprintf(stderr, "Storage file not specified\n");
		tlvstore_usage();
		exit(EXIT_FAILURE);
	}

	tlvd = tlvd_open(store_file, store_size);
	if (!tlvd) {
		fprintf(stderr, "Failed to initialize '%s' storage file\n", store_file);
		exit(EXIT_FAILURE);
	}

	ldebug("Opened storage memory file %s at %p, size: %zi", store_file, tlvd->base, tlvd->size);

	tlvp = tlvp_init(tlvd);
	if (!tlvp) {
		fprintf(stderr, "Unknown storage protocol for '%s'\n", store_file);
		tlvd_close(tlvd);
		exit(EXIT_FAILURE);
	}

	ldebug("Initialized storage protocol '%s'", tlvp->name);

	if (tlvstore_parse_params(argc - optind, &argv[optind])) {
		tlvstore_usage();
		tlvp_free(tlvp);
		tlvd_close(tlvd);
		exit(EXIT_FAILURE);
	}

	if (op == OP_LIST) {
		tlvstore_list_properties();
		ret = 0;
	} else if (op == OP_GET) {
		if (!tlvstore_export_params())
			ret = 0;
	} else if (op == OP_SET) {
		if (!tlvstore_import_params())
			ret = 0;
	} else {
		linfo("No operation specified");
	}

	tlvp_free(tlvp);

	tlvd_close(tlvd);

	return ret;
}
