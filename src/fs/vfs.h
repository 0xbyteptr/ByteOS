#pragma once

// VFS: read a file from any supported filesystem
int vfs_read_file(const char* path, void** buf, size_t* len);

