# Group31 FAT32 File System Utility

## Project Description

This project focuses on understanding and manipulating a FAT32 file system image through a user-space, shell-like utility. By working directly with a FAT32 image, you will:

- Learn how cluster-based storage, FAT tables, sectors, and directory structures operate within the FAT32 file system.
- Implement a variety of commands to interact with the file system image, including creating, reading, writing, navigating directories, and managing files.
- Maintain the integrity of the file system image by avoiding corruption during operations.
- Handle errors gracefully, providing informative messages that help in troubleshooting issues while ensuring the file system state remains consistent.

This structured and modular approach allows each command to be implemented as a separate function, improving maintainability, readability, and overall design quality. Throughout the project, you can also verify the correctness of your operations by inspecting the file system image with external tools (like `hexedit`) or by mounting it as a loopback device.

## Group Member for Group31

- Erik Princi (ep16g@fsu.edu)



## Implementation Tasks

The following commands and functionalities have been implemented or targeted for implementation:

- **mounting**: Mount the FAT32 file system image for operations.
- **info**: Display information about the file system (e.g., BPB data).
- **exit**: Exit the shell utility.
- **navigation**: Manage directory traversal and current working directory context.
  - **cd**: Change the current directory.
  - **ls**: List contents of the current directory.
- **create**: High-level file creation logic.
  - **mkdir**: Create a new directory.
  - **creat**: Create a new file.
- **read**: Read data from files.
  - **open**: Open a file with specified mode (e.g., read/write).
  - **close**: Close an open file.
  - **lsof**: List open files.
  - **size**: Display the size of a file.
  - **lseek**: Change the read/write position in an open file.
  - **read**: Perform the actual read operation to retrieve data from a file.
  - **update**: Update in-memory metadata structures after I/O.
  - **write**: Write data to an open file.
- **rename**: Rename a file or directory.
- **delete**: Delete a file from the file system.
  - **rm**: Remove a file.
  - **rmdir**: Remove a directory.

- All steps above assigned to Erik Princi.

## How to Run the Program

1. **Prepare the File System Image:**
   - Obtain or create a FAT32-formatted disk image.
   - Place the image file (e.g., `fat32.img`) in the project directory.

2. **Compile the Utility:**
   - Navigate to the project directory containing the `Makefile` and source files.
   - Run:
     ```bash
     make
     ```
   - This will produce an executable, for example `filesys`.

3. **Run the Utility:**
   - Launch the shell-like utility:
     ```bash
     ./filesys fat32.img
     ```
   - Once inside the shell:
     - Use `info` to see file system details.
     - Use `cd`, `ls`, `mkdir`, `creat`, `open`, `read`, `write`, `close`, etc. to interact with the file system.
     - Use `exit` to leave the shell.

4. **Validate File System Integrity:**
   - Use `hexedit fat32.img` or similar tools to inspect the raw file system image.
   - Optionally, mount the image using a loopback device to confirm changes:
     ```bash
     sudo mount -o loop fat32.img /mnt
     ls /mnt
     ```
   - Verify that your operations are reflected in the mounted file system.
  
## Directory Structure:
root/
├── src/
│   ├── fat32.c
│   ├── filesys.c
│   └── commands.c
├── include/
│   ├── filesystem.h
│   ├── fat32.h
│   └── commands.h
├── Makefile
└── README.md

