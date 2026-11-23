# ADR 0002: HUD rendering approach (Qt widgets vs GL-drawn)

Status: Proposed
Date: 2026-11-22

## Context

The game map is rendered with OpenGL inside a `QOpenGLWidget`. On top of it we need a HUD with two kinds of elements:

- **World-space:** up to about 1,000 nameplates (background, name text, HP bar) that move and scale with the map camera.
- **Screen-space:** a 10-slot hotbar and a combat-text strip, fixed to the window.

Two approaches were built in the S2 spike and driven by the same scene data (`HudTestScene`), camera (`Camera`) and layout (`HudLayout`), so the comparison is like for like:

- **A. Qt widgets:** each HUD element is a child `QWidget` of the canvas (`QtHudOverlay`, `NameplateWidget`, `QPushButton`, `QLabel`). Plates are repositioned with `move()` and resized per zoom.
- **B. GL-drawn:** the HUD is rebuilt every frame as textured quads (glyph atlas plus UI atlas) and drawn in two draw calls, one with the camera matrix (world batch) and one with a pixel-space matrix (screen batch). Clicks are resolved by `GlHudInput`.

## Method

- Scripted camera pan (same path every run), vsync off (swap interval 0), zoom 1.
- Metric: total frame time from the tick start to `frameSwapped`, as median and 95th percentile. `paintGL` time is recorded as a secondary figure. Total frame time is the fair metric because approach A does much of its work (moving widgets, repainting them, compositing them over the GL output) outside `paintGL`.
- Each configuration is preceded by a warm-up, and stats are reset after it.
- Environment: Qt 6, desktop OpenGL. <GPU name>, <driver>, <exact Qt version>, <compiler>, <display scale>, <build type: Release>.

## Results

Mode "None" (map only) is the baseline. Each HUD's cost is its row minus the baseline.

| Mode | Nameplates | Frame median (ms) | Frame p95 (ms) | Paint median (ms) | Paint p95 (ms) | Frames |
|---|---|---|---|---|---|---|
| None | 0 | 1.18 | 1.90 | 0.088 | 0.238 | 5504 |
| Qt | 0 | 1.16 | 1.95 | 0.085 | 0.229 | 7989 |
| GL | 0 | 1.28 | 2.01 | 0.185 | 0.384 | 7636 |
| Qt | 50 | 5.70 | 6.36 | 0.140 | 0.264 | 2189 |
| GL | 50 | 1.41 | 2.11 | 0.319 | 0.556 | 6821 |
| Qt | 200 | 10.97 | 12.66 | 0.153 | 0.359 | 1002 |
| GL | 200 | 1.72 | 2.42 | 0.684 | 1.059 | 5877 |
| Qt | 1000 | 65.67 | 72.62 | 0.095 | 0.242 | 301 |
| GL | 1000 | 3.61 | 4.53 | 2.451 | 3.041 | 2888 |

Note: the 1,000-plate rows come from an earlier run than the other rows. <Re-run 1000 in the same session if you want all rows from one run.>

### Reading the results

- **Lowest nameplate count (0 plates):** Qt was slightly faster than GL (1.16 ms vs 1.28 ms median). The gap is about 0.1 ms, well inside the p95 spread of about 2 ms, so the two are effectively tied here. Qt's HUD at zero plates is a dozen mostly static widgets that Qt leaves alone, while GL pays a small fixed cost every frame (rebuilding the vertex list, uploading the buffer, binding state and making two draw calls). That fixed cost shows in GL's paint time, which is double the baseline.
- **As the count increased:** GL changed little, while Qt grew steeply.
  - GL: 1.28 ms (0 plates), 1.41 (50), 1.72 (200), 3.61 (1000). The cost grows slowly and steadily, roughly 2 ms per 1,000 plates.
  - Qt: 1.16 ms (0 plates), 5.70 (50), 10.97 (200), 65.67 (1000). Even 50 plates cost about 4.5 ms, and at 1,000 plates Qt ran at about 15 fps against about 277 fps for GL, roughly 18 times slower.
  - The p95 values sit close to the medians in every row, so the differences are not noise.
