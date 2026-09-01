# example-pdm

A minimal, packable `.pdm` package. This folder is the **source layout** you hand
to `pmm pack` — it shows the directory style a package should have before it becomes
a `.pdm` archive.

## Layout

```
example-pdm/
├── pdm-control                 # package metadata (REQUIRED)
├── bin/
│   └── hello                   # executable payload -> added to PATH after install
├── share/
│   └── data/
│       └── README.txt          # arbitrary payload file (folder name is free-form, NOT the package name)
└── README.md                   # this file
```

## `pdm-control`

A `KEY: value` text file, one field per line, at the root of the package folder.
Recognized fields: `Package`, `Version`, `Architecture`, `Description`, `Maintainer`.

```ini
Package: example
Version: 1.0.0
Architecture: all
Description: An example PMM package demonstrating the .pdm source layout
Maintainer: PMM example <example@pmm.dev>
```

- `Package`  — the lowercase package name (also becomes the folder name on install).
- `Version`  — any version string; supports `==`, `>=`, `<=`, `>`, `<`, `!=` matching.
- `Architecture` — `all`, or a specific arch (e.g. `amd64` / `x86_64`).
- `Description` / `Maintainer` — optional, used for registry dots and the uninstall entry.

## How the layout maps to a `.pdm`

`pmm pack <dir>` wraps the folder into a tar archive with three members
(mirroring a Debian `.deb`):

| member           | contents                                |
|------------------|-----------------------------------------|
| `control.tar.gz` | just `pdm-control`                      |
| `data.tar.gz`    | everything else in the folder           |
| `sha256sums`     | SHA-256 of `control.tar.gz` + `data.tar.gz` |

So anything you place in `example-pdm/` (except `pdm-control`) ends up in `data.tar.gz`
and is extracted into the install root on install.

## Pack + install

```bash
# from the repo root
pmm pack examples/example-pdm example-1.0.0.pdm
pmm install example-1.0.0.pdm

# `hello` should now be on PATH
hello
```

To publish it to a registry mirror, also add a registry entry pointing at the `.pdm`
file — see `mirror/packages/<pkg>.json` for the variant format.
