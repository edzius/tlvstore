#ifndef __FIRMUX_FIELDS_H
#define __FIRMUX_FIELDS_H

#include "datamodel-firmux-struct.h"

struct firmux_property {
	const char *fp_name;
	off_t fp_offset;
	size_t fp_size;
	ssize_t (*fp_parse)(void **data_out, void *data_in, size_t size_in);
	ssize_t (*fp_format)(void **data_out, void *data_in, size_t size_in);
};

#endif /* __FIRMUX_FIELDS_H */
