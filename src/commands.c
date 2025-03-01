// src/commands.c
#include "commands.h"
#include "filesystem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>  // For strcasecmp
#include <ctype.h>



char *strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *dup = malloc(len);
    if (dup) {
        memcpy(dup, s, len);
    }
    return dup;
}

// Function to check if a file is open
int is_file_open(filesystem *fs, const char *filename) {
    for (int i = 0; i < fs->open_file_count; i++) {
        if (strcasecmp(fs->open_files[i].filename, filename) == 0) {
            return 1; // File is open
        }
    }
    return 0; // File is not open
}

// Helper function to find an open file index
int find_open_file(filesystem *fs, const char *filename) {
    for (int i = 0; i < fs->open_file_count; i++) {
        if (strcasecmp(fs->open_files[i].filename, filename) == 0) {
            return i;
        }
    }
    return -1; // Not found
}

// Helper function to add an open file
int add_open_file(filesystem *fs, const char *filename, const char *mode) {
    if (fs->open_file_count >= MAX_OPEN_FILES) {
        fprintf(stderr, "Error: Maximum number of open files (%d) reached.\n", MAX_OPEN_FILES);
        return -1;
    }
    strncpy(fs->open_files[fs->open_file_count].filename, filename, 11);
    fs->open_files[fs->open_file_count].filename[11] = '\0';
    strncpy(fs->open_files[fs->open_file_count].mode, mode, 2);
    fs->open_files[fs->open_file_count].mode[2] = '\0';
    fs->open_files[fs->open_file_count].offset = 0;
    strncpy(fs->open_files[fs->open_file_count].path, fs->current_path, 255);
    fs->open_files[fs->open_file_count].path[255] = '\0';
    fs->open_file_count++;
    return 0;
}

// Helper function to remove an open file
int remove_open_file(filesystem *fs, const char *filename) {
    int index = find_open_file(fs, filename);
    if (index == -1) {
        return -1; // Not found
    }
    for (int i = index; i < fs->open_file_count - 1; i++) {
        fs->open_files[i] = fs->open_files[i + 1];
    }
    fs->open_file_count--;
    return 0;
}

// Function to handle the 'open' command
void cmd_open(filesystem *fs, char *filename, char *flags) {
    // Validate flags
    if (strcmp(flags, "-r") != 0 && strcmp(flags, "-w") != 0 &&
        strcmp(flags, "-rw") != 0 && strcmp(flags, "-wr") != 0) {
        fprintf(stderr, "Error: Invalid flags '%s'.\n", flags);
        return;
    }

    // Check if file is already open
    for (int i = 0; i < fs->open_file_count; i++) {
        if (strcmp(fs->open_files[i].filename, filename) == 0) {
            fprintf(stderr, "Error: File '%s' is already open.\n", filename);
            return;
        }
    }

    // Locate the file in the directory
    fat32_dir_entry entry;
    if (!find_dir_entry(fs, filename, &entry, NULL, NULL)) {
        fprintf(stderr, "Error: File '%s' does not exist.\n", filename);
        return;
    }

    // Add the file to the open files list
    if (fs->open_file_count >= 10) { // Max 10 open files
        fprintf(stderr, "Error: Too many open files.\n");
        return;
    }

    open_file *file = &fs->open_files[fs->open_file_count++];
    strncpy(file->filename, filename, sizeof(file->filename) - 1);
    file->filename[sizeof(file->filename) - 1] = '\0'; // Ensure null-termination
    strncpy(file->mode, flags + 1, sizeof(file->mode) - 1); // Copy mode after '-'
    file->mode[sizeof(file->mode) - 1] = '\0';             // Ensure null-termination
    file->offset = 0;

    printf("File '%s' opened with flags '%s'.\n", filename, flags);
}



// Function to handle the 'close' command
void cmd_close(filesystem *fs, char *filename) {
    if (!filename) {
        fprintf(stderr, "Usage: close <filename>\n");
        return;
    }

    // Check if file is open
    if (!is_file_open(fs, filename)) {
        fprintf(stderr, "Error: File '%s' is not open.\n", filename);
        return;
    }

    // Remove from open files
    if (remove_open_file(fs, filename) != 0) {
        fprintf(stderr, "Error: Failed to close file '%s'.\n", filename);
        return;
    }

    printf("File '%s' closed successfully.\n", filename);
}

// Function to handle the 'lsof' command
void cmd_lsof(filesystem *fs) {
    if (fs->open_file_count == 0) {
        printf("No files are currently opened.\n");
        return;
    }

    printf("Open Files:\n");
    printf("Index\tFilename\tMode\tOffset\tPath\n");
    for (int i = 0; i < fs->open_file_count; i++) {
        printf("%d\t%s\t%s\t%u\t%s\n",
               i + 1,
               fs->open_files[i].filename,
               fs->open_files[i].mode,
               fs->open_files[i].offset,
               fs->open_files[i].path);
    }
}

// Function to handle the 'size' command
void cmd_size(filesystem *fs, char *filename) {
    if (!filename) {
        fprintf(stderr, "Usage: size <filename>\n");
        return;
    }

    fat32_dir_entry entry;
    if (!find_dir_entry(fs, filename, &entry, NULL, NULL)) {
        fprintf(stderr, "Error: File '%s' does not exist.\n", filename);
        return;
    }

    if (entry.attr & 0x10) { // Directory attribute
        fprintf(stderr, "Error: '%s' is a directory.\n", filename);
        return;
    }

    printf("Size of '%s': %u bytes\n", filename, entry.file_size);
}

