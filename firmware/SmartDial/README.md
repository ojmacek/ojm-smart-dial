# Firmware directory

Arduino requires the sketch folder and the main `.ino` file to use the same
name. Keep this directory named `SmartDial`.

Before compiling, copy the existing OJM Systems logo asset into this directory:

```text
SmartDial.ino
SmartDial_Fonts_DINish.h
logomodre256.c
```

`logomodre256.c` must be an LVGL 8-compatible RGB565 image descriptor and LVGL
must be configured with `LV_COLOR_DEPTH` set to `16`.

The stable public firmware version is shown in the boot message. Experimental
sketches should be developed in a branch or a separate local copy, not added as
`SmartDial_old.ino`, `SmartDial_final2.ino` and similar files here.

