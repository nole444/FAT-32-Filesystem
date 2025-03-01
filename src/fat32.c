// src/fat32.c
#include "fat32.h"
#include "filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Function to read the boot sector
int read_boot_sector(FILE *fp, fat32_boot_sector *boot) {
    fseek(fp, 0, SEEK_SET);
    size_t read = fread(boot, sizeof(fat32_boot_sector), 1, fp);
    if (read != 1) {
        return -1;
    }
    return 0;
}

// Function to calculate the first data sector
uint32_t first_data_sector(fat32_boot_sector *boot) {
    return boot->reserved_sector_count + (boot->num_fats * boot->fat_size_32);
}

// Function to calculate the first sector of a given cluster
uint32_t first_sector_of_cluster(fat32_boot_sector *boot, uint32_t cluster) {
    return ((cluster - 2) * boot->sectors_per_cluster) + first_data_sector(boot);
}

// Function to read a directory entry
int read_dir_entry(FILE *fp, fat32_boot_sector *boot, uint32_t cluster, fat32_dir_entry *entry, int index) {
    uint32_t first_sector = first_sector_of_cluster(boot, cluster);
    uint32_t offset = (first_sector * boot->bytes_per_sector) + (index * sizeof(fat32_dir_entry));
    fseek(fp, offset, SEEK_SET);
    size_t read_bytes = fread(entry, sizeof(fat32_dir_entry), 1, fp);
    if (read_bytes != 1) {
        return -1;
    }
    return 0;
}

// Function to convert filename to FAT32 8.3 format
void convert_filename(const char *filename, unsigned char *fat32_name) {
    memset(fat32_name, ' ', 11); // Initialize with spaces
    size_t i = 0, j = 0;
    const char *dot = strchr(filename, '.');

    // If there's no dot, set dot to the end of the string
    if (dot == NULL) {
        dot = filename + strlen(filename);
    }

    // Copy name part (up to 8 characters)
    while (filename + i < dot && i < 8) {
        fat32_name[i] = toupper((unsigned char)filename[i]);
        i++;
    }

    // Copy extension part (up to 3 characters)
    if (*dot == '.') {
        i = 8; // Start of extension in FAT32 name
        dot++; // Move past the dot
        while (*dot != '\0' && j < 3) {
            fat32_name[i + j] = toupper((unsigned char)dot[j]);
            j++;
        }
    }
}

// Function to find a directory entry by filename
int find_dir_entry(filesystem *fs, const char *filename, fat32_dir_entry *entry, uint32_t *cluster_num, int *index) {
    if (filename == NULL) {
        return 0; // Cannot search for NULL filename
    }

    unsigned char fat32_name[11];
    convert_filename(filename, fat32_name);

    fat32_dir_entry current_entry;
    int current_index = 0;
    uint32_t cluster = fs->current_dir_cluster;

    while (1) {
        if (read_dir_entry(fs->fp, &fs->boot, cluster, &current_entry, current_index) != 0) {
            // Reached end of directory or read error
            break;
        }

        // Check if entry is unused
        if (current_entry.name[0] == 0x00) {
            break;
        }

        // Check if entry is a valid file/directory
        if (current_entry.name[0] != 0xE5 && (current_entry.attr & 0x0F) != 0x0F) {
            // Compare names
            if (memcmp(current_entry.name, fat32_name, 11) == 0) {
                // Found the entry
                if (entry != NULL) {
                    memcpy(entry, &current_entry, sizeof(fat32_dir_entry));
                }
                if (cluster_num != NULL) {
                    *cluster_num = cluster;
                }
                if (index != NULL) {
                    *index = current_index;
                }
                return 1; // Found
            }
        }

        current_index++;
        // Assuming a fixed number of entries per cluster
        if (current_index >= (fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster) / sizeof(fat32_dir_entry)) {
            // Move to next cluster in the chain
            cluster = fs->fat_table[cluster];
            if (cluster >= 0x0FFFFFF8) { // End of cluster chain
                break;
            }
            current_index = 0;
        }
    }

    return 0; // Not found
}

