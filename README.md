# NITCbase

This repository contains NITCbase, a relational database management system
(RDBMS) built from scratch in C++ as part of the Database System Design Lab at
the National Institute of Technology Calicut (NITC).

## Overview

NITCbase was developed incrementally through the following stages:

### Preparation

- [Installation and preparation](https://nitcbase.github.io/docs/Roadmap/Stage00)
- [Disk interface](https://nitcbase.github.io/docs/Roadmap/Stage01)
- [Record blocks and catalogs](https://nitcbase.github.io/docs/Roadmap/Stage02)
- [Buffer and catalog caches](https://nitcbase.github.io/docs/Roadmap/Stage03)

### Storage and access

- [Linear search](https://nitcbase.github.io/docs/Roadmap/Stage04)
- [Opening relations](https://nitcbase.github.io/docs/Roadmap/Stage05)
- [Buffer management and disk write-back](https://nitcbase.github.io/docs/Roadmap/Stage06)
- [Record insertion](https://nitcbase.github.io/docs/Roadmap/Stage07)
- [Relation creation and deletion](https://nitcbase.github.io/docs/Roadmap/Stage08)

### Query processing and indexing

- [Selection and projection](https://nitcbase.github.io/docs/Roadmap/Stage09)
- [B+ tree search](https://nitcbase.github.io/docs/Roadmap/Stage10)
- [Index creation and deletion](https://nitcbase.github.io/docs/Roadmap/Stage11)
- [Joins](https://nitcbase.github.io/docs/Roadmap/Stage12)

The final system is organized into layers for the frontend, schema and
catalogs, relational algebra, block access, B+ tree indexing, caching,
buffering, and simulated disk storage.

## Repository contents

- `NITCbase/mynitcbase/` - main database engine
- `NITCbase/XFS_Interface/` - disk setup and inspection utility
- `NITCbase/Files/` - sample input, batch, and output files

## Building

```bash
cd NITCbase/mynitcbase
make
./nitcbase
```

## Notes

Run only one of NITCbase or the XFS interface at a time to avoid disk and
cache desynchronization. Earlier stages may also depend on their corresponding
stage completion state.

## Documentation

For detailed stage instructions, see the
[NITCbase documentation](https://nitcbase.github.io/).
