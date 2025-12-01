# Backup Management System

This is a C program that implements a backup management system. It allows users to define backup sources and their target locations, with the ability to add, remove, and list backup configurations.

## Features

- **Add backups**: Define source directories and their target backup locations
- **Remove backups**: Delete specific target locations from backup configurations
- **List backups**: Display all current backup configurations
- **Inotify integration**: Monitor file system changes for backup operations
- **Directory validation**: Ensures targets are empty directories
- **Duplicate prevention**: Prevents adding duplicate sources or targets

## Usage

### Commands

1. **add** `<source_path>` `<target_path1>` `<target_path2>` ...
   - Add a new backup configuration with one or more target locations
   - Source directory must exist
   - Target directories must be empty or non-existent (will be created)

2. **end** `<source_path>` `<target_path1>` `<target_path2>` ...
   - Remove specified target locations from a backup configuration
   - If all targets are removed, the source configuration is deleted

3. **list**
   - Display all current backup configurations

4. **exit**
   - Cleanly exit the program and free all resources

## Implementation Details

### Data Structures

- **Node_source**: Represents a backup source with:
  - `source_full`: Full path to source directory
  - `source_friendly`: Friendly name of source directory
  - `targets`: List of target backup locations

- **Node_target**: Represents a backup target with:
  - `target_friendly`: Friendly name of target directory
  - `target_full`: Full path to target directory

- **List_source**: Doubly-linked list of source configurations
- **List_target**: Doubly-linked list of target configurations

### Key Functions

- `take_input()`: Parses command line input and creates backup configurations
- `parse_targets()`: Validates and adds target directories to a source
- `list_source_add()`, `list_target_add()`: Add nodes to their respective lists
- `list_source_delete()`, `list_target_delete()`: Remove nodes from lists
- `is_empty_dir()`: Checks if directory is empty or creates it if needed
- `find_element_by_target()`, `find_element_by_source()`: Search for existing configurations

### Error Handling

- All system calls use `ERR()` macro for proper error reporting
- Directory validation ensures targets are empty or non-existent
- Duplicate source/target prevention
- Memory allocation error checking

### Compilation

```bash
gcc -Wall -Wextra -std=c99 main.c -o backup_manager
```

### Requirements

- POSIX-compliant system (Linux, BSD)
- Standard C library
- `inotify` support for file system monitoring

## Example Usage

```bash
$ ./backup_manager
add /home/user/documents /backup/documents1 /backup/documents2
add /home/user/pictures /backup/pictures1
list
end /home/user/documents /backup/documents1
list
exit
```

## Notes

- The program uses `inotify` for monitoring file system changes (though not fully implemented in this version)
- Target directories must be empty or non-existent
- Source directories must exist
- The program automatically creates target directories if they don't exist
- Memory is properly managed with cleanup functions