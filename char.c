
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "log.h"
#include "char.h"

void tlvd_close(struct tlv_device *tlvd)
{
	fsync(tlvd->fd);
	if (munmap(tlvd->base, tlvd->size))
		perror("munmap() failed");
	close(tlvd->fd);
}

struct tlv_device *tlvd_open(const char *file_name, int pref_size)
{
	struct tlv_device *tlvd;
	struct stat fs;
	int fd = -1;
	int file_init, file_size, last_size;

	ldebug("Opening storage memory file %s, preferred size: %d", file_name, pref_size);

	tlvd = malloc(sizeof(*tlvd));
	if (!tlvd) {
		perror("malloc() failed");
		return NULL;
	}

	file_init = access(file_name, F_OK);

	fd = open(file_name, O_CREAT|O_SYNC|O_RDWR, 0644);
	if (fd == -1) {
		perror("open() failed");
		goto fail;
	}

	if (fstat(fd, &fs)) {
		perror("fstat() failed");
		goto fail;
	}

	file_size = fs.st_size;
	if (pref_size) {
		if (ftruncate(fd, pref_size))
			perror("ftruncate() failed");
		else
			file_size = pref_size;
	}

	if (!file_size) {
		fprintf(stderr, "Invalid storage size\n");
		goto fail;
	}

	tlvd->fd = fd;
	tlvd->size = file_size;
	tlvd->base = mmap(NULL, file_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (tlvd->base == MAP_FAILED) {
		perror("mmap() failed");
		goto fail;
	}

	last_size = fs.st_size;
	while (last_size < pref_size)
		((char *)tlvd->base)[last_size++] = 0xFF;

	return tlvd;
fail:
	if (tlvd)
		free(tlvd);
	if (fd != -1)
		close(fd);
	if (file_init)
		unlink(file_name);
	return NULL;
}
