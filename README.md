# GNU Shogi

Shogi, Japanese chess, against the computer, in a curses display with an
opening book and a choice of time controls.

## Play

With [av](https://asciiverse.sh), the Asciiverse client:

```
av install gnushogi
av play gnushogi
```

av starts the curses display (`gnushogi -C`). Pick a level, then type
moves the way the board is labelled: `7g7f` moves from 7g to 7f, `2d2c+`
promotes, `P*5e` drops a pawn from your hand. `help` lists every command,
`hint` suggests a move, `undo` takes one back.

## Credits

GNU Shogi began in 1993 when Matthias Mutz turned GNU Chess 4.0 into a
shogi program; Mike Vanier, Luis Felipe Strano Moraes and Yann Dirson
maintained it after him. This build is the last state of Yann Dirson's
development branch at Savannah (2014), with its opening book found
beside the program and the player's files kept in a folder of their own.
See PATCHES.md, and README for upstream's own notes.
