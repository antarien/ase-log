# ase-log

**Design:** DSGN_016 (AEC//LOGS — Logging Framework)

[![Layer](https://img.shields.io/badge/Layer-1%20Core-green.svg)]()
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)]()

> ECS-integrated logging system built on spdlog

Part of [ASE - Antares Simulation Engine](../../..)

## Overview

`ase-log` provides structured logging for ASE using [spdlog](https://github.com/gabime/spdlog), implementing a 3-axis filtering system that enables precise control over log output during development and production. The three axes are: severity level (trace, debug, info, warn, error, critical), module source (which ase-* module produced the log entry), and category (initialization, tick, network, persistence, etc.). This enables queries like "show only error-level messages from ase-terrain during persistence operations" without drowning in irrelevant output from the 40+ active modules. The logger initializes as an ECS System during the Initialization schedule and cleanly shuts down during the Shutdown schedule, ensuring log file handles are properly flushed. Console output uses color-coded formatting with module prefixes (e.g. `\x1b[38;5;178mTM[data]\x1b[0m`) for visual distinction in development, while file output produces structured JSON for production log aggregation. Every system's on_start() logs info, invalid values trigger warn, and NOT_FOUND conditions produce error — these conventions are enforced by the ECS validator.

## Features

- **ECS Integration**: Logger is a System with proper lifecycle management
- **Dual Output**: Console (colored) and file logging (`logs/antares.log`)
- **Multiple Log Levels**: trace, debug, info, warn, error, critical
- **Format Support**: fmt-style string formatting
- **Client Logging**: Separate logger for browser console forwarding
- **RTC Logging**: Special logging for WebRTC server events

## Installation

### Dependencies

- C++20 compiler
- spdlog (included via CMake)
- ase-ecs (Layer 1)

### CMake Integration

```cmake
add_subdirectory(core/ase-log)
target_link_libraries(your_target PRIVATE ase-log)
```

## Usage

### Basic Logging

```cpp
#include <ase/log/log.hpp>

// Simple string logging
ase::log::info("Server started");
ase::log::warn("Connection timeout");
ase::log::error("Failed to load chunk");

// Formatted logging (fmt style)
ase::log::info("Server listening on port {}", 8090);
ase::log::debug("Loaded {} chunks in {:.2f}ms", count, duration);
ase::log::error("Entity {} not found in registry", entity_id);
```

### Log Levels

```cpp
ase::log::trace("Detailed trace information");
ase::log::debug("Debug information");
ase::log::info("General information");
ase::log::warn("Warning message");
ase::log::error("Error occurred");
ase::log::critical("Critical failure");
```

### Client Logging

Forward browser console logs to server:

```cpp
// Client logs (no [SERVER] prefix)
ase::log::client_info("Browser: Started rendering");
ase::log::client_warn("Browser: Low frame rate");
ase::log::client_error("Browser: WebGL context lost");
```

### RTC Logging

WebRTC server events:

```cpp
ase::log::rtc_info("Client connected: {}", client_id);
ase::log::rtc_warn("ICE candidate timeout for client {}", client_id);
ase::log::rtc_error("DataChannel failed for client {}", client_id);
```

### ECS Integration

The LogSystem is automatically registered as part of the application lifecycle:

```cpp
#include <ase/ecs/app.hpp>
#include <ase/log/log_module.hpp>

ase::ecs::App app;
app.add_module<ase::log::LogModule>();  // Registers LogSystem
app.run();  // LogSystem initializes in Startup schedule
```

### Configuration

```cpp
// Set log level
ase::log::set_level(spdlog::level::debug);

// Flush logs to disk
ase::log::flush();
```

## API Reference

### Logging Functions

```cpp
namespace ase::log {

// Simple string logging
void info(const std::string& msg);
void warn(const std::string& msg);
void error(const std::string& msg);
void debug(const std::string& msg);
void trace(const std::string& msg);
void critical(const std::string& msg);

// Formatted logging (fmt style)
template<typename... Args>
void info(spdlog::format_string_t<Args...> fmt, Args&&... args);

template<typename... Args>
void warn(spdlog::format_string_t<Args...> fmt, Args&&... args);

template<typename... Args>
void error(spdlog::format_string_t<Args...> fmt, Args&&... args);

// Client logging (browser console)
void client_info(const std::string& msg);
void client_warn(const std::string& msg);
void client_error(const std::string& msg);

// RTC server logging
void rtc_info(const std::string& msg);
void rtc_warn(const std::string& msg);
void rtc_error(const std::string& msg);

// Configuration
void set_level(spdlog::level::level_enum level);
void flush();

}  // namespace ase::log
```

### LogSystem Class

```cpp
class LogSystem : public ecs::System {
    const char* name() const override;
    void on_start(ecs::Registry& registry) override;
    void on_stop(ecs::Registry& registry) override;
    void tick(ecs::Registry& registry, float dt) override;

    // Configuration (call before on_start)
    void set_name(const std::string& name);
    void set_log_file(const std::string& path);

    // Access underlying loggers
    static std::shared_ptr<spdlog::logger>& logger();
    static std::shared_ptr<spdlog::logger>& client_logger();
};
```

## Architecture

### Log Output Format

**Server logs**:
```
[2025-12-23 14:32:10.123] [ASE] [SERVER] [info] Server started on port 8090
```

**Client logs** (forwarded from browser):
```
[2025-12-23 14:32:11.456] [ASE] [info] Browser: Started rendering
```

**RTC logs**:
```
[2025-12-23 14:32:12.789] [ASE] [SERVER] [RTC] [info] Client connected: 42
```

### File Output

All logs are written to:
```
logs/antares.log
```

Monitor in real-time:
```bash
tail -F logs/antares.log
```

`-F`, not `-f`: log files ROTATE at the quota (`logs/quota.conf`, default 50 MiB with 3 kept
generations). Rotation renames the file and opens a new one at the same path, so `tail -f` — which
follows the file DESCRIPTOR — keeps reading the renamed `…1.log` and goes silent without any error.
`-F` follows the NAME and picks the new file up. Silence from a `-f` monitor means rotation, not
"nothing happened".

### Lifecycle

1. **on_start()**: Creates spdlog instances, sets up file/console sinks
2. **tick()**: No-op (logging is immediate)
3. **on_stop()**: Flushes and closes log files

## Dependencies

- **External**: spdlog, fmt
- **Internal**: ase-ecs (Layer 1)

## License

MIT License - Part of ASE (Antares Simulation Engine)