// Function to handle the 'lseek' command
void cmd_lseek(filesystem *fs, char *filename, char *offset_str) {
    if (!filename || !offset_str) {
        fprintf(stderr, "Usage: lseek <filename> <offset>\n");
        return;
    }

    // Convert offset to integer
    char *endptr;
    uint32_t offset = strtoul(offset_str, &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Error: Invalid offset '%s'.\n", offset_str);
        return;
    }

    // Check if file is open
    int open_index = find_open_file(fs, filename);
    if (open_index == -1) {
        fprintf(stderr, "Error: File '%s' is not open.\n", filename);
        return;
    }

    // Get file size
    fat32_dir_entry entry;
    if (!find_dir_entry(fs, filename, &entry, NULL, NULL)) {
        fprintf(stderr, "Error: File '%s' does not exist.\n", filename);
        return;
    }

    if (offset > entry.file_size) {
        fprintf(stderr, "Error: Offset %u is larger than the size of the file (%u bytes).\n", offset, entry.file_size);
        return;
    }

    // Update offset
    fs->open_files[open_index].offset = offset;
    printf("Offset of '%s' set to %u.\n", filename, offset);
}

// Function to handle the 'read' command
// Function to handle the 'read' command
/*void cmd_read(filesystem *fs, char *filename, char *size_str) {
    if (!filename || !size_str) {
        fprintf(stderr, "Usage: read <filename> <size>\n");
        return;
    }

    // Convert size to integer
    char *endptr;
    uint32_t size = strtoul(size_str, &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Error: Invalid size '%s'.\n", size_str);
        return;
    }

    // Check if file is open
    int open_index = find_open_file(fs, filename);
    if (open_index == -1) {
        fprintf(stderr, "Error: File '%s' is not open.\n", filename);
        return;
    }

    // Check if file is opened for reading
    char mode = fs->open_files[open_index].mode[0];
    if (mode != 'r' && mode != 'R') {
        fprintf(stderr, "Error: File '%s' is not opened for reading.\n", filename);
        return;
    }

    // Get file size
    fat32_dir_entry entry;
    if (!find_dir_entry(fs, filename, &entry, NULL, NULL)) {
        fprintf(stderr, "Error: File '%s' does not exist.\n", filename);
        return;
    }

    if (entry.attr & 0x10) { // Directory attribute
        fprintf(stderr, "Error: '%s' is a directory.\n", filename);
        return;
    }

    uint32_t current_offset = fs->open_files[open_index].offset;
    if (current_offset >= entry.file_size) {
        printf("Reached end of file '%s'.\n", filename);
        return;
    }

    // Calculate number of bytes to read
    uint32_t bytes_to_read = size;
    if (current_offset + bytes_to_read > entry.file_size) {
        bytes_to_read = entry.file_size - current_offset;
    }

    // Read data
    char *buffer = malloc(bytes_to_read + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    // Calculate starting cluster and sector
    uint32_t cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;
    uint32_t bytes_per_sector = fs->boot.bytes_per_sector;
    uint32_t sectors_per_cluster = fs->boot.sectors_per_cluster;
    uint32_t first_sector = first_sector_of_cluster(&fs->boot, cluster);
    uint32_t data_offset = first_sector * bytes_per_sector + current_offset;

    fseek(fs->fp, data_offset, SEEK_SET);
    size_t read_bytes = fread(buffer, 1, bytes_to_read, fs->fp);
    if (read_bytes != bytes_to_read) {
        fprintf(stderr, "Error: Failed to read from file '%s'.\n", filename);
        free(buffer);
        return;
    }
    buffer[read_bytes] = '\0'; // Null-terminate for safe printing

    printf("Read %zu bytes from '%s':\n", read_bytes, filename);
    printf("%s\n", buffer);

    // Update offset
    fs->open_files[open_index].offset += read_bytes;

    free(buffer);
}*/

