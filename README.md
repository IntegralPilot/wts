# Warthog Test Suite

This is a test suite for my [`warthog`](https://github.com/IntegralPilot/linux/tree/warthog-7.1) branch of Asahi Linux, which adds support for 4K processes running on the 16K Asahi kernel, using [a patch series](https://lpc.events/event/20/contributions/2630/) that Google made for Android, with my own adaptations.
(NOTE: very unstable!)

The test suite verifies that a variety of 4K processes showing complex behaviours can run correctly.

## Running

To test the warthog kernel, you can grab a release from the releases page, or clone this and build it yourself with `make package`. Either way, the generated executable is self-contained and can just be run with `./wts`.

## Structure

The tests are written in C and in `c/`. The initial set was written by my friend (who wishes to remain anonymous online), as I thought it was good for someone else to adversarially test the kernel.

The runner is in Haskell (I'm learning it, so wanted to practice) and in `runner/`. Along with that are some small C files that it uses for things which aren't as suited to Haskell. 

`embed.py` is a Python script that helps package everything together.

## Contributing

If you have ideas for other tests to add (or would like to add them), issues and PRs would be much appreciated!

## License

GPL 2 or later.
