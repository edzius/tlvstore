#ifndef __FIRMUX_STRUCT_H
#define __FIRMUX_STRUCT_H

struct __attribute__ ((__packed__)) firmux_header {
	char magic[8];
	uint32_t crc;
};

struct __attribute__ ((__packed__)) firmux_fields {
	char product_id[16];
	char product_name[16];
	char serial_no[16];
	char pcb_name[8];
	char pcb_revision[4];
	char pcb_prdate[4];
	char pcb_prlocation[16];
	char pcb_serial[16];
	char mac_addr[6];
	/* char mac_addr[128]; /* multiple MAC/interface pairs */
};

#endif /* __FIRMUX_STRUCT_H */
