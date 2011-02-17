This is a tiny tool that measures per-process I/O as best it can for unrooted
Android phones.

You'll need the Android NDK to compile this. Simply edit the paths in
`Makefile.config` appropriately and run `make`. Copy the resulting binary to
your phone via `adb push` and invoke it with `adb shell`.

