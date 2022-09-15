Magic Touchscreen
=================

A simple library to make touch screen input on linux
easily available to applications made in Unity etc.
Can also be used in any other programming env,
as a wrapper around reading events directly.

## Dependencies
Magic Touchscreen uses libevdev for input events, so
install `libevdev-dev` on Ubuntu, or the corresponding
package on your distro.

## Building
Run `make`

## Testing
Mark the `magicts_test.cpp` as executable by running
`chmod +x magicts_test.cpp`, and then execute it. It will
compile the library and make use of it to run a simple test
program. You should see touch events printed to the terminal,
simillarly to the `evtest` program available in some linux distros.

**Note that the running user must be part of the input group,
or the application must be run using sudo.**

## Interface
Magic Touchscreen has a simple interface of three
functions:
`magicts_initialize`, `magicts_update`, and `magicts_finalize`

The first and last sets up and frees the touch screen context.
Magic Touchscreen searches through a list of candidate input
event files (/dev/input/eventX) and looks for a file it can
open and that supports ABS_MT events. It chooses the first
available file that matches.

The update calls fills and returns a TouchData struct that
holds info on up to 10 touches.

Magic Touchscreen does not support pressure or tilt/angle,
simply position and up/down for each touch

The TouchData uses a normalized coordinate system with origo
at the top left corner relative to the screens native orientation.
Flipping and/or swapping axes is up to the client application.
