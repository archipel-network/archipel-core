# File CLA Protocol

> Description of the file-based convergence layer

## Configuration

To enable file CLA, juste add `file:` to your cla config string in command line options

## Address

Cla address is defined as a string starting with `file:` followed by the full path of a directory containing bundle files. Path MAY end with a `/`.

Example
```
file:/var/bundles
```

## Bundle files

In Root folder of contact, each file in the directory ending with extesion `.bundle7` or `.bundle6`.

**Todo**