void cmd_read(filesystem *fs, char *filename, char *size_str) {
    if (!filename || !size_str) {
        fprintf(stderr, "Usage: read <filename> <size>\n");
        return;
    }

    // Convert size to integer
    char *endptr;
    uint32_t size = strtoul(size_str, &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Error: Invalid size '%s'.\n", size_str);
        return;
    }

    // Check if the file is open
    int open_index = find_open_file(fs, filename);
    if (open_index == -1) {
        fprintf(stderr, "Error: File '%s' is not open.\n", filename);
        return;
    }

    // Check if the file is opened for reading
    if (!strchr(fs->open_files[open_index].mode, 'r') &&
        !strchr(fs->open_files[open_index].mode, 'w')) {
        fprintf(stderr, "Error: File '%s' is not opened for reading.\n", filename);
        return;
    }

    // Get the directory entry
    fat32_dir_entry entry;
    if (!find_dir_entry(fs, filename, &entry, NULL, NULL)) {
        fprintf(stderr, "Error: File '%s' does not exist.\n", filename);
        return;
    }

    if (entry.attr & 0x10) { // Check if it's a directory
        fprintf(stderr, "Error: '%s' is a directory.\n", filename);
        return;
    }

    uint32_t current_offset = fs->open_files[open_index].offset;

    // Ensure the offset is within file size bounds
    if (current_offset >= entry.file_size) {
        printf("Reached end of file '%s'.\n", filename);
        return;
    }

    // Calculate bytes to read
    uint32_t bytes_to_read = size;
    if (current_offset + bytes_to_read > entry.file_size) {
        bytes_to_read = entry.file_size - current_offset; // Adjust to file size
    }

    char *buffer = malloc(bytes_to_read + 1);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        return;
    }

    uint32_t cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;
    uint32_t bytes_per_cluster = fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster;
    uint32_t cluster_offset = current_offset % bytes_per_cluster;
    uint32_t first_sector;

    // Read the requested bytes
    uint32_t bytes_read = 0;
    while (bytes_to_read > 0) {
        // Calculate the first sector of the cluster
        first_sector = first_sector_of_cluster(&fs->boot, cluster);
        uint32_t data_offset = (first_sector * fs->boot.bytes_per_sector) + cluster_offset;

        // Determine how many bytes to read from the current cluster
        uint32_t bytes_in_cluster = bytes_per_cluster - cluster_offset;
        uint32_t bytes_this_read = (bytes_to_read < bytes_in_cluster) ? bytes_to_read : bytes_in_cluster;

        // Read data
        fseek(fs->fp, data_offset, SEEK_SET);
        size_t actual_read = fread(buffer + bytes_read, 1, bytes_this_read, fs->fp);

        if (actual_read != bytes_this_read) {
            fprintf(stderr, "Error: Failed to read from file '%s'.\n", filename);
            free(buffer);
            return;
        }

        // Update counters and offsets
        bytes_read += actual_read;
        bytes_to_read -= actual_read;
        cluster_offset = 0; // Reset cluster offset after the first cluster

        // Move to the next cluster if needed
        if (bytes_to_read > 0) {
            cluster = fs->fat_table[cluster];
            if (cluster >= 0x0FFFFFF8) {
                break; // End of cluster chain
            }
        }
    }

    // Null-terminate the buffer for safe printing
    buffer[bytes_read] = '\0';

    // Print the read data
    printf("Read %u bytes from '%s':\n%s\n", bytes_read, filename, buffer);

    // Update the file's offset
    fs->open_files[open_index].offset += bytes_read;

    free(buffer);
}



// Function to handle the 'write' command
// Corrected Function to handle the 'write' command
void cmd_write(filesystem *fs, char *filename, char *content) {
    if (!filename || !content) {
        fprintf(stderr, "Usage: write <filename> \"<string>\"\n");
        return;
    }

    // Check if the file is open for writing
    int open_index = find_open_file(fs, filename);
    if (open_index == -1) {
        fprintf(stderr, "Error: File '%s' is not open.\n", filename);
        return;
    }
    if (!strchr(fs->open_files[open_index].mode, 'w')) {
        fprintf(stderr, "Error: File '%s' is not opened for writing.\n", filename);
        return;
    }

    fat32_dir_entry entry;
    uint32_t dir_cluster_num;
    int dir_entry_index;

    // Find the directory entry
    if (!find_dir_entry(fs, filename, &entry, &dir_cluster_num, &dir_entry_index)) {
        fprintf(stderr, "Error: File '%s' does not exist.\n", filename);
        return;
    }
    if (entry.attr & 0x10) { // Check if it's a directory
        fprintf(stderr, "Error: '%s' is a directory.\n", filename);
        return;
    }

    uint32_t current_offset = fs->open_files[open_index].offset;
    uint32_t string_length = strlen(content);
    uint32_t new_offset = current_offset + string_length;

    // Get the starting cluster of the file
    uint32_t start_cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;

    // If the file is empty (no starting cluster), allocate the first cluster
    if (start_cluster == 0) {
        if (allocate_cluster_filesystem(fs, &start_cluster) != 0) {
            fprintf(stderr, "Error: Unable to allocate cluster for '%s'.\n", filename);
            return;
        }

        // Update the directory entry with the new starting cluster
        entry.first_cluster_low = start_cluster & 0xFFFF;
        entry.first_cluster_high = (start_cluster >> 16) & 0xFFFF;

        // Write the updated directory entry back to disk
        if (write_dir_entry(fs->fp, &fs->boot, dir_cluster_num, dir_entry_index, &entry) != 0) {
            fprintf(stderr, "Error: Failed to update directory entry for '%s'.\n", filename);
            return;
        }
    }

    // Extend the file if necessary
    uint32_t last_cluster = start_cluster;
    if (new_offset > entry.file_size) {
        if (extend_file(fs, &entry, &last_cluster, new_offset) != 0) {
            fprintf(stderr, "Error: Unable to extend file '%s'.\n", filename);
            return;
        }
        entry.file_size = new_offset;

        // Write the updated directory entry back to disk
        if (write_dir_entry(fs->fp, &fs->boot, dir_cluster_num, dir_entry_index, &entry) != 0) {
            fprintf(stderr, "Error: Failed to update directory entry for '%s'.\n", filename);
            return;
        }
    }

    // Prepare to write data
    uint32_t cluster_size = fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster;
    uint32_t cluster = start_cluster;
    uint32_t offset_in_cluster = current_offset % cluster_size;
    const char *string_ptr = content;
    uint32_t bytes_remaining = string_length;
    uint32_t bytes_written = 0;

    // Traverse to the correct cluster based on the current offset
    uint32_t offset_clusters = current_offset / cluster_size;
    for (uint32_t i = 0; i < offset_clusters; i++) {
        cluster = fs->fat_table[cluster];
        if (cluster >= 0x0FFFFFF8) {
            fprintf(stderr, "Error: Reached end of cluster chain unexpectedly in '%s'.\n", filename);
            return;
        }
    }

    // Write the data
    while (bytes_remaining > 0) {
        uint32_t first_sector = first_sector_of_cluster(&fs->boot, cluster);
        uint32_t data_offset = first_sector * fs->boot.bytes_per_sector + offset_in_cluster;

        uint32_t bytes_in_cluster = cluster_size - offset_in_cluster;
        uint32_t bytes_to_write = (bytes_remaining < bytes_in_cluster) ? bytes_remaining : bytes_in_cluster;

        // Write to the file
        fseek(fs->fp, data_offset, SEEK_SET);
        size_t written = fwrite(string_ptr, 1, bytes_to_write, fs->fp);
        if (written != bytes_to_write) {
            fprintf(stderr, "Error: Failed to write to file '%s'.\n", filename);
            return;
        }
        fflush(fs->fp);

        string_ptr += written;
        bytes_remaining -= written;
        bytes_written += written;
        offset_in_cluster = 0; // Start at the beginning of the next cluster

        // Move to the next cluster if necessary
        if (bytes_remaining > 0) {
            if (fs->fat_table[cluster] >= 0x0FFFFFF8) {
                // Allocate a new cluster
                uint32_t new_cluster;
                if (allocate_cluster_filesystem(fs, &new_cluster) != 0) {
                    fprintf(stderr, "Error: Unable to allocate new cluster while writing to '%s'.\n", filename);
                    return;
                }
                fs->fat_table[cluster] = new_cluster;
                fs->fat_table[new_cluster] = 0x0FFFFFFF; // Mark as end of chain
                cluster = new_cluster;

                // Write the updated FAT entries to the disk
                fseek(fs->fp, fs->boot.reserved_sector_count * fs->boot.bytes_per_sector, SEEK_SET);
                fwrite(fs->fat_table, sizeof(uint32_t), fs->boot.fat_size_32 * fs->boot.bytes_per_sector / sizeof(uint32_t), fs->fp);
                fflush(fs->fp);
            } else {
                cluster = fs->fat_table[cluster];
            }
        }
    }

    // Update the file offset
    fs->open_files[open_index].offset = new_offset;

    printf("Wrote %u bytes to '%s'.\n", bytes_written, filename);
}



