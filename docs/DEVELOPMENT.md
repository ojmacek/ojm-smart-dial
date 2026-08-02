# Development notes

Smart Dial began as a quick functional mock-up and is now being rebuilt as a
presentable bachelor-project proof of concept. The goal of this page is to show
the design path without publishing a folder full of obsolete sketches.

## Milestones

### Early interaction prototype

- Verified the rotary encoder and push input
- Tested the round-display layout and basic climate-control pages
- Established the first mechanical envelope

### Mechanical rework

- Redesigned the fixed tube and rotating geared ring in Fusion 360
- Added screw-mounted display supports inside the tube
- Added physical-button locations and travel stops
- Reduced the overall diameter while retaining the proven 0.2 mm running fit
- Added a guided top transition around the display
- Designed an upper cover around M3 heat-set inserts

### Rendering rework

- Moved the runtime interface to Arduino_GFX
- Replaced full-screen updates with partial RGB565 transfers
- Tuned encoder handling and button response
- Reworked the visual language around white, red and blue
- Replaced unnecessary page arcs with simpler page-specific indicators
- Cleaned startup animation trails and retained the OJM Systems reveal

### Current public baseline

`v0.1.0-poc` is the first coherent version intended for demonstration. The old
local folder names such as `BPv0.2` and `BPv0.3.x` are backups, not public
releases. Future public milestones will be documented in `CHANGELOG.md`.

## Next decision

The final system controlled by the dial will be selected with the project
supervisor. Until then, the four climate pages act as a realistic interaction
demonstrator rather than a claim about the final application.

