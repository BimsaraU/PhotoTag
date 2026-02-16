# PhotoTag

A high-performance batch image tagging and watermarking tool for Windows.

## Features

- **Batch Processing**: Process entire folders of images at once.
- **Smart Tagging**: Automatically applies portrait or landscape watermarks based on image orientation.
- **Precision Controls**: Adjust tag scale, position (X/Y), and opacity with visual feedback.
- **Grid Snapping**: Use a customizable grid for perfect alignment.
- **Background Effects**: Optional background blur to emphasize the tag or subject.
- **Performance**: Hardware-accelerated rendering using DirectX 11.
- **Modern UI**: Clean, dark-themed interface powered by Dear ImGui.

## Usage

1. **Select Folders**: Choose your source folder containing images and an output folder for the processed files.
2. **Load Watermarks**: Select watermark images for both portrait and landscape orientations.
3. **Adjust Settings**: Use the preview window to position and scale your tag. Enable grid snapping for precision.
4. **Export**: Click "Export All" to process all images in the source folder.

## Installation

Download the latest release from the [Releases](https://github.com/BimsaraU/PhotoTag/releases) page or build from source.

### Building from Source

This project is a Visual Studio solution using C++14 and DirectX 11.

1. Open `PhotoTag.sln` in Visual Studio.
2. Build the solution in `Release` / `x64` configuration.
3. The executable will be in `x64/Release/PhotoTag.exe`.

## Dependencies

- [Dear ImGui](https://github.com/ocornut/imgui) for the user interface.
- GDI+ for image loading and saving.
- DirectX 11 for hardware acceleration.
