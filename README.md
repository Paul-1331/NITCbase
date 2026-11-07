# NITCbase

NITCbase is a small, educational relational database management system
developed in C++ as part of the Database System Design Lab at the National
Institute of Technology Calicut (NITC).

It demonstrates how a database processes relational operations and manages
data from a user-facing command interface down to block-level disk storage.

## Architecture

```text
+--------------------------------------------------+
| Frontend Interface - command parsing             |
+--------------------------------------------------+
| Schema - tables, attributes, and catalogs        |
+--------------------------------------------------+
| Relational Algebra - select, project, insert     |
|                       and join                   |
+--------------------------------------------------+
| Block Access - record and block operations       |
+--------------------------------------------------+
| B+ Tree - indexing                               |
+--------------------------------------------------+
| Cache - relation and attribute metadata          |
+--------------------------------------------------+
| Buffer - in-memory block buffering               |
+--------------------------------------------------+
| Disk - simulated persistent storage              |
+--------------------------------------------------+
```

The main database engine is located in [`NITCbase/mynitcbase`](./NITCbase/mynitcbase).
The repository also contains an `XFS_Interface` utility for disk setup and
sample input, batch, and output files under `NITCbase/Files`.

## Build and run

```bash
cd NITCbase/mynitcbase
make
./nitcbase
```