/*void cmd_write(filesystem *fs, char *filename, char *content) {
     if (!filename || !content) {
        fprintf(stderr, "Usage: write <filename> \"<string>\"\n");
        return;
    }
    // Check if file is open
    int open_index = find_open_file(fs, filename);
    if (open_index == -1) {
        fprintf(stderr, "Error: File '%s' is not open.\n", filename);
        return;
    }

    // Check if file is opened for writing
    if (!strchr(fs->open_files[open_index].mode, 'w')) {
        fprintf(stderr, "Error: File '%s' is not opened for writing.\n", filename);
        return;
    }

    // Get file entry
    fat32_dir_entry entry;
    uint32_t cluster_num;
    int dir_index;

    if (!find_dir_entry(fs, filename, &entry, &cluster_num, &dir_index)) {
        fprintf(stderr, "Error: File '%s' does not exist.\n", filename);
        return;
    }

    if (entry.attr & 0x10) { // Directory attribute
        fprintf(stderr, "Error: '%s' is a directory.\n", filename);
        return;
    }

    uint32_t current_offset = fs->open_files[open_index].offset;
    uint32_t string_length = strlen(content);
    uint32_t new_offset = current_offset + string_length;

    // Check if we need to extend the file
    if (new_offset > entry.file_size) {
        uint32_t last_cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;
        if (extend_file(fs, &entry, &last_cluster, new_offset) != 0) {
            fprintf(stderr, "Error: Unable to extend file '%s'.\n", filename);
            return;
        }

        entry.file_size = new_offset;
        entry.first_cluster_low = last_cluster & 0xFFFF;
        entry.first_cluster_high = (last_cluster >> 16) & 0xFFFF;

        // Update directory entry
        if (write_dir_entry(fs->fp, &fs->boot, cluster_num, dir_index, &entry) != 0) {
            fprintf(stderr, "Error: Failed to update directory entry for '%s'.\n", filename);
            return;
        }
    }

    // Write the string to the file
    uint32_t cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;
    uint32_t bytes_remaining = string_length;
    const char *string_ptr = content;

    while (bytes_remaining > 0) {
        uint32_t cluster_size = fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster;
        uint32_t cluster_offset = current_offset % cluster_size;
        uint32_t bytes_in_cluster = cluster_size - cluster_offset;
        uint32_t bytes_to_write = (bytes_remaining < bytes_in_cluster) ? bytes_remaining : bytes_in_cluster;

        uint32_t first_sector = first_sector_of_cluster(&fs->boot, cluster);
        uint32_t data_offset = first_sector * fs->boot.bytes_per_sector + cluster_offset;

        fseek(fs->fp, data_offset, SEEK_SET);
        size_t written = fwrite(string_ptr, 1, bytes_to_write, fs->fp);

        if (written != bytes_to_write) {
            fprintf(stderr, "Error: Failed to write to file '%s'.\n", filename);
            return;
        }

        fflush(fs->fp);
        string_ptr += written;
        bytes_remaining -= written;
        current_offset += written;

        if (bytes_remaining > 0) {
            cluster = fs->fat_table[cluster];
            if (cluster >= 0x0FFFFFF8) {
                fprintf(stderr, "Error: Unexpected end of cluster chain while writing to '%s'.\n", filename);
                return;
            }
        }
    }

    // Update offset
    fs->open_files[open_index].offset = new_offset;

    printf("Wrote %u bytes to '%s'.\n", string_length, filename);
}
*/

