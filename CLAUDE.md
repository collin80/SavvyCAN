# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Development Commands

### Building SavvyCAN
- **Standard build**: `qmake && make`
- **Release build**: `qmake CONFIG+=release && make`
- **Debug build**: `qmake CONFIG+=debug && make`
- **Clean rebuild**: `make clean && qmake && make`
- **Requirements**: Qt 5.14+ with modules: core, gui, widgets, serialbus, serialport, qml, help, network, opengl

### Running Tests
Navigate to `/test` directory:
```bash
cd test
qmake test.pro && make && ./test
```

### Platform-Specific Packaging
- **Linux AppImage**: Build, then use `linuxdeployqt`
- **macOS DMG**: Build, then use `macdeployqt`
- **Windows**: Build with Visual Studio 2022, creates portable executable

### Version Management
When committing fixes, increment the patch version in `/config.h` (currently VERSION 221)

## Architecture Overview

### Core Data Flow
1. **CAN Hardware** → `CANConnection` implementations → Lock-free queues → `CANFrameModel` → UI Display
2. **Log Files** → `FrameFileIO` (12+ formats) → `CANFrameModel` → Analysis Windows
3. **DBC Files** ↔ `DBCHandler` ↔ Frame interpretation

### Key Components

**Connection Layer** (`/connections/`)
- Base class: `CANConnection` with lock-free queue (`LFQueue`)
- Manager: `CANConManager` singleton manages all connections
- Factory: `CANConFactory` creates specific connection types
- Implementations: GVRET serial, SocketCAN, Lawicel, MQTT, Qt SerialBus plugins

**Frame Model** (`/canframemodel.h`)
- Central data store for all CAN frames
- Manages frame timestamps, filtering, and sorting
- Emits signals for real-time updates

**Main Window** (`/mainwindow.h`)
- Hub for all sub-windows and frame routing
- Manages filter observers for targeted frame delivery
- Uses `killEmAll()` for cleanup of child windows

**Protocol Support** (`/bus_protocols/`)
- ISO-TP: Multi-frame message assembly/disassembly
- J1939: Heavy vehicle protocol with PGN support
- UDS: Diagnostic protocol implementation

**Analysis Tools** (`/re/`)
- Each tool extends `QDialog` and registers as frame observer
- Tools include: GraphingWindow, FlowViewWindow, SnifferWindow, FuzzingWindow

### Important Patterns

**Frame Handling**
- `CANFrame` extends Qt's `QCanBusFrame` with additional metadata
- Frames stored in `QVector<CANFrame>` containers
- Timer-based GUI updates to avoid overwhelming the UI

**File I/O**
- `FrameFileIO` supports: CRTD, CSV, generic, BusMaster, Microchip, Vector, PCAN, and more
- Auto-detection of file format based on content/extension

**DBC Support**
- Full parsing and editing of CAN database files
- Signal interpretation integrated with frame display
- Multiple editor dialogs for nodes, messages, and signals

**Memory Management**
- Qt parent-child ownership for UI components
- Explicit cleanup in destructors for non-Qt resources
- Frame buffers have configurable size limits

### CI/CD Workflow
GitHub Actions automatically builds for Linux/macOS/Windows on commits to master. Check `.github/workflows/build.yml` for details.