# RealMCP+ (`real-mcp-plus`) v1.0.1

High-performance composite Model Context Protocol (MCP) server for Roblox reverse engineering, memory scanning, game discovery, and script automation written in pure **C11 & C++20** with native Win32/WinHTTP integration.

Designed to run alongside or on top of `real-mcp.exe` (Real executor MCP runtime) via stdio JSON-RPC 2.0.

---

## What's New in v1.0.1 (Optimizations & Fixes)

- **Worker Thread Reaping**: Proactive cleanup of finished asynchronous tool caller worker threads, eliminating thread accumulation and memory growth during high-frequency requests.
- **Async Concurrent WinHTTP Fetching**: `web-scripts` now runs ScriptBlox and RScripts search queries concurrently in parallel threads using `std::async`.
- **Parallel Game Discovery Enriched Metadata**: `rbxgame-search` executes game details, vote counts/ratings, and icon thumbnail requests asynchronously in parallel streams over WinHTTP.
- **Decompression & Query Preservation**: Added transparent gzip/deflate response decompression support via `WINHTTP_OPTION_DECOMPRESSION` and enhanced URL path extra-info handling.
- **Multi-threaded Recon**: `recon` runs `scac`, `proxscan`, `econscan`, `uiscan`, and `clicheck` in parallel worker tasks.

---

## Architecture & Features

- **No external dependencies**: Pure C11 (`win_bridge.c`) + C++20 (`json.hpp`, `bridge.cpp`, `tools_*.cpp`).
- **Direct Win32 Pipes**: Low-overhead asynchronous RPC communication with `real-mcp.exe`.
- **Native WinHTTP**: Fast TLS/HTTPS client for public web APIs without curl or Python overhead.
- **Embedded Luau Payloads**: 18 optimized byte-level Luau scanners embedded into the binary at compile time.
- **24 High-Level MCP Tools**:
  - **Data Model & Deobfuscation**: `simpledeobf`, `dumper`
  - **Game Discovery & Research**: `rbxgame-search`, `web-scripts`
  - **Recon & Anti-Cheat**: `recon`, `scac`, `uiscan`, `httpscan`, `keyscan`, `hookwatch`
  - **Remotes & Networking**: `remmap`, `remtrace`, `remotesim`, `clicheck`, `netcheck`
  - **World & Interactables**: `proxscan`, `npcscan`, `zonescan`, `toolscan`, `modscan`, `econscan`
  - **Animations & Assets**: `animscan`, `animmake`, `scriptmap`

---

## Building

Requires MinGW-w64 (`g++` & `gcc` supporting C++20/C11) and PowerShell or GNU Make.

### Using PowerShell (Windows)

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1
```

### Using Makefile

```bash
mingw32-make
```

Output binary: `target/release/real-mcp-plus.exe`.

---

## Configuration (`mcp_config.json`)

To use `real-plus` alongside the base `real` MCP server, register both in your MCP settings file:

```json
{
  "mcpServers": {
    "real": {
      "command": "real-mcp",
      "args": []
    },
    "real-plus": {
      "command": "real-mcp-plus",
      "args": ["--only-plus"]
    }
  }
}
```

### Path Resolution

`real-mcp-plus` automatically searches for `real-mcp.exe` in the following order:
1. `--real-mcp <path>` CLI flag.
2. `REAL_MCP_PATH` environment variable.
3. Same directory as `real-mcp-plus.exe`.
4. `%LOCALAPPDATA%\Real\data\mcp\real-mcp.exe`.
5. `%LOCALAPPDATA%\Programs\Real\real-mcp.exe`.
6. `%APPDATA%\Real\real-mcp.exe`.
7. `%ProgramFiles%\Real\real-mcp.exe`.
8. System `PATH`.

---

## Available Tools (22)

| Tool | Category | Description |
|---|---|---|
| `rbxgame-search` | Web & Discovery | Search Roblox experiences across categories (anime, pvp, horror, simulator, etc.) with concurrent players, visits, ratings, and icons |
| `web-scripts` | Web & Discovery | Search ScriptBlox, RScripts, rblxscripts.net, and haxhell.com platforms by query or current place info, with raw script source fetching |
| `recon` | Reconnaissance | Automated single-step full game reconnaissance combining game info, remotes, anti-cheat, UI, interactables, and memory persistence |
| `scac` | Anti-Cheat | Client-side anti-cheat scanner (hidden scripts, tamper hooks, suspicious remotes, and connection flags) |
| `uiscan` | Interface | Audit PlayerGui, StarterGui, and nil instances for hidden or suspicious GUI elements |
| `httpscan` | Network | Scan for HTTP/URL endpoints, Discord webhooks, and HttpService/RequestAsync usage |
| `keyscan` | Security | Scan scripts and garbage collector memory for key systems, whitelists, HWID checks, and link shorteners |
| `hookwatch` | Security | Runtime metamethod/hookfunction tampering detection and anti-cheat hook status |
| `remmap` | Remotes | Comprehensive catalog of RemoteEvents, RemoteFunctions, and risk classifications |
| `remtrace` | Remotes | Find callers, references, and live traffic for target remotes |
| `remotesim` | Remotes | Test-fire or invoke remotes while capturing server inbound responses |
| `clicheck` | Logic | Audit client-trusted values, cooldowns, WalkSpeed/JumpPower checks, and debounce patterns |
| `netcheck` | Physics | Inspect NetworkOwnership of unanchored Workspace parts near the player |
| `proxscan` | World | Scan ProximityPrompts, ClickDetectors, TouchTransmitters, and Seats with distance sorting |
| `npcscan` | World | Scan non-player character Humanoids, health, walkspeeds, and attached prompts |
| `zonescan` | World | Detect zones, teleporters, safezones, doors, and tagged regions |
| `toolscan` | World | Scan player inventory, backpack, and storage tools with weapon logic & connections |
| `modscan` | Scripts | Map ModuleScripts and safely require/dump exported module keys |
| `scriptmap` | Scripts | Map LocalScripts and ModuleScripts (including nil-parented) with decompiler test |
| `econscan` | Economy | Scan currencies, shops, Leaderstats, GamePasses, and purchase prompt scripts |
| `animscan` | Assets | Scan Animation and Sound asset identifiers across character and storage |
| `animmake` | Animation | Create, play, stop, and export keyframe animations applied directly to Motor6D joints |

---

## License

MIT License.