// Function to handle the 'rm' command
void cmd_rm(filesystem *fs, char *filename) {
    if (!filename) {
        fprintf(stderr, "Usage: rm <filename>\n");
        return;
    }

    // Check if file is open
    if (is_file_open(fs, filename)) {
        fprintf(stderr, "Error: Cannot remove '%s' because it is open.\n", filename);
        return;
    }

    // Find the directory entry
    fat32_dir_entry entry;
    uint32_t cluster_num;
    int index;
    if (!find_dir_entry(fs, filename, &entry, &cluster_num, &index)) {
        fprintf(stderr, "Error: File '%s' does not exist.\n", filename);
        return;
    }

    if (entry.attr & 0x10) { // Directory attribute
        fprintf(stderr, "Error: '%s' is a directory. Use 'rmdir' to remove directories.\n", filename);
        return;
    }

    // Reclaim file data (FAT entries)
    uint32_t cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;
    while (cluster < 0x0FFFFFF8) {
        uint32_t next_cluster = fs->fat_table[cluster];
        fs->fat_table[cluster] = 0x00000000; // Mark as free
        cluster = next_cluster;
    }

    // Update FAT table in the image
    fseek(fs->fp, fs->boot.reserved_sector_count * fs->boot.bytes_per_sector, SEEK_SET);
    fwrite(fs->fat_table, sizeof(uint32_t), fs->boot.fat_size_32, fs->fp);
    fflush(fs->fp);

    // Remove the directory entry by setting the first byte to 0xE5
    fat32_dir_entry deleted_entry = entry;
    deleted_entry.name[0] = 0xE5;
    if (write_dir_entry(fs->fp, &fs->boot, cluster_num, index, &deleted_entry) != 0) {
        fprintf(stderr, "Error: Failed to delete directory entry for '%s'.\n", filename);
        return;
    }

    printf("File '%s' deleted successfully.\n", filename);
}

// Function to handle the 'rmdir' command
void cmd_rmdir_cmd(filesystem *fs, char *dirname) {
    if (!dirname) {
        fprintf(stderr, "Usage: rmdir <dirname>\n");
        return;
    }

    // Prevent removing special directories
    if (strcasecmp(dirname, ".") == 0 || strcasecmp(dirname, "..") == 0) {
        fprintf(stderr, "Error: Cannot remove special directories '.' or '..'.\n");
        return;
    }

    // Check if directory exists
    fat32_dir_entry entry;
    uint32_t cluster_num;
    int index;
    if (!find_dir_entry(fs, dirname, &entry, &cluster_num, &index)) {
        fprintf(stderr, "Error: Directory '%s' does not exist.\n", dirname);
        return;
    }

    if (!(entry.attr & 0x10)) { // Not a directory
        fprintf(stderr, "Error: '%s' is not a directory.\n", dirname);
        return;
    }

    // Check if directory is empty (only '.' and '..' entries)
    fat32_dir_entry child_entry;
    uint32_t dir_cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;
    int is_empty = 1;
    size_t child_index = 0; // Changed from int to size_t

    while (1) {
        if (read_dir_entry(fs->fp, &fs->boot, dir_cluster, &child_entry, child_index) != 0) {
            break; // Reached end or error
        }

        if (child_entry.name[0] == 0x00) {
            break; // No more entries
        }

        if (child_entry.name[0] == 0xE5 || child_entry.name[0] == '.') {
            // Deleted entry or '.'/'..', skip
        } else if ((child_entry.attr & 0x0F) != 0x0F) {
            // Valid entry
            // Convert name to readable format
            char name[12];
            memcpy(name, child_entry.name, 11);
            name[11] = '\0';
            for(int i = 10; i >=0; i--){
                if(name[i] == ' ') name[i] = '\0';
                else break;
            }

            if (strcasecmp(name, ".") != 0 &&
                strcasecmp(name, "..") != 0) {
                is_empty = 0;
                break;
            }
        }

        child_index++;
        // Check if need to move to next cluster
        if (child_index >= (fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster) / sizeof(fat32_dir_entry)) {
            dir_cluster = fs->fat_table[dir_cluster];
            if (dir_cluster >= 0x0FFFFFF8) { // End of cluster chain
                break;
            }
            child_index = 0;
        }
    }

    if (!is_empty) {
        fprintf(stderr, "Error: Directory '%s' is not empty.\n", dirname);
        return;
    }

    // Remove directory entry by setting first byte to 0xE5
    fat32_dir_entry deleted_entry = entry;
    deleted_entry.name[0] = 0xE5;
    if (write_dir_entry(fs->fp, &fs->boot, cluster_num, index, &deleted_entry) != 0) {
        fprintf(stderr, "Error: Failed to delete directory entry for '%s'.\n", dirname);
        return;
    }

    // Reclaim directory clusters
    uint32_t cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;
    while (cluster < 0x0FFFFFF8) {
        uint32_t next_cluster = fs->fat_table[cluster];
        fs->fat_table[cluster] = 0x00000000; // Mark as free
        cluster = next_cluster;
    }

    // Update FAT table in the image
    fseek(fs->fp, fs->boot.reserved_sector_count * fs->boot.bytes_per_sector, SEEK_SET);
    fwrite(fs->fat_table, sizeof(uint32_t), fs->boot.fat_size_32, fs->fp);
    fflush(fs->fp);

    printf("Directory '%s' removed successfully.\n", dirname);
}

