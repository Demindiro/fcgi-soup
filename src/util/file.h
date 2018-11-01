#ifndef UTIL_FILE_H
#define UTIL_FILE_H

static char *file_read(int fd, size_t size)
{
	char *buf = malloc(size);
	if (buf == NULL)
		return NULL;
	int total = 0, left = size;
	while (total < size) {
		int r = read(fd, buf, left);
		if (r < 0) {
			free(buf);
			return NULL;
		}
		total += r;
		left  -= r;
	}
	return buf;
}

#endif
