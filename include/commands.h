// src/commands.h
#ifndef COMMANDS_H
#define COMMANDS_H

#include "filesystem.h"

// Existing function declarations...

// Part 4: Read Commands
void cmd_open(filesystem *fs, char *filename, char *flags);
void cmd_close(filesystem *fs, char *filename);
void cmd_lsof(filesystem *fs);
void cmd_size(filesystem *fs, char *filename);
void cmd_lseek(filesystem *fs, char *filename, char *offset_str);
void cmd_read(filesystem *fs, char *filename, char *size_str);

// Part 5: Update Commands
void cmd_write(filesystem *fs, char *filename, char *content);
void cmd_rename(filesystem *fs, char *old_filename, char *new_filename);

// Part 6: Delete Commands
void cmd_rm(filesystem *fs, char *filename);
void cmd_rmdir_cmd(filesystem *fs, char *dirname);

// Command handler
void handle_command(filesystem *fs, char *command, char **args, int argc);
char *strdup(const char *s);

#endif // COMMANDS_H