// Function to handle the 'info' command
void cmd_info(filesystem *fs) {
    printf("FAT32 Filesystem Information:\n");
    printf("------------------------------\n");
    printf("Position of root cluster: %u\n", fs->boot.root_cluster);
    printf("Bytes per sector: %u\n", fs->boot.bytes_per_sector);
    printf("Sectors per cluster: %u\n", fs->boot.sectors_per_cluster);
    printf("Reserved sector count: %u\n", fs->boot.reserved_sector_count);
    printf("Number of FATs: %u\n", fs->boot.num_fats);
    printf("Total sectors (32-bit): %u\n", fs->boot.total_sectors_32);
    printf("FAT size (32-bit): %u\n", fs->boot.fat_size_32);
    printf("Media type: 0x%X\n", fs->boot.media);
    printf("Total number of clusters in data region: %lu\n",
           (unsigned long)(fs->boot.total_sectors_32 / fs->boot.sectors_per_cluster));
    printf("Number of entries in one FAT: %lu\n",
           (unsigned long)(fs->boot.fat_size_32 * fs->boot.bytes_per_sector) / sizeof(uint32_t));
    printf("Size of image: %u bytes\n", (unsigned int)(fs->boot.total_sectors_32 * fs->boot.bytes_per_sector));
    printf("------------------------------\n");
}

// Function to handle the 'exit' command
void cmd_exit(filesystem *fs) {
    // Write back the FAT table before exiting
    fseek(fs->fp, fs->boot.reserved_sector_count * fs->boot.bytes_per_sector, SEEK_SET);
    fwrite(fs->fat_table, sizeof(uint32_t), fs->boot.fat_size_32, fs->fp);
    fflush(fs->fp);

    fclose(fs->fp);
    printf("Exiting...\n");
    exit(0);
}

// Function to handle the 'ls' command
void cmd_ls(filesystem *fs) {
    // List directory entries in the current directory
    fat32_dir_entry entry;
    uint32_t cluster = fs->current_dir_cluster;
    size_t index = 0;

    while (1) {
        if (read_dir_entry(fs->fp, &fs->boot, cluster, &entry, index) != 0) {
            // Reached end of directory or read error
            break;
        }

        // Check if entry is unused
        if (entry.name[0] == 0x00) {
            break;
        }

        // Check if entry is a valid file/directory
        if (entry.name[0] != 0xE5 && (entry.attr & 0x0F) != 0x0F) {
            // Convert name to readable format
            char name[12];
            memcpy(name, entry.name, 11);
            name[11] = '\0';
            for(int i = 10; i >=0; i--){
                if(name[i] == ' ') name[i] = '\0';
                else break;
            }
            printf("%s\n", name);
        }
        index++;
        // Assuming a fixed number of entries per cluster
        if (index >= (fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster) / sizeof(fat32_dir_entry)) {
            // Move to next cluster in the chain
            cluster = fs->fat_table[cluster];
            if (cluster >= 0x0FFFFFF8) { // End of cluster chain
                break;
            }
            index = 0;
        }
    }
}

void cmd_cd(filesystem *fs, char *dirname) {
    // Handle root directory and current directory
    if (strcmp(dirname, "/") == 0 || strcmp(dirname, ".") == 0) {
        strcpy(fs->current_path, "/");
        fs->current_dir_cluster = fs->boot.root_cluster;
        return;
    }

    // Handle parent directory '..'
    if (strcmp(dirname, "..") == 0) {
        fat32_dir_entry parent_entry;
        size_t parent_index = 1; // '..' is typically the second entry in the directory

        // Read the '..' entry
        if (read_dir_entry(fs->fp, &fs->boot, fs->current_dir_cluster, &parent_entry, parent_index) != 0) {
            fprintf(stderr, "Error: Failed to read '..' entry in current directory.\n");
            return;
        }

        // Extract the parent cluster number
        uint32_t parent_cluster = ((uint32_t)parent_entry.first_cluster_high << 16) | parent_entry.first_cluster_low;

        // Prevent navigating above the root directory
        if (parent_cluster == 0) {
            parent_cluster = fs->boot.root_cluster; // Handle special case for root
        }

        // Update the current directory cluster to the parent cluster
        fs->current_dir_cluster = parent_cluster;

        // Update current_path by removing the last directory component
        char *last_slash = strrchr(fs->current_path, '/');
        if (last_slash != NULL && last_slash != fs->current_path) {
            *last_slash = '\0'; // Truncate the path at the last '/'
        } else {
            // If only one '/', remain at root
            strcpy(fs->current_path, "/");
        }

        return;
    }

    // Find the directory entry for dirname
    fat32_dir_entry entry;
    uint32_t cluster_num;
    int entry_index;

    if (find_dir_entry(fs, dirname, &entry, &cluster_num, &entry_index) != 1) {
        fprintf(stderr, "Error: Directory '%s' not found.\n", dirname);
        return;
    }

    // Check if the entry is a directory
    if (!(entry.attr & 0x10)) { // 0x10 attribute indicates a directory
        fprintf(stderr, "Error: '%s' is not a directory.\n", dirname);
        return;
    }

    // Update current directory cluster
    fs->current_dir_cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster_low;

    // Update current_path
    if (strcmp(fs->current_path, "/") != 0) {
        strcat(fs->current_path, "/");
    }
    strcat(fs->current_path, dirname);
}


