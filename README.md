# Rci — The highly interactive Plan 9 shell #

Standalone port of rc shell written by Tom Duff with an interactive REPL frontend supporting
command line editing, history, and command line completion using suggestions.

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
The REPL frontend uses [repline](https://github.com/jorbakk/repline).

## References ##
* [Rc — The Plan 9 Shell](http://9p.io/sys/doc/rc.html)
* [*rc(1)*](./rc.md)
* rci is based on this [rc port](https://github.com/benavento/rc), which in turn is based on rc in
  the [Plan 9 port](https://9fans.github.io/plan9port) by Russ Cox et al.
