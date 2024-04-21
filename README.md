# Repline - A highly interactive REPL #

Highly interactive REPL for the use in POSIX shells supporting command line editing, history, and command line completion using suggestions.

## Build ##

In a terminal run:
```
$ make
$ make install

```
Installation prefix is `/usr/local` by default and can be changed by setting `PREFIX`:
```
$ make PREFIX=/my/install/path
$ make install

```

## Dependencies ##
The history file backend currently needs [SQLite](https://github.com/sqlite/sqlite).

## References ##
* repline is based on [isocline](https://github.com/jorbakk/isocline) by Daan Leijen.
