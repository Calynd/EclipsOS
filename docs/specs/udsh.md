# UDSH Shell

## Prompt

**DECIDED.** The prompt is the current namespace path followed by `>`, for example `disk:0//usr/root>`. See [namespaces.md](namespaces.md).

**DECIDED (D-020).** The prompt does not contain the user name. The current user is shown in the top right corner of the screen, not in the prompt.

## Commands

**DECIDED** names. Syntax, flags and exact behavior are **OPEN**.

| Command | Description | Reference behavior |
|---|---|---|
| `go` | Change directory | `cd` in UNIX |
| `dir` | List directory | `ls` in UNIX |
| `md` | Create directory | `mkdir` in UNIX |
| `rd` | Remove directory | `rmdir` in UNIX |
| `ver` | Show system version | |
| `help` | Show command help | |
| `run` | Run UDE programs | |
| `del` | Remove files | |
| `cp` | Copy files to another directory | `cp` in UNIX |
| `mv` | Move files to another directory | `mv` in UNIX |
| `shdw` | Shut down the system | |
| `rbt` | Reboot the system | |

More commands are planned and not yet defined.

## Open items

| Item | Status |
|---|---|
| Command syntax and argument conventions | OPEN |
| Flags and options style | OPEN |
| Wildcards, redirection, pipes | OPEN |
| Scripting | OPEN |
| Error message format | OPEN |
| Remaining commands | OPEN |
