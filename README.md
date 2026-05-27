# Layerhost

Layerhost is a modern, cross-platform Digital Audio Workstation (DAW) built with [JUCE 8](https://juce.com/). It features infinite track counts, a sleek mixer, a full-featured piano roll, and robust CLAP plugin hosting capabilities.

![Layerhost Interface](https://raw.githubusercontent.com/samplaman/layerhost/main/screenshot.png)

## Features

- **Infinite Track Counts:** Dynamically add as many tracks as your machine can handle, with smooth scrolling through the arrangement and mixer views.
- **Advanced Automation:** Multi-lane parameter automation (e.g., Volume and Pan) seamlessly integrated directly onto the tracks.
- **CLAP Plugin Hosting:** Experimental support for the CLAP (Clever Audio Plug-in) standard, bringing next-generation audio plugin capabilities to the DAW.
- **Cross-Platform:** Built with C++ and JUCE, fully compatible with Windows, macOS, and Linux out of the box.

## Building from Source

Layerhost uses standard CMake (version 3.22+) to manage its build process and gracefully handles dependency fetching (including JUCE and CLAP headers).

```bash
# Configure the project
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build the executable
cmake --build build --config Release -j4
```

The resulting binary will be located in `build/layerhost_artefacts/Release/` or `build/layerhost_artefacts/`.

## Continuous Integration

Every push to the `main` branch automatically builds and packages the application for Windows, macOS, and Linux using GitHub Actions, providing a continuous `latest` release tag.

## License

This project is licensed under the MIT License.
