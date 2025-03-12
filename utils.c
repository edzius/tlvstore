#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

void *afread(const char *file_name, size_t *file_size)
{
	struct stat st;
	FILE *fp;
	void *fdata;
	ssize_t fsize;

	fp = fopen(file_name, "rb");
	if (!fp) {
		perror("fopen() failed");
		return NULL;
	}

	if (fstat(fileno(fp), &st) != 0) {
		perror("fstat() failed");
		fclose(fp);
		return NULL;
	}

	fsize = st.st_size;
	fdata = malloc(fsize);
	if (!fdata) {
		perror("malloc() failed");
		fclose(fp);
		return NULL;
	}

	if (fread(fdata, 1, fsize, fp) != fsize) {
		perror("fread() failed");
		fclose(fp);
		free(fdata);
		return NULL;
	}

	fclose(fp);

	*file_size = fsize;
	return fdata;
}
