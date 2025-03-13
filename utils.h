#ifndef __UTILS_H
#define __UTILS_H

void *afread(const char *file_name, size_t *file_size);
ssize_t afwrite(const char *file_name, void *data, size_t size);

#endif /* __UTILS_H */
