<div align="center">

# PhotoTag

**Batch image tagging and watermarking for Windows.**

Drop in a folder, place your watermark once, export the whole set.

[![Download PhotoTag](https://img.shields.io/badge/Download-PhotoTag.exe-11c2ed?style=for-the-badge&logo=windows&logoColor=white)](bin/PhotoTag.exe)

![Platform](https://img.shields.io/badge/platform-Windows%20x64-1a252d?style=flat-square)
![Language](https://img.shields.io/badge/C%2B%2B-14-1a252d?style=flat-square&logo=cplusplus)
![Renderer](https://img.shields.io/badge/DirectX-11-1a252d?style=flat-square)
![UI](https://img.shields.io/badge/UI-Dear%20ImGui-1a252d?style=flat-square)
![Dependencies](https://img.shields.io/badge/runtime%20deps-none-1a252d?style=flat-square)

</div>

---

## Why

Watermarking a shoot one file at a time is slow, and most batch tools make you guess
where the tag will land. PhotoTag shows you the real composition while you drag, then
applies that exact placement across every image in the folder — switching between your
portrait and landscape watermark automatically as orientation changes.

## Features

| | |
|---|---|
| **Batch processing** | Point at a folder, export the whole set in one pass. |
| **Orientation-aware tagging** | Portrait images get the portrait watermark, landscape gets landscape. Falls back to whichever is loaded if only one is set. |
| **Direct manipulation** | Drag the tag to move it. Drag the corner handle to resize. The preview is the editor. |
| **Grid snapping** | 2–20 grid lines with an optional safe-margin guide for consistent placement. |
| **Background blur** | Separable Gaussian blur behind the tag, with independent strength and opacity. |
| **Fullscreen preview** | Collapse the sidebar to judge the composition at full size. |
| **Non-blocking export** | Export runs on a worker thread — the UI stays responsive, with live progress. |
| **Hardware accelerated** | DirectX 11 rendering, GDI+ for decode and encode. |
| **Single executable** | Statically linked. No installer, no runtime to chase down. |

## Install

Click the download badge above, or grab the latest build from the
[Releases](https://github.com/BimsaraU/PhotoTag/releases) page.

`PhotoTag.exe` is a standalone x64 binary — no installation, no Visual C++ redistributable required.

## Usage

**1. Select folders**
Choose a source folder of images and an output folder for the results.

**2. Load watermarks**
Pick a portrait watermark and a landscape watermark. Setting just one is fine; it will be used for both orientations.

**3. Compose**
Hit **Load Images**, then drag the tag into place. Use the corner handle to resize, or the sliders for exact values. Turn on **Snap Grid** for alignment, **Enable Blur** to lift the tag off a busy background.

**4. Check your work**
Step through the set with **<< Prev** and **Next >>** to see the placement against both orientations before committing.

**5. Export**
Click **Export All**. Files are written to the output folder with a `Tagged_` prefix, leaving your originals untouched.

## Controls

| Control | Range | Effect |
|---|---|---|
| Scale | 0.01 – 1.00 | Tag width as a fraction of image width. Height follows the tag's aspect ratio. |
| X / Y | 0.00 – 1.00 | Tag centre position. Clamped so the tag never overhangs the edge. |
| Tag Opacity | 0.00 – 1.00 | Blend strength of the watermark. |
| Snap Grid | on / off | Snaps position to grid intersections while dragging. |
| Grid Lines | 2 – 20 | Grid density. |
| Safe Margin | 0.00 – 0.50 | Draws an inset guide rectangle. Visual aid only. |
| Enable Blur | on / off | Blurs the image region behind the tag. |
| Blur Strength | 2 – 64 | Blur radius. |
| Blur Opacity | 0.00 – 1.00 | How strongly the blur is mixed over the original. |

> **Note** — Y is shown flipped in the sidebar, so `1.00` is the top of the image.

## Details

<details>
<summary><b>Supported formats</b></summary>

Input: `.jpg`, `.jpeg`, `.png`, `.bmp` — scanned non-recursively from the source folder.
Watermarks: any format GDI+ can decode; use PNG for transparency.
Output: JPEG at quality 92.

</details>

<details>
<summary><b>How the preview stays fast</b></summary>

Images wider or taller than 1920px are downscaled to a proxy for on-screen display, so
large RAW-sized exports don't fill VRAM while you compose. The full-resolution original
is released as soon as the proxy exists.

Export ignores the proxy entirely and works from the original file, so output resolution
always matches the input.

</details>

<details>
<summary><b>Blur implementation</b></summary>

The blur is a separable box blur run in multiple passes, which approximates a true
Gaussian at a fraction of the cost. During export the source is first downscaled to at
most 512px on its long edge, blurred there, then scaled back up — the radius is scaled to
match, so the result is visually equivalent to blurring at full size but far cheaper.

</details>

<details>
<summary><b>Building from source</b></summary>

Requires Visual Studio 2022 (toolset v143) with the Desktop C++ workload and the Windows SDK.

```
1. Open PhotoTag.sln
2. Select the Release / x64 configuration
3. Build
```

The binary lands in `x64/Release/PhotoTag.exe`. Release builds link the static runtime
(`/MT`) so the executable runs on machines without the Visual C++ redistributable.

</details>

## Built with

- [Dear ImGui](https://github.com/ocornut/imgui) — immediate-mode user interface
- **Direct3D 11** — hardware-accelerated preview rendering
- **GDI+** — image decoding, compositing, and JPEG encoding

<div align="center">
<sub>Built for Windows · C++14 · No runtime dependencies</sub>
</div>
