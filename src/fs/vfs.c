#include <stddef.h>
#include "drivers/fat/fat.h"
#include "drivers/ext/ext.h"

// Try to read a file from any supported filesystem
int vfs_read_file(const char* path, void** buf, size_t* len) {
	if (!path || !buf || !len) return -1;
	// Try FAT first
	if (fat_read_file(path, buf, len) == 0)
		return 0;
	// Try EXT next
	if (ext_read_file(path, buf, len) == 0)
		return 0;
	// Not found
	*buf = NULL;
	*len = 0;
	return -1;
}
