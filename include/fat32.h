// include/fat32.h
#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stdio.h>

// FAT32 Boot Sector Structure
typedef struct {
    uint8_t  jump_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t  num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t  media;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t bk_boot_sec;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  fs_type[8];
} __attribute__((packed)) fat32_boot_sector;

// Directory Entry Structure
typedef struct fat32_dir_entry {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  nt_reserved;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} __attribute__((packed)) fat32_dir_entry;

// Forward Declaration of filesystem structure
struct filesystem;

// Function Declarations
void convert_filename(const char *filename, unsigned char *fat32_name);
int extend_file(struct filesystem *fs, struct fat32_dir_entry *entry, uint32_t *last_cluster, uint32_t new_size);
int allocate_cluster_filesystem(struct filesystem *fs, uint32_t *cluster_num);
int find_dir_entry(struct filesystem *fs, const char *filename, struct fat32_dir_entry *entry, uint32_t *cluster_num, int *index);
int write_dir_entry(FILE *fp, fat32_boot_sector *boot, uint32_t cluster, int index, struct fat32_dir_entry *entry);
uint32_t first_sector_of_cluster(fat32_boot_sector *boot, uint32_t cluster);
int read_dir_entry(FILE *fp, fat32_boot_sector *boot, uint32_t cluster, fat32_dir_entry *entry, int index);
int read_boot_sector(FILE *fp, fat32_boot_sector *boot); // Added declaration

#endif // FAT32_H

