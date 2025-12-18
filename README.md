# DISCLAIMER (READ THIS FIRST)
While checking this project you might see a lot of weird things, like:
- wrappers for functions which do nothing, except calling another function.
- unnecessarily complex data structures
- functions which take more arguments than they need
- seemingly pointless checks and overly complicated approaches.

This all is due to the fact that firstly I wrote this project on threads(as i didn't see the fact that it has been on forks). That version had a lot of optimization and queueing of tasks. As well as having one inotify fd per source. After noticing my mistake I tried my best to rewrite it so that it uses forks, however some things remained, as it was too time consuming to rewrite it from scratch. However if you want to see(or grade if that approach is acceptable) the multithreaded version of which I am a lot more proud than of this, then you can find it at "https://github.com/Poprostulesny/Unix_backup_multithreaded". 

Sincerely, Mateusz Leśniczak.

---

## sop-backup

Small, interactive backup daemon that mirrors one or more source directories to one or more target directories using inotify and incremental copy. Written in C (POSIX/Linux). Current implementation uses **one forked process per target**; the parent coordinates them with signals.

### Build
- Prerequisites: Linux with `gcc`, `make`, inotify support.
- Build: `make` (produces `sop-backup`).

### Run
Start the binary and control it via stdin. Commands are space-separated; wrap paths that include spaces in double quotes.

Supported commands:
- `add <source> <target1> [target2 ...]` — start backing up `source` into one or more empty target directories (created if missing). Each target gets its **own child process** with its own inotify fd and queues.
- `end <source> <target1> [target2 ...]` — stop backing up the listed targets for a given source; only the matching child is signaled and reaped. The source is removed when it has no targets.
- `list` — print all active sources and their targets.
- `restore <destination> <backup>` — parent pauses all children via `SIGUSR1`, waits for ACKs, restores files from `backup` into `destination` (creating it if needed), then resumes children with another `SIGUSR1`. Existing files in `destination` are overwritten/removed to match the backup.
- `exit` — send `SIGUSR2` to all children, reap them, and quit. `SIGINT/SIGTERM` trigger the same cleanup.

### Approach overview (why forks, signals, and per-target state)
- **Per-target isolation:** Each target has its own process, inotify fd, watch list, event queue, and move dictionary. No mutexes are needed; lists are single-threaded per child.
- **Process groups for broadcast:** Children are placed in the parent’s process group (`setpgid(pid, getpid())`). Broadcasts for pause/resume use `kill(-getpid(), SIGUSR1)`; targeted shutdowns (`end`) use `kill(child_pid, SIGUSR2)`.
- **Pause/resume for restore:** Parent blocks `SIGUSR1`, broadcasts pause, then `sigsuspend`-waits until every child ACKs via the SIGUSR1 handler counter. After restore, a second `SIGUSR1` resumes work.
- **Cleanup on exit:** Parent sends `SIGUSR2` to children, reaps all with `wait`, resets counts, and frees lists. SIGUSR2 is blocked in parent to avoid self-termination.
- **Path validation and safety:** `realpath` normalizes paths; targets must be empty and not inside a source; events outside the source tree drop their watches to avoid writing to unknown locations.

### Data structures (linked lists)
- `list_sc` / `node_sc`: all active sources; holds only metadata and the target list.
- `list_tg` / `node_tr`: targets for a source; owns `child_pid`, `inotify_fd`, watcher list, inotify event list, and move dictionary.
- `list_wd` / `Node_wd`: inotify watch descriptors per directory in a source tree (path, wd, suffix).
- `Ino_List` / `Ino_Node`: queued inotify events waiting to be processed (per target).
- `M_list` / `M_node`: temporary move-event dictionary (pairs `IN_MOVED_FROM/TO` by cookie, expires after timeout).

### How backups work (per target/child)
1. `add` validates paths (empty target, not inside any source, not reused elsewhere) via `realpath`.
2. Child initial backup:
   - Copies the full source tree to its target (dirs, files, symlinks) preserving permissions/timestamps.
   - Installs inotify watches for every directory under the source (child-local fd and watcher list).
3. Continuous sync inside the child:
   - `IN_CREATE` dir → watch it and clone it into the target; file → create empty file then copy on close.
   - `IN_CLOSE_WRITE` → copy modified file to the target.
   - `IN_DELETE` / `IN_DELETE_SELF` → remove from target (recursively if needed).
   - `IN_ATTRIB` → propagate permission/timestamp changes.
   - Moves/renames paired via the move-event list; on match, target is renamed (or copy+delete across filesystems). Unmatched moves fall back to copy/delete after timeout.
   - Events outside the source tree cause the watcher to be removed to avoid writing to unknown locations.

### Restore flow
- Command: `restore <destination> <backup>`.
- Parent blocks SIGUSR1, broadcasts pause to the process group, waits for ACKs (SIGUSR1 handler increments a counter).
- Phase 1: walk `backup`, copying missing or changed files/symlinks to `destination` (creating directories first).
- Phase 2: walk `destination` and delete entries that do not exist in `backup`.
- Permissions and timestamps preserved; paths are created if missing.
- Resume children via another `SIGUSR1` broadcast.

### Things to watch when using
- Targets must start empty and not overlap with any source.
- Use absolute or relative paths; the tool normalizes them with `realpath`.
- Restores are destructive for the destination (it is made to match the backup).
- `SIGINT`/`SIGTERM` act like `exit`: signal children to stop, reap them, and clean up. SIGUSR2 is blocked in the parent so only children terminate on that signal.
