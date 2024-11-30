#ifndef __TLV_DEVICE_H
#define __TLV_DEVICE_H

struct tlv_device {
	int fd;
	void *base;
	size_t size;
};

struct tlv_device *tlvd_open(const char *file_name, int pref_size);
void tlvd_close(struct tlv_device *tlvd);

#endif /* __TLV_DEVICE_H */
