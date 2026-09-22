# tools

`sweep.sh` builds every demo in `upstream/src/demos/` in turn and records one row per demo in
`build/sweep.tsv`:

```
<name> <TAB> OK | BUILD-FAIL | IMPORTS-FAIL <TAB> <first reason>
```

Run it from the WSL builder, in the background - it links a full Mesa ELF per demo and takes
around half an hour for the set. It derives its own root, so it can be invoked by whatever path
reaches it:

```
bash <title>/tools/sweep.sh
```

From Windows that means going through the builder, and `MSYS_NO_PATHCONV=1` is needed or Git Bash
rewrites the WSL path into a Windows one and `bash` reports the script missing:

```
MSYS_NO_PATHCONV=1 wsl -d oops-builder -u root -- bash <title-in-wsl>/tools/sweep.sh
```

**Nothing else may build in this directory while it runs** - every demo links to the same
`build/mesa-demos.elf` and they collide.

The three outcomes are meaningfully different and the distinction is the point of the tool:

- **BUILD-FAIL** - the demo does not compile. Something it calls is not declared, so the port
  surface is missing a header or a define.
- **IMPORTS-FAIL** - the demo compiles and links, and `make imports` refuses to write a manifest
  because a symbol is undefined. This is the *useful* failure: it names exactly what is missing.
  `app.mk` links payloads with `--unresolved-symbols=ignore-all`, so without this check the title
  would package happily and fault on the console instead.
- **OK** - compiles, links, and every symbol is placed.

**None of those means the demo draws anything.** That needs a console, and the census cannot say.
