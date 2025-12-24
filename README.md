# Game-Away

![Tech Stack](https://skillicons.dev/icons?i=cpp,windows,cmake)

**Keyboard & Mouse Input Mirroring for Windows**

Mirror your keyboard and mouse input from one Windows PC to another over a network connection. Ideal for controlling multiple machines simultaneously or gaming across systems.

> **Note:** This project is under active development and may contain minor bugs.

---

## Features

-   **Real-Time Input Capture** – Mirrors keyboard and mouse events with low latency
-   **Token-Based Authentication** – Secure connection approval system
-   **Pause/Resume Control** – Toggle mirroring with `Ctrl+Shift+P`
-   **WebSocket Communication** – Fast, bidirectional data transfer
-   **Lightweight Console Interface** – Minimal resource footprint

---

## Network Requirements

Game-Away currently supports **local network connections** (e.g., `192.168.x.x`).

For remote connections over the internet, I recommend using [Tailscale](https://tailscale.com/download) to create a secure virtual private network.

---

## Quick Start (End Users)

1. Download the latest release (`GameAway.zip`)
2. Extract the ZIP file to any location
3. Run `GameAway.exe`
4. Select your mode:
    - **Server (1)**: The PC that will _receive_ input
    - **Client (2)**: The PC that will _send_ input
5. On the client, enter the server's IP address and the connection token displayed on the server

### Controls

| Hotkey         | Action                       |
| -------------- | ---------------------------- |
| `Ctrl+Shift+P` | Pause/Resume input mirroring |
| `Ctrl+C`       | Exit the application         |

---

## Building from Source (Developers)

### Prerequisites

-   Windows 10/11
-   [Git](https://git-scm.com/)
-   [CMake 3.15+](https://cmake.org/)
-   [MSYS2/MinGW-w64](https://www.msys2.org/)
    -   Install the `ucrt64` toolchain: `pacman -S mingw-w64-ucrt-x86_64-gcc`

### Build Steps

```bash
# Clone the repository
git clone https://github.com/your-username/Game-Away.git
cd Game-Away

# Run the build script
build.bat
```

The build script will:

1. Clone and bootstrap vcpkg (if not present)
2. Install dependencies (`ixwebsocket`, `nlohmann-json`)
3. Configure and build the project

### Creating a Distribution Package

```bash
package.bat
```

This generates a `GameAway/` folder with all required files ready for distribution.

### Project Structure

```
Game-Away/
├── src/
│   ├── main.cpp           # Entry point
│   ├── config.hpp         # Configuration constants
│   ├── server/            # Server implementation
│   ├── client/            # Client implementation
│   └── utils/             # Utility functions
├── build.bat              # Build automation script
├── package.bat            # Packaging script
├── CMakeLists.txt         # CMake configuration
└── vcpkg.json             # Dependencies manifest
```

---

## Dependencies

-   [IXWebSocket](https://github.com/machinezone/IXWebSocket) – WebSocket client/server library
-   [nlohmann/json](https://github.com/nlohmann/json) – JSON parsing library

---

## Contributing

Contributions are welcome. Please feel free to submit issues or pull requests.

---

Made with ❤️ for fun.
