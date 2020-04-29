# localflock

This is a small library that redirects the creation of BSD-style (flock) and POSIX-style (fnctl) locks to `/var/lock` instead to creating locks for the actual files that should be locked. The created locks are fully functional and affect all programs that have this library loaded.

## How it works

The library `liblocalflock` is either preloaded using `LD_PRELOAD` or directly linked to a program. When the program calls one of the system functions `flock`, `fnctl`, or `close`, then these calls intercepted. 

When the program wants to lock the file `/path/to/original/file`, then a new empty temporal file `/var/lock/localflock/b2d33d7f4dd528265ae5dcd613d8e33109ab527b` is created and locked using the file system of `/var/lock`, which always provides locking capabilities. The name of the temporal file is a SHA1-hash of the actual filename and will always be the same when different programs try to lock the same file. 

## How to use it

1. Use the `LD_PRELOAD` environment variable. This will cause all programs running within the same shell to load `liblocalflock.so` on startup:

```
export LD_PRELOAD=/absolute/path/to/liblocalflock.so
```

2. Link your program against `liblocalflock.so`:

```
gcc -llocalflock -L/absolute/path/to -Wl,-rpath=/absolute/path/to
```

In the second case, only programs linked to `liblocalflock` will respect created locks, in the first case all programs running within the same shell are affected. 

## Configuration

No configuration required, but the following environment variables are available:

* `LOCALFLOCK_DEBUG`: if defined, all function calls shown on `stderr`.
* `LOCALFLOCK_LOCKDIR`: can point to any directory which supports locks. The default is `/var/lock/localflock`.
* `LOCALFLOCK_SHOW_NAMES`: if defined, lock files show the actual name of the locked file and are not a hash code. In terms of data privacy, this is not optimal, because everyone can see who is working on which files.