// Function to handle the 'mkdir' command
void cmd_mkdir(filesystem *fs, char *dirname) {
    // Check if directory/file already exists
    fat32_dir_entry entry;
    if (find_dir_entry(fs, dirname, &entry, NULL, NULL)) {
        fprintf(stderr, "Error: Directory or file '%s' already exists.\n", dirname);
        return;
    }

    // Find free directory entry
    int free_index = -1;
    fat32_dir_entry current_entry;
    uint32_t cluster = fs->current_dir_cluster;
    size_t index = 0;

    // Iterate through directory entries to find a free spot
    while (read_dir_entry(fs->fp, &fs->boot, cluster, &current_entry, index) == 0) {
        if (current_entry.name[0] == 0x00 || current_entry.name[0] == 0xE5) {
            free_index = index;
            break;
        }
        index++;
        if (index >= (fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster) / sizeof(fat32_dir_entry)) {
            // Move to next cluster in the chain
            cluster = fs->fat_table[cluster];
            if (cluster >= 0x0FFFFFF8) { // End of cluster chain
                break;
            }
            index = 0;
        }
    }

    if (free_index == -1) {
        fprintf(stderr, "Error: No free directory entries available.\n");
        return;
    }

    // Allocate a cluster for the new directory
    uint32_t new_dir_cluster;
    if (allocate_cluster_filesystem(fs, &new_dir_cluster) != 0) {
        fprintf(stderr, "Error: No free clusters available.\n");
        return;
    }

    // Initialize '.' and '..' entries in the new directory
    fat32_dir_entry dot_entry = {0};
    memset(&dot_entry, 0, sizeof(fat32_dir_entry));
    memcpy(dot_entry.name, ".          ", 11);
    dot_entry.attr = 0x10; // Directory attribute
    dot_entry.first_cluster_low = new_dir_cluster & 0xFFFF;
    dot_entry.first_cluster_high = (new_dir_cluster >> 16) & 0xFFFF;
    dot_entry.file_size = 0;

    fat32_dir_entry dotdot_entry = {0};
    memset(&dotdot_entry, 0, sizeof(fat32_dir_entry));
    memcpy(dotdot_entry.name, "..         ", 11);
    dotdot_entry.attr = 0x10; // Directory attribute
    dotdot_entry.first_cluster_low = fs->current_dir_cluster & 0xFFFF;
    dotdot_entry.first_cluster_high = (fs->current_dir_cluster >> 16) & 0xFFFF;
    dotdot_entry.file_size = 0;

    // Write '.' entry
    if (write_dir_entry(fs->fp, &fs->boot, new_dir_cluster, 0, &dot_entry) != 0 ||
        write_dir_entry(fs->fp, &fs->boot, new_dir_cluster, 1, &dotdot_entry) != 0) {
        fprintf(stderr, "Error: Failed to initialize directory entries.\n");
        return;
    }

    // Create directory entry in current directory
    fat32_dir_entry new_entry = {0};
    memset(&new_entry, 0, sizeof(fat32_dir_entry));
    convert_filename(dirname, new_entry.name);
    new_entry.attr = 0x10; // Directory attribute
    new_entry.first_cluster_low = new_dir_cluster & 0xFFFF;
    new_entry.first_cluster_high = (new_dir_cluster >> 16) & 0xFFFF;
    new_entry.file_size = 0;

    if (write_dir_entry(fs->fp, &fs->boot, fs->current_dir_cluster, free_index, &new_entry) != 0) {
        fprintf(stderr, "Error: Failed to write directory entry for '%s'.\n", dirname);
        return;
    }

    printf("Directory '%s' created successfully.\n", dirname);
}

// Function to handle the 'creat' command
void cmd_creat(filesystem *fs, char *filename) {
    // Check if file/directory already exists
    fat32_dir_entry entry;
    if (find_dir_entry(fs, filename, &entry, NULL, NULL)) {
        fprintf(stderr, "Error: File or directory '%s' already exists.\n", filename);
        return;
    }

    // Find free directory entry
    int free_index = -1;
    fat32_dir_entry current_entry;
    uint32_t cluster = fs->current_dir_cluster;
    size_t index = 0;

    // Iterate through directory entries to find a free spot
    while (read_dir_entry(fs->fp, &fs->boot, cluster, &current_entry, index) == 0) {
        if (current_entry.name[0] == 0x00 || current_entry.name[0] == 0xE5) {
            free_index = index;
            break;
        }
        index++;
        if (index >= (fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster) / sizeof(fat32_dir_entry)) {
            // Move to next cluster in the chain
            cluster = fs->fat_table[cluster];
            if (cluster >= 0x0FFFFFF8) { // End of cluster chain
                break;
            }
            index = 0;
        }
    }

    if (free_index == -1) {
        fprintf(stderr, "Error: No free directory entries available.\n");
        return;
    }

    // Allocate a cluster for the new file
    uint32_t new_file_cluster;
    if (allocate_cluster_filesystem(fs, &new_file_cluster) != 0) {
        fprintf(stderr, "Error: No free clusters available.\n");
        return;
    }

    // Create directory entry
    fat32_dir_entry new_entry = {0};
    memset(&new_entry, 0, sizeof(fat32_dir_entry));
    convert_filename(filename, new_entry.name);
    new_entry.attr = 0x20; // Archive attribute for file
    new_entry.first_cluster_low = new_file_cluster & 0xFFFF;
    new_entry.first_cluster_high = (new_file_cluster >> 16) & 0xFFFF;
    new_entry.file_size = 0;

    if (write_dir_entry(fs->fp, &fs->boot, fs->current_dir_cluster, free_index, &new_entry) != 0) {
        fprintf(stderr, "Error: Failed to write directory entry.\n");
        return;
    }

    printf("File '%s' created successfully.\n", filename);
}

