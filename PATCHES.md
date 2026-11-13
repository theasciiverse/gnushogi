# Changes from upstream

This is the Asciiverse edition of GNU Shogi (GPL-3.0-or-later), from
upstream's repository at Savannah, git.savannah.gnu.org/git/gnushogi.git
(the `upstream` remote), at the head of `master`, `5bb0b5b` (Yann Dirson,
2014-11-19): the unreleased 1.5 development after release 1.4.2, which
fixes the forfeit in the last move of a time control and switches the
build to automake. The `asciiverse` branch carries the changes below,
all under `AV_PATHS`, so other builds are unchanged.

## Code

- **The opening book is found beside the program.** The book's path is
  compiled in (`BOOK`, under the install prefix), so a program unpacked
  anywhere else said "Can't find book." and played without it. When the
  compiled path cannot be read, the book (`gnushogi.tbk`) is read from
  the program's own folder (`gnushogi/main.c`, `av_paths()`; the
  program's folder from /proc/self/exe, `_NSGetExecutablePath` or
  `GetModuleFileName`). A book given with `-b` is left alone.
- **The binary book goes to the player's state folder.** GNU Shogi turns
  the text book into a binary one at first start and writes it beside
  the compiled path. It now writes `gnushogi.bbk` to
  `$XDG_STATE_HOME/gnushogi`, else `~/.local/state/gnushogi` (Linux),
  `~/Library/Application Support/gnushogi` (macOS) or
  `%LOCALAPPDATA%\gnushogi` (Windows). One given with `-B` is left alone.
- **Started from its own folder, as av starts it, it moves into the state
  folder**, so games kept with `save` (and `list`, and `get` to read them
  back) land there instead of in the install folder, which an update
  replaces. Started from anywhere else, it stays where it is.
- **Nothing printed over the curses screen at first start.** Building the
  binary book printed "creating bookfile ..." and a line per 1000
  records, which landed on the board. `opts.h` turns on upstream's own
  `QUIETBOOKGEN` in this build, and those two messages now follow it
  (`gnushogi/book.c`; upstream's comment on the second already said "may
  want to get rid of this").

The curses display is unchanged; av starts it with `-C` (upstream's
default display is the text interface for XBoard and other front ends).

## Packaging

- `asciiverse/package.sh` runs autoreconf and configure
  (`--prefix=/usr --libdir=/usr/share`, so the compiled book folder is
  `/usr/share/gnushogi`), builds the opening patterns first (their
  helper program races under a parallel make), then the program, and
  packs `gnushogi-<target>` with the program, `gnushogi.tbk`, COPYING,
  README, NEWS, the manual page and this file.
- `.github/workflows/release.yml`: static musl Linux (Alpine, x86-64 and
  ARM64; ncurses' tinfo stands in for the termcap library configure
  links its curses test with), macOS (Intel and Apple silicon, system
  ncurses), on our tags (`*-av.*`), plus .deb, .rpm, .apk and Arch
  packages of the static Linux binary by nfpm (`asciiverse/nfpm.yaml`),
  with the book in `/usr/share/gnushogi`. No Windows: the curses display
  uses POSIX signals.
- `asciiverse/record.sh` records the catalog preview,
  `asciiverse/gnushogi.cast`, while `asciiverse/drive.py` plays Black: a
  second GNU Shogi in its text interface on a pseudo terminal chooses
  the moves, and the driver types them into the curses display, which
  plays White at level 1 with its search depth lowered to 3.

## Checked

Linux, native and static (Alpine): played at 100x30 and 80x24 from the
install folder with a fresh state folder; the book loads ("Book used
5668(8000)"), the binary book and a saved game land in the state folder,
help, save and quit work. Computer against computer at depth 3 and 4
plays to mate. macOS: not built here.

## Builds on macOS (written to go upstream)

- `gnushogi.h` began with an unconditional `#define inline` ("Hack for
  anal-retentive ANSI-compliance"). Defined before the system headers, it
  turned macOS's inline `<ctype.h>` and `<sys/select.h>` helpers
  (`isupper`, `__darwin_fd_set`, ...) into ordinary external functions in
  every object, and linking `pat2inc` failed on 31 duplicate symbols. The
  hack is gone (the `#ifndef __GNUC__` fallback further down stays), and
  the three external functions that relied on it, `CB` in `util.c` and
  `BRLscan` and `KingScan` in `eval.c`, are plain functions now, since a
  C99 `inline` external definition with no `extern` declaration emits no
  symbol.
- The release job publishes when the Linux builds succeed, with macOS
  added whenever it builds.
