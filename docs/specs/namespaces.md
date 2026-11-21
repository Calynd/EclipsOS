# Namespaces and Resources

## Identifiers

**DECIDED**

| Namespace | Meaning |
|---|---|
| `disk:0//` | Root of disk 0 |
| `disk:1//` | Root of disk 1 |
| `ram:0//` | Temporary RAM space 0 |
| `net:0//` | Network resource or interface 0 |

`disk:N` identifies a physical storage device of any kind (HD, SSD, floppy, others). Numbers are independent between namespaces.

Examples:

```text
disk:0//
disk:0//usr
disk:0//usr/root
disk:1//
ram:0//
net:0//
```

The current namespace appears in the UDSH prompt:

```text
disk:0//>
disk:0//usr>
disk:0//usr/root>
disk:1//>
ram:0//>
net:0//>
```

Drive letters such as `C:\` or `A:\` are not used.

## Open items

| Item | Status |
|---|---|
| How namespaces are registered | OPEN |
| How resources are accessed | OPEN |
| How resources are mounted or made available | OPEN |
| How paths are resolved | OPEN |
| Permissions and isolation between resources | OPEN |
| Partitions inside a `disk:N` | OPEN |
| Which disk is `disk:0` when several are present | OPEN |
| Relationship between `dev/` and namespaces | OPEN |