// Function to handle the 'rename' command
void cmd_rename(filesystem *fs, char *old_filename, char *new_filename) {
    if (!old_filename || !new_filename) {
        fprintf(stderr, "Usage: rename <old_filename> <new_filename>\n");
        return;
    }

    // Check if new_filename already exists
    fat32_dir_entry temp_entry;
    if (find_dir_entry(fs, new_filename, &temp_entry, NULL, NULL)) {
        fprintf(stderr, "Error: '%s' already exists.\n", new_filename);
        return;
    }

    // Find the old file's directory entry
    fat32_dir_entry entry;
    uint32_t cluster_num;
    int index;
    if (!find_dir_entry(fs, old_filename, &entry, &cluster_num, &index)) {
        fprintf(stderr, "Error: '%s' does not exist.\n", old_filename);
        return;
    }

    // Ensure the file is closed
    if (is_file_open(fs, old_filename)) {
        fprintf(stderr, "Error: Cannot rename '%s' because it is open.\n", old_filename);
        return;
    }

    // Prevent renaming special directories
    if (strcasecmp(old_filename, ".") == 0 || strcasecmp(old_filename, "..") == 0) {
        fprintf(stderr, "Error: Cannot rename special directories.\n");
        return;
    }

    // Convert new_filename to FAT32 format
    unsigned char fat32_new_name[11];
    convert_filename(new_filename, fat32_new_name);

    // Update the directory entry's name
    memcpy(entry.name, fat32_new_name, 11);

    // Write back the updated entry
    if (write_dir_entry(fs->fp, &fs->boot, cluster_num, index, &entry) != 0) {
        fprintf(stderr, "Error: Failed to rename '%s'.\n", old_filename);
        return;
    }

    printf("Renamed '%s' to '%s' successfully.\n", old_filename, new_filename);
}

// Function to handle all commands
void handle_command(filesystem *fs, char *command, char **args, int argc) {
    if (strcasecmp(command, "info") == 0) {
        cmd_info(fs);
    }
    else if (strcasecmp(command, "exit") == 0) {
        cmd_exit(fs);
    }
    else if (strcasecmp(command, "ls") == 0) {
        cmd_ls(fs);
    }
    else if (strcasecmp(command, "cd") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: cd <dirname>\n");
            return;
        }
        cmd_cd(fs, args[1]);
    }
    else if (strcasecmp(command, "mkdir") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: mkdir <dirname>\n");
            return;
        }
        cmd_mkdir(fs, args[1]);
    }
    else if (strcasecmp(command, "creat") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: creat <filename>\n");
            return;
        }
        cmd_creat(fs, args[1]);
    }
    else if (strcasecmp(command, "rename") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: rename <old_filename> <new_filename>\n");
            return;
        }
        cmd_rename(fs, args[1], args[2]);
    }
    // Part 4: Read Commands
    else if (strcasecmp(command, "open") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: open <filename> <flags>\n");
            return;
        }
        cmd_open(fs, args[1], args[2]);
    }
    else if (strcasecmp(command, "close") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: close <filename>\n");
            return;
        }
        cmd_close(fs, args[1]);
    }
    else if (strcasecmp(command, "lsof") == 0) {
        cmd_lsof(fs);
    }
    else if (strcasecmp(command, "size") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: size <filename>\n");
            return;
        }
        cmd_size(fs, args[1]);
    }
    else if (strcasecmp(command, "lseek") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: lseek <filename> <offset>\n");
            return;
        }
        cmd_lseek(fs, args[1], args[2]);
    }
    else if (strcasecmp(command, "read") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: read <filename> <size>\n");
            return;
        }
        cmd_read(fs, args[1], args[2]);
    }
    // Part 5: Update Commands
    else if (strcasecmp(command, "write") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: write <filename> \"<string>\"\n");
            return;
        }
        cmd_write(fs, args[1], args[2]);
    }
    // Part 6: Delete Commands
    else if (strcasecmp(command, "rm") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: rm <filename>\n");
            return;
        }
        cmd_rm(fs, args[1]);
    }
    else if (strcasecmp(command, "rmdir") == 0) {
        if (argc < 2) {
            fprintf(stderr, "Usage: rmdir <dirname>\n");
            return;
        }
        cmd_rmdir_cmd(fs, args[1]);
    }
    else {
        fprintf(stderr, "Unknown command: %s\n", command);
    }
}

