# Contributing to Daurade

Daurade is a small fork. Bugs and ideas: [Issues](https://github.com/delirioux/daurade/issues) on this repository,
please; not Felucca's or Tuna's trackers, since their authors do not maintain Daurade.

With a bug, say the Daurade version (the splash), the style and song, the steps to reproduce it, what you expected
and what happened. A change to the grooves goes in `firmware/src/da_groove.c`, to the sounds in `da_sounds.h` (run
`sh tests/da_audition.sh` and keep the trims even). The keys only ever play music. `sh tests/da_tests.sh` must pass,
and the screenshots in `docs/screens/` change with the screen.
