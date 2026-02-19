# CHOC Examples

This directory contains comprehensive examples demonstrating the CHOC library's capabilities. Each example showcases specific functionality with practical, real-world usage patterns.

## Quick Start

```bash
# Build all examples
cmake -B build -S .
cmake --build build

# Run any example
./build/example_name
```

## Examples Overview

### 🎵 **audio_operations**
**Audio buffer manipulation and processing**
- Multi-channel audio buffers with various layouts
- Sample rate conversion and resampling
- Audio file I/O operations
- Real-time audio processing patterns

### 📊 **value_and_json** 
**Dynamic value system and JSON processing**
- Creating and manipulating choc::value objects
- JSON serialization and deserialization
- Type-safe value access and conversion
- Complex nested data structures

### 🔗 **fifo_producer_consumer**
**Thread-safe lock-free data structures**
- Single producer, single consumer FIFO
- Multi-producer, multi-consumer FIFO  
- Memory ordering and synchronization
- High-performance inter-thread communication

### 📁 **file_watcher_example**
**Cross-platform file system monitoring**
- Real-time file change detection
- Directory watching with recursive options
- Event filtering and handling
- Platform-native implementations (CoreServices/inotify)

### ⚡ **xxhash_example**
**Fast hashing with collision resistance testing**
- 32-bit and 64-bit hash variants
- Streaming hash computation
- Seed variation demonstrations
- Hash consistency verification and collision testing

### 🎹 **midi_file_processing**
**Complete MIDI manipulation toolkit**
- MIDI file loading, parsing, and generation
- Note utilities (frequency conversion, names)
- MIDI message construction and analysis
- Sequence building with timing and BPM support

### 🔗 **javascript_integration**
**Embedded JavaScript engine with C++ bindings**
- QuickJS engine integration
- Bidirectional C++/JavaScript communication
- Value marshalling between languages
- Error handling and debugging support

### 🧵 **threading_patterns**
**Advanced concurrent programming patterns**
- Thread-safe containers (FIFO, SpinLock, TaskThread)
- Producer-consumer patterns with backpressure
- Real-time audio thread simulation
- Lock-free data structures and memory ordering

### 📝 **text_processing**
**Comprehensive text manipulation and generation**
- UTF-8 string processing and validation
- Wildcard pattern matching
- HTML document generation with CSS
- Code generation with proper indentation
- Text tables and formatted output
- Float-to-string conversion with precision control

### 🖥️ **webview_desktop_app**
**Cross-platform desktop GUI with embedded browser**
- Native window management
- WebView integration with custom content serving
- JavaScript ↔ C++ communication bridge
- Resource handling and routing
- Modern web UI with responsive design

### 🌐 **http_server** *(Requires Boost)*
**HTTP server with WebSocket support**
- HTTP request/response handling
- WebSocket bidirectional communication
- Static content serving
- Client session management
- *Note: Requires Boost libraries - commented out by default*

## Build Requirements

- **C++17** compatible compiler
- **CMake 3.15+**
- **Platform-specific dependencies:**
  - **macOS**: WebKit, CoreServices frameworks (for WebView/FileWatcher)
  - **Linux**: gtk+-3.0, webkit2gtk-4.1 (for WebView)
  - **Windows**: No additional dependencies for basic functionality

## Platform Support

All examples are designed to work cross-platform:
- ✅ **macOS** (native frameworks)
- ✅ **Linux** (GTK/WebKit) 
- ✅ **Windows** (native APIs)
- ✅ **Build tested** on Apple Silicon and x86_64

## Example Categories

### 🎯 **Core Data Structures**
`value_and_json`, `fifo_producer_consumer`, `threading_patterns`

### 🎵 **Audio & MIDI**  
`audio_operations`, `midi_file_processing`

### 🌐 **Web & Networking**
`webview_desktop_app`, `javascript_integration`, `http_server`

### 📁 **System Integration**
`file_watcher_example`, `text_processing`

### ⚡ **Performance & Utilities**
`xxhash_example`, `threading_patterns`

## Key Features Demonstrated

- **Memory Management**: Lock-free structures, RAII patterns
- **Cross-Platform APIs**: Native implementations with unified interface  
- **Real-Time Performance**: Low-latency audio processing patterns
- **Modern C++**: C++17/20 features, template metaprogramming
- **Web Integration**: Embedded browsers, JavaScript engines
- **File Formats**: MIDI, JSON, HTML generation
- **Concurrency**: Thread-safe containers, producer-consumer patterns
- **Text Processing**: Unicode, pattern matching, code generation

## Usage Patterns

Each example is self-contained and demonstrates:
1. **Setup and initialization** of CHOC components
2. **Core functionality** with practical use cases  
3. **Error handling** and resource management
4. **Performance considerations** and best practices
5. **Integration points** with other systems

The examples serve as both learning resources and production-ready code templates for building applications with the CHOC library.