// Function to write a directory entry
int write_dir_entry(FILE *fp, fat32_boot_sector *boot, uint32_t cluster, int index, fat32_dir_entry *entry) {
    uint32_t first_sector = first_sector_of_cluster(boot, cluster);
    uint32_t offset = (first_sector * boot->bytes_per_sector) + (index * sizeof(fat32_dir_entry));
    fseek(fp, offset, SEEK_SET);
    size_t written = fwrite(entry, sizeof(fat32_dir_entry), 1, fp);
    if (written != 1) {
        return -1; // Write error
    }
    fflush(fp); // Ensure data is written to disk
    return 0; // Success
}

// Function to allocate a new cluster within the filesystem
int allocate_cluster_filesystem(filesystem *fs, uint32_t *cluster_num) {
    fat32_boot_sector *boot = &fs->boot;

    for (uint32_t i = 2; i < (boot->total_sectors_32 / boot->sectors_per_cluster); i++) {
        if (boot->fat_size_32 == 0) {
            fprintf(stderr, "Error: FAT size is zero.\n");
            return -1;
        }

        if (i >= boot->fat_size_32) {
            fprintf(stderr, "Error: Cluster number exceeds FAT size.\n");
            return -1;
        }

        // Check for free cluster
        if (fs->fat_table[i] == 0x00000000) { // Free cluster
            *cluster_num = i;

            // Find the end of the current cluster chain
            uint32_t current = fs->current_dir_cluster;
            while (fs->fat_table[current] < 0x0FFFFFF8) {
                current = fs->fat_table[current];
            }

            // Update FAT to link the new cluster
            fs->fat_table[current] = *cluster_num;
            fs->fat_table[*cluster_num] = 0x0FFFFFFF; // End of chain

            // Write updated FAT entries to the image
            fseek(fs->fp, boot->reserved_sector_count * boot->bytes_per_sector + current * sizeof(uint32_t), SEEK_SET);
            fwrite(&fs->fat_table[current], sizeof(uint32_t), 1, fs->fp);
            fseek(fs->fp, boot->reserved_sector_count * boot->bytes_per_sector + (*cluster_num) * sizeof(uint32_t), SEEK_SET);
            fwrite(&fs->fat_table[*cluster_num], sizeof(uint32_t), 1, fs->fp);
            fflush(fs->fp);

            return 0; // Success
        }
    }

    // No free clusters found
    return -1;
}

// Function to extend a file by allocating new clusters
int extend_file(filesystem *fs, fat32_dir_entry *entry, uint32_t *last_cluster, uint32_t new_size) {
    // Calculate the number of clusters needed
    uint32_t clusters_needed = (new_size + fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster - 1) /
                               (fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster);

    uint32_t current_clusters = (entry->file_size + fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster - 1) /
                                (fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster);

    if (clusters_needed <= current_clusters) {
        // No need to extend
        return 0;
    }

    // Allocate new clusters
    for (uint32_t i = current_clusters; i < clusters_needed; i++) {
        uint32_t new_cluster;
        if (allocate_cluster_filesystem(fs, &new_cluster) != 0) {
            fprintf(stderr, "Error: Unable to allocate more clusters.\n");
            return -1;
        }

        // Update FAT
        fs->fat_table[*last_cluster] = new_cluster;
        *last_cluster = new_cluster;
    }

    // Mark the last cluster as end of chain
    fs->fat_table[*last_cluster] = 0x0FFFFFFF;

    // Update the FAT table in the image
    fseek(fs->fp, fs->boot.reserved_sector_count * fs->boot.bytes_per_sector, SEEK_SET);
    fwrite(fs->fat_table, sizeof(uint32_t), fs->boot.fat_size_32, fs->fp);
    fflush(fs->fp);

    return 0;
}

