# crawl

A first-person, grid-based dungeon crawler engine and editor in C11 on SDL3.
Campaigns are plain text: ASCII maps plus TOML for events, dialogue, items and
rules. SVG art scales crisply at any window size.

## Build

```sh
git submodule update --init --recursive
cmake -B b && cmake --build b
ctest --test-dir b
```

Binaries land in `b/bin/`. Options: `-DCRAWL_SYSTEM_SDL=ON` uses an installed
SDL3 instead of `vendor/SDL`; `-DCRAWL_SAN=ON` enables ASan/UBSan.

## Use

```sh
crawl play  <dir|pak>   # play a campaign
crawl edit  <dir>       # editor
crawl check <dir>       # validate refs, reachability, SVG subset, licenses
crawl pack  <dir>       # build a .pak / packaged game
crawl slices <dir>      # render wall/floor slices
```

`crawl-rt` is the play-only runtime shipped inside packaged games.

## TOML dialect

Data files are TOML (parsed by tomlc17), with one extension: `;` may separate
key/value pairs on a single line, e.g. `hp = 8; ac = 12`.

## License

ISC (see `LICENSE`). Vendored libraries are zlib/MIT; see `CREDITS.md`.