- **Why Qt's paint time is misleading:** Qt's `paintGL` time stays around 0.1 ms at every count, because `paintGL` draws only the map in that mode. Nearly all of its extra time is spent outside `paintGL`. This is why total frame time, not paint time, is the headline metric. GL is the opposite: its paint time rises with plate count (0.19 ms to 2.45 ms) because the CPU builds and uploads the vertex list inside `paintGL`, and that is almost all of its added cost.
- **Where Qt's time goes (interpretation, not profiled):**
  - One `move()` call per plate per frame, each invalidating the old and new regions of a widget.
  - Heavy overlap of translucent plates, forcing repeated repainting of the same pixels.
  - Software `QPainter` text and bar drawing for every dirty plate, with nothing cached.
  - Qt compositing the widget tree back over the GL output after `paintGL`.
- **Why GL stays nearly flat:** there are no per-plate objects. All plates, bars and letters (about 12,000 quads at 1,000 plates) go into one buffer and are drawn in two calls. The camera matrix pans and zooms the whole batch on the GPU, so nothing is moved per frame.

## Crispness

The pixel-art crispness checks passed: C1 (map zoom 1, 2, 3) and C2 (display scale). <Record the blurred-pixel counts for each approach and attach the screenshots to `docs/adr/assets/0002/`.>

## Comparison beyond speed

| Concern | A. Qt widgets | B. GL-drawn |
|---|---|---|
| Frame cost at 1,000 plates | 65.7 ms median | 3.6 ms median |
| Scaling with plate count | Grows steeply (about 4.5 ms already at 50 plates) | Grows slowly |
| Camera zoom | Must be applied by hand to every widget (`apply_zoom` resizes and repaints each plate) | Free, from the camera matrix |
| Sub-pixel placement | `move()` takes integers, so fractional pans can make plates jitter by a pixel | Quads can sit between pixels, so plate origins must be rounded manually to keep text crisp |
| Creating or removing plates | One-off hitch: up to 1,000 widgets created or destroyed on a mode or count change | None; the vertex list is rebuilt each frame |
| Mouse routing and hit-testing | Free (Qt delivers clicks to the right widget) | Hand-written (`GlHudInput`, rectangle lookup plus hover state) |
| Hover, tooltips, keyboard focus, text input | Free | Not provided; must be built |
| Accessibility (screen readers) | Provided by Qt widgets | Not provided; a painted HUD is invisible to Narrator unless a parallel accessibility layer is added |
| Text rendering | Qt font engine, any font and any language | Pre-rendered glyph atlas, limited to the characters in it (ASCII 32 to 126 here); other scripts need more work |
| Pixel-art crispness | Passed | Passed |
| Code size | <fill in line counts> | <fill in line counts> |
| Nine-slice frames | <state whether implemented on the Qt side; in this spike the Qt buttons use default styling> | Implemented (9 quads per frame) |

## Decision

Use a **hybrid**:

- **World-space UI (nameplates) is drawn with the GL renderer (approach B).** Approach A did not scale. Even 50 plates cost about 4.5 ms more than the baseline, and 1,000 plates ran at about 15 fps, while B stayed under 4 ms.
- **Screen-space UI (hotbar, combat strip) uses Qt widgets (approach A).** It is only about a dozen widgets, so the cost is negligible (at 0 plates Qt matched the baseline), and Qt provides mouse routing, hover, keyboard focus, tooltips, text input and accessibility for free. These are the elements where the player interacts and where assistive technology matters most.

## Consequences

- We take on the maintenance of a glyph atlas, a quad batcher and a text-layout loop for nameplates.
- The GL renderer's screen-space batch (hotbar, combat strip, hover highlight) and `GlHudInput` were built for the comparison and are not needed in production. They can be removed, or kept for reference.
- Nameplate text is limited to what the glyph atlas contains. Localisation beyond ASCII needs a larger or dynamic atlas.
- Nameplates are not accessible to screen readers. If that matters, they need a separate accessibility path.
- Two rendering systems must be kept visually consistent (fonts, frame art, colours), and the Qt widgets must sit correctly over the GL canvas, including at fractional display scales.
- The world batch and the map must share one camera-matrix function so plates stay locked to tokens.
- At a very low nameplate count (including zero) the two approaches are effectively tied, so B's advantage only appears when many nameplates are on screen.

## Caveats and limits of this spike

- Approach A was built naively: live translucent widgets, nothing cached. Opaque plates, cached text pixmaps, or only updating visible plates would narrow the gap but not remove the per-widget cost. This was not measured.
- The GL result covers only what the spike draws, with a 128×128 atlas and fixed-size 8 px text.
- The 1,000-plate rows come from a separate run from the other rows.