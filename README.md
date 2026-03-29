# smash - Small Shell

A Linux shell implementation written in C/C++ as part of the 
Operating Systems course at the Technion (Israel Institute of Technology).

## Features
- Built-in commands: `cd`, `pwd`, `jobs`, `fg`, `kill`, `quit`, `alias` and more
- Background process execution with `&`
- Job control and process management
- Signal handling (Ctrl+C → SIGKILL to foreground process)
- I/O redirection (`>`, `>>`)
- Pipes (`|`, `|&`)
- External command execution via `fork` + `exec`

## Build
```bash
make
```

## Run
```bash
./smash
```

## Tech
C/C++ · Linux System Calls · fork/exec/waitpid · Signals · IPC
