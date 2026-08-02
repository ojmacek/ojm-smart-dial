# Smart Dial v0.1.0-poc

This is the first public proof-of-concept milestone for Smart Dial, an
automotive rotary controller built around the Waveshare
ESP32-S3-Touch-AMOLED-1.32.

## Highlights

- Responsive encoder-driven interface
- Four demonstrator pages
- Partial display updates with clean arcs and transitions
- Animated fan and a custom OJM Systems startup sequence
- First documented mechanical and firmware baseline

## Scope

The release demonstrates the interaction model, visual direction and current
mechanical architecture. It is not intended for use as a finished automotive
control unit.

## Before building

Copy the existing `logomodre256.c` asset into `firmware/SmartDial/`, install the
libraries listed in the main README and use the documented ESP32-S3 board
settings.

