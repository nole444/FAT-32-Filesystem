// src/filesys.c
#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int parse_input(char *input, char **args, int max_args) {
    int arg_count = 0;
    char *p = input;
    char *arg_start = NULL;

    while (*p != '\0' && arg_count < max_args) {
        // Skip leading whitespace
        while (*p == ' ' || *p == '\t') {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        if (*p == '"') {
            // Start of quoted string
            p++; // Move past the starting quote
            arg_start = p;
            while (*p != '\0' && *p != '"') {
                p++;
            }
            if (*p == '"') {
                *p = '\0'; // Null-terminate the string and remove ending quote
                p++;
                args[arg_count++] = arg_start;
            } else {
                // Missing closing quote
                fprintf(stderr, "Error: Missing closing quote.\n");
                return -1;
            }
        } else {
            // Non-quoted argument
            arg_start = p;
            while (*p != '\0' && *p != ' ' && *p != '\t') {
                p++;
            }
            if (*p != '\0') {
                *p = '\0';
                p++;
            }
            args[arg_count++] = arg_start;
        }
    }
    return arg_count;
}

// Function to initialize the filesystem
int filesystem_init(filesystem *fs, const char *image_path) {
    fs->fp = fopen(image_path, "r+b");
    if (!fs->fp) {
        perror("Failed to open FAT32 image");
        return -1;
    }

     // Save the image name
    const char *base_name = strrchr(image_path, '/'); // Extract base name
    if (base_name) {
        strncpy(fs->image_name, base_name + 1, sizeof(fs->image_name) - 1);
    } else {
        strncpy(fs->image_name, image_path, sizeof(fs->image_name) - 1);
    }
    fs->image_name[sizeof(fs->image_name) - 1] = '\0'; // Ensure null termination

    // Read the boot sector
    if (read_boot_sector(fs->fp, &fs->boot) != 0) { // Now properly declared
        fprintf(stderr, "Failed to read boot sector.\n");
        fclose(fs->fp);
        return -1;
    }

    // Calculate FAT table size in bytes
    size_t fat_table_size = fs->boot.fat_size_32 * sizeof(uint32_t);

    // Allocate memory for FAT table
    fs->fat_table = malloc(fat_table_size);
    if (!fs->fat_table) {
        fprintf(stderr, "Failed to allocate memory for FAT table.\n");
        fclose(fs->fp);
        return -1;
    }

    // Read FAT table into memory
    fseek(fs->fp, fs->boot.reserved_sector_count * fs->boot.bytes_per_sector, SEEK_SET);
    size_t read_fat = fread(fs->fat_table, sizeof(uint32_t), fs->boot.fat_size_32, fs->fp);
    if (read_fat != fs->boot.fat_size_32) {
        fprintf(stderr, "Failed to read FAT table.\n");
        free(fs->fat_table);
        fclose(fs->fp);
        return -1;
    }

    fs->current_dir_cluster = fs->boot.root_cluster;
    strcpy(fs->current_path, "/");
    fs->open_file_count = 0;

    return 0; // Success
}

// Function to clean up the filesystem
void filesystem_cleanup(filesystem *fs) {
    if (fs->fp) {
        // Write back the FAT table to the image
        fseek(fs->fp, fs->boot.reserved_sector_count * fs->boot.bytes_per_sector, SEEK_SET);
        fwrite(fs->fat_table, sizeof(uint32_t), fs->boot.fat_size_32, fs->fp);
        fflush(fs->fp);

        fclose(fs->fp);
        fs->fp = NULL;
    }

    if (fs->fat_table) {
        free(fs->fat_table);
        fs->fat_table = NULL;
    }

    // Free other resources if necessary
}

// Main function
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <fat32_image.img>\n", argv[0]);
        return EXIT_FAILURE;
    }

    filesystem fs;
    memset(&fs, 0, sizeof(filesystem));

    if (filesystem_init(&fs, argv[1]) != 0) {
        fprintf(stderr, "Failed to initialize filesystem.\n");
        return EXIT_FAILURE;
    }

    char input[512];
    char *args[10];
    int arg_count;

    printf("FAT32 Filesystem Utility\n");
    printf("Type 'exit' to quit.\n");

    while (1) {
        printf("[%s]%s> ", fs.image_name, fs.current_path);
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // EOF encountered
        }

        // Remove trailing newline
        input[strcspn(input, "\n")] = '\0';

        // Tokenize input using the custom parser
        arg_count = parse_input(input, args, 10);
        if (arg_count == -1) {
            continue; // Error in parsing input
        }

        if (arg_count == 0) {
            continue; // Empty input
        }

        // Handle the command
        handle_command(&fs, args[0], args, arg_count);
    }

    filesystem_cleanup(&fs);
    return EXIT_SUCCESS;
}

