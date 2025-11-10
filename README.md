# Blu-ray Ripper TUI

A terminal user interface for ripping Blu-ray discs using MakeMKV and HandBrake.

## Features

- Automatic optical drive detection
- Interactive title selection
- Real-time progress monitoring
- MakeMKV integration for disc ripping
- HandBrake integration for video encoding
- Clean TUI built with FTXUI

## Prerequisites

### System Requirements
- Linux (tested on NixOS)
- MakeMKV (with `makemkvcon` CLI)
- HandBrake CLI (`HandBrakeCLI`)
- C++20 compatible compiler

### Installing Dependencies

#### On NixOS (recommended):
```bash
nix develop
```

#### On Ubuntu/Debian:
```bash
sudo apt install cmake g++ makemkv handbrake-cli
```

## Building

### With Nix Flakes:
```bash
nix build
./result/bin/bluray-ripper
```

### With CMake:
```bash
cmake -B build
cmake --build build
./build/bluray-ripper
```

## Project Structure

```
bluray-ripper/
├── CMakeLists.txt          # Build configuration
├── flake.nix               # Nix flake for reproducible builds
├── include/
│   ├── disc_detector.h     # Optical drive detection
│   ├── makemkv_wrapper.h   # MakeMKV subprocess wrapper
│   ├── handbrake_wrapper.h # HandBrake subprocess wrapper
│   └── ui/
│       └── main_ui.h       # Main UI component
├── src/
│   ├── main.cpp            # Entry point
│   ├── disc_detector.cpp
│   ├── makemkv_wrapper.cpp
│   ├── handbrake_wrapper.cpp
│   └── ui/
│       └── main_ui.cpp
└── README.md
```

## Usage

1. Insert a Blu-ray disc into your optical drive
2. Run the application: `./bluray-ripper`
3. Press `r` to scan for discs
4. Use arrow keys to select a disc
5. Press `Enter` to load titles
6. Use `Space` to toggle title selection
7. Press `s` to start ripping
8. Press `q` to quit

## Keyboard Controls

- `q` - Quit application
- `r` - Rescan for optical drives
- `s` - Start ripping selected titles
- `e` - Start encoding (when MKV files are ready)
- `Space` - Toggle title selection
- Arrow keys - Navigate menus

## Technical Details

### MakeMKV Integration
The application spawns `makemkvcon` as a subprocess and parses its output for:
- Disc information (titles, duration, size)
- Ripping progress (percentage, current file)
- Status messages

### HandBrake Integration
HandBrake CLI is executed with `--json` flag for structured output:
- Encoding progress (percentage, FPS, ETA)
- Quality metrics
- Completion status

### UI Framework
Built with FTXUI, which provides:
- Reactive UI components
- Event-driven input handling
- Cross-platform terminal rendering
- Modern C++ API

## Future Enhancements

- [ ] SQLite database for tracking ripped discs
- [ ] Queue management (batch processing)
- [ ] Custom HandBrake presets
- [ ] Audio track and subtitle selection
- [ ] Automatic file naming based on disc metadata
- [ ] Integration with media server (Jellyfin, Plex)
- [ ] Configuration file support
- [ ] Retry failed operations
- [ ] Disc ejection after completion

## Development

### Adding New Features
The codebase is structured for easy extension:
- Add new wrappers in `include/` and `src/`
- Extend UI in `src/ui/main_ui.cpp`
- UI state machine in `AppState` enum

### Debugging
With Nix:
```bash
nix develop
gdb ./build/bluray-ripper
```

### Code Style
- C++20 features encouraged
- Modern STL (ranges, concepts where appropriate)
- RAII for resource management
- Clear separation of concerns

## License

MIT License - See LICENSE file for details

## Contributing

Contributions welcome! Please ensure:
- Code compiles with C++20
- Follows existing code style
- Tests with actual MakeMKV/HandBrake installation
- Updates README for new features

## Troubleshooting

### "MakeMKV not found"
Ensure `makemkvcon` is in your PATH:
```bash
which makemkvcon
```

### "HandBrakeCLI not found"
Ensure HandBrake CLI is installed:
```bash
which HandBrakeCLI
```

### "No optical drives found"
Check `/dev/sr*` devices exist and you have read permissions:
```bash
ls -la /dev/sr*
```

### Permission denied errors
Add your user to the `cdrom` group:
```bash
sudo usermod -a -G cdrom $USER
```
Then log out and back in.

## Acknowledgments

- [FTXUI](https://github.com/ArthurSonzogni/FTXUI) - Excellent TUI library
- [MakeMKV](https://www.makemkv.com/) - Blu-ray ripping tool
- [HandBrake](https://handbrake.fr/) - Video transcoding tool
