// include/filesystem.h
#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "fat32.h"

#define MAX_OPEN_FILES 10
#define MAX_FILENAME_LENGTH 256

typedef struct open_file {
    char filename[12]; // 11 chars + null terminator
    char mode[3];      // e.g., "r", "w", "rw"
    uint32_t offset;
    char path[256];    // Path within the image
} open_file;

// Structure representing the filesystem
typedef struct filesystem {
    FILE *fp;
    fat32_boot_sector boot;
    uint32_t current_dir_cluster;
    char current_path[256];
    open_file open_files[MAX_OPEN_FILES];
    int open_file_count;
    char image_name[256];
    uint32_t *fat_table; // Pointer to FAT table
} filesystem;

// Function Declarations Related to Filesystem
void convert_filename(const char *filename, unsigned char *fat32_name);
int extend_file(filesystem *fs, fat32_dir_entry *entry, uint32_t *last_cluster, uint32_t new_size);
int allocate_cluster_filesystem(filesystem *fs, uint32_t *cluster_num);
int find_dir_entry(filesystem *fs, const char *filename, fat32_dir_entry *entry, uint32_t *cluster_num, int *index);
int write_dir_entry(FILE *fp, fat32_boot_sector *boot, uint32_t cluster, int index, fat32_dir_entry *entry);

// Add other filesystem-related function declarations as needed

#endif // FILESYSTEM_H

