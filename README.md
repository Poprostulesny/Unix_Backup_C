## sop-backup

Small, interactive backup daemon that mirrors one or more source directories to one or more target directories using inotify and incremental copy. Written in C (POSIX/Linux).

### Build
- Prerequisites: Linux with `gcc`, `make`, inotify support.
- Build: `make` (produces `sop-backup`).

### Run
Start the binary and control it via stdin. Commands are space-separated; wrap paths that include spaces in double quotes.

Supported commands:
- `add <source> <target1> [target2 ...]` — start backing up `source` into one or more empty target directories (created if missing). A source gets its own worker thread.
- `end <source> <target1> [target2 ...]` — stop backing up the listed targets for a given source; the worker stops when the source has no targets left.
- `list` — print all active sources and their targets.
- `restore <destination> <backup>` — pause all workers, restore files from `backup` into `destination` (creating it if needed), then resume workers. Existing files in `destination` are overwritten/removed to match the backup.
- `exit` — stop all workers and quit.

### Data structures (linked lists)
- `list_sc` / `node_sc`: all active backup sources; each holds its thread, inotify fd, and lists below.
- `list_tg` / `node_tr`: targets for a given source.
- `list_wd` / `Node_wd`: inotify watch descriptors per directory in a source tree (path, wd, suffix).
- `Ino_List` / `Ino_Node`: queued inotify events waiting to be processed.
- `M_list` / `M_node`: temporary move-event dictionary (pairs `IN_MOVED_FROM/TO` by cookie, expires after timeout).
- `list_bck` / `Node_init`: initial-backup jobs waiting to run when a source is added.

All lists are mutex-protected; most operations take locks only as long as needed and copy paths with `strdup`.

### How backups work
1. `add` performs validation: target must be an empty directory, cannot sit inside any source, and cannot be used by another backup. Paths are normalized with `realpath`.
2. An initial backup copies the whole source tree to each target:
   - Directories are created, files copied, symlinks recreated. Permissions and timestamps are preserved.
   - `inotify` watches are installed for every directory in the source tree.
3. Continuous sync:
   - `IN_CREATE` dir → recursively watch/copy into all targets; file → create empty file then copy on close.
   - `IN_CLOSE_WRITE` → copy modified file to all targets.
   - `IN_DELETE` / `IN_DELETE_SELF` → remove from targets (recursively if needed).
   - `IN_ATTRIB` → propagate permission/timestamp changes.
   - Moves/renames are paired via the move-event list; on match, targets are renamed (or copied+deleted across filesystems). Unmatched moves fall back to copy/delete after a timeout.
   - Events outside the source tree drop their watch and are ignored to avoid writing to unknown locations.
4. Each source thread sleeps briefly between iterations and stops when flagged or on program exit.

### Restore flow
- Command: `restore <destination> <backup>`.
- All backup threads acknowledge a pause via semaphores to avoid concurrent writes.
- Phase 1: walk `backup`, copying missing or changed files/symlinks to `destination` (creating directories first).
- Phase 2: walk `destination` and delete entries that do not exist in `backup`.
- Permissions and timestamps are preserved; paths are created if missing.
- Workers resume after restore completes.

### Things to watch when using
- Targets must start empty and not overlap with any source.
- Use absolute or relative paths; the tool normalizes them with `realpath`.
- Restores are destructive for the destination (it is made to match the backup).
- Signals `SIGINT`/`SIGTERM` trigger a clean shutdown (`stop_all_backups`).
