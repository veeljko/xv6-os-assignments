# xv6 OS Assignments

The first three Operating Systems assignments, collected in one repository. Each assignment contains its own x86 xv6 source tree and builds independently.

## Assignments

| Directory | Focus | Original commits |
| --- | --- | --- |
| [homework-1](homework-1/) | Console input and keyboard handling | 6 |
| [homework-2](homework-2/) | Text editor, console and filesystem system calls | 6 |
| [homework-3](homework-3/) | Process snapshots and related system calls | 9 |

## Build and run

Use Linux, WSL, or another compatible Unix environment with GNU Make, an x86 ELF GCC/binutils toolchain, Perl, and QEMU. See each assignment's original README for details.

```sh
make -C homework-1
make -C homework-1 qemu
```

Replace `homework-1` with `homework-2` or `homework-3` to build another assignment. For a terminal-only QEMU session, use `make -C homework-1 qemu-nox`.

## Preserved history

All 21 original commits are ancestors of `main`, with their original commit hashes, authors, timestamps, and messages. Each source tree was imported into its directory through a merge commit, without squashing or rewriting history.

The original branches are also available as `history/homework-1/trunk`, `history/homework-2/trunk`, and `history/homework-3/trunk`. Earlier commits retain their original paths, before the directories were combined.

```sh
git log --graph --oneline --all
git log history/homework-1/trunk
```

## Sources and licensing

The assignments originated from:

- OSRAF/os-domaci-1-vmladenovic1524rn
- OSRAF/os-domaci-2-vmladenovic1524rn
- OSRAF/os-domaci-3-vmladenovic1524rn

Original xv6 READMEs, acknowledgments, and LICENSE files are retained inside each assignment directory.
