Magic Touchscreen
=================

A library to make touch screen input on linux
easily available to applications made in Unity etc.
Can also be used in any other programming env,
as a wrapper around reading events directly.

Magic Touchscreen has a simple interface of three
functions:
'magicts_initialize', 'magicts_update', and 'magicts_finalize'

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

