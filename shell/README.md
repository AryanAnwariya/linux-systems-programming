# belt-shell

A small interactive shell with a warehouse conveyor-belt theme. The prompt is `belt-control$`.

| Command | Type | Behaviour |
|---|---|---|
| `add_item <name>` | built-in | Adds an item to a fixed queue of 10; errors if the queue is full or the name is missing |
| `list_items` | built-in | Prints items in the order added, or `Queue is empty` |
| `date` | external | Runs `date` with `fork` + `execlp` |
| `ping <address>` | external | Runs `ping -c 4 <address>` |
| `quit` | built-in | The only normal way to exit |

**Ctrl+C does not kill the shell.** A `sigaction` handler sets a flag, and the main loop prints `[ALERT] Emergency stop triggered, item queue cleared`, empties the queue and carries on. Because `SA_RESTART` is off, a blocked `fgets` or `wait` returns with `EINTR`, so the alert shows straight away. Interrupted `wait` calls are retried.

```bash
../build/belt_shell
```

Concepts: read–parse–execute loop, built-ins run in the shell process vs external commands in a child, `fork`/`exec`/`wait`, async-signal-safe handlers (`volatile sig_atomic_t`), `EINTR`.
