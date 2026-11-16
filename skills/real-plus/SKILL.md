---
name: replus
description: >-
  Complete guide for RealMCP+ (real-plus MCP server, 25 high-level tools). Use
  when scanning/reversing a Roblox game, running scripts, tracing remotes,
  testing animations/physics, searching ScriptBlox/RScripts, searching games
  across categories, auditing client-side anti-cheat, deobfuscating Luau scripts,
  unpacking bytecode constants/substitution tables, or dumping data model hierarchies.
---

# RealMCP+ (`real-plus`) v1.0.5

High-level composite MCP server providing 25 specialized tools for Roblox reverse engineering, memory scanning, world auditing, script automation, bytecode unpacking, deobfuscation, and data model dumping.

- **Executable**: `real-mcp-plus.exe --only-plus`
- **MCP Server Name**: `real-plus`
- **Tool Protocol**: stdio MCP JSON-RPC 2.0

## Routing & Rules

- Always pass `ServerName: "real-plus"` for all 25 tools.
- Never write print statements, notifications, or toasts in generated Luau code.
- Read `<appDataDir>\mcp\real-plus\<tool>.json` if inspecting detailed schemas.

## RealMCP+ Tools Reference (25 Tools)

### Reconnaissance & Anti-Cheat
- **`recon`** (`depth`, `remember`): One-shot automated game reconnaissance combining game info, remotes, anti-cheat, UI, interactables, and memory persistence.
- **`scac`** (`includeConnections`, `checkRemotes`, `includeBuiltinScan`): Full client-side anti-cheat scanner across scripts, signals, metatable hooks, and suspicious remotes.
- **`uiscan`** (`includeNil`, `includeCoreGui`, `onlySuspicious`, `checkConnections`, `maxItems`): Inspect PlayerGui, StarterGui, CoreGui, and nil for hidden/off-screen UI, admin panels, and button connections.
- **`httpscan`** (`scanInstances`, `hookRequests`, `grepScripts`): Scan ValueBases and decompiled scripts for URLs, Discord webhooks, and HttpService/RequestAsync calls.
- **`keyscan`** (`includeGc`, `maxMatches`): Scan scripts and GC memory for key systems, whitelists, HWID checks, and link shorteners.
- **`hookwatch`** (`action`, `includeBuiltin`): Runtime metamethod/hookfunction tampering detection and anti-cheat hook status.

### Remotes, Logic & Economy
- **`remmap`** (`includeNil`, `includeBindables`, `filter`, `maxItems`): Discover and catalog RemoteEvents, RemoteFunctions, and risk classifications (admin, economy, combat, movement, anticheat).
- **`remtrace`** (`remoteName`, `remotePath`, `spyDurationSec`, `maxCallers`): Find calling scripts, grep references, and capture live outgoing traffic for a specific remote.
- **`remotesim`** (`remotePath`, `method`, `args`, `captureInbound`, `captureSec`): Safely test-fire or invoke a Remote while capturing server inbound responses.
- **`clicheck`** (`includeGc`, `includeGrep`, `maxItems`): Audit client-trusted values, cooldowns, WalkSpeed/JumpPower checks, and debounce patterns.
- **`modscan`** (`filter`, `requireModules`, `maxItems`): Map ModuleScripts across ReplicatedStorage and Player containers; safely require and dump exported keys.
- **`econscan`** (`includeGrep`, `maxItems`): Scan currencies, shops, Leaderstats, GamePasses, and purchase prompt scripts.
- **`toolscan`** (`includeStorage`, `checkConnections`, `maxItems`): Inspect Tool instances in Backpack, Character, StarterPack, and ReplicatedStorage.

### World, Physics & Animations
- **`proxscan`** (`includePrompts`, `includeClicks`, `includeTouch`, `includeSeats`, `checkConnections`, `sortByDistance`, `maxItems`): Scan interactive world triggers sorted by distance.
- **`npcscan`** (`aliveOnly`, `sortByDistance`, `maxItems`): Scan non-player character Humanoid models with HP, coordinates, distance, and attached prompts.
- **`zonescan`** (`maxItems`): Map SpawnLocations, CollectionService tags, teleporters, safezones, doors, and tagged regions.
- **`netcheck`** (`targetPath`, `unanchoredOnly`, `maxItems`): Inspect NetworkOwnership (`isnetworkowner`, ReceiveAge) of unanchored parts near player.
- **`animscan`** (`includeSounds`, `maxItems`): Scan Animation and Sound asset identifiers across character, backpack, and storage.
- **`animmake`** (`action`, `animId`, `loop`, `speed`, `keyframes`): Create, play, stop, or export keyframe animations applied directly to Motor6D joints.
- **`scriptmap`** (`includeNil`, `testDecompile`, `filter`, `maxItems`): Inventory of all LocalScripts and ModuleScripts with decompiler readability check.

### Deobfuscation, Bytecode & Data Model Dumping
- **`simpledeobf`** (`script`, `mode`, `extractStrings`, `extractUrls`): Deobfuscate and analyze protected Luau scripts. Modes: `auto`, `wearedevs`, `prometheus`, `ironbrew`, `moonsec`, `proto-unpack`, `generic`. Automatically recognizes WeAreDevs (`https://wearedevs.net/obfuscator`), Prometheus V2 (`https://github.com/0x251/Prometheus-DeobfuscatorV2`), IronBrew 2 (IB2 / Luraph legacy), and MoonSec V2/V3 (with automatic XOR key recovery), folded character arrays, hex/byte decoding, and extracts hidden endpoints, webhooks, referenced services, and IOCs. Operates fully offline without requiring an active Roblox client.
- **`proto-unpack`** (`script`, `inlineConstants`, `decodeBytecode`, `recoverXor`, `foldBuiltins`, `extractUrls`, `extractIOCs`): Deep bytecode constants and substitution table unpacker. Automatically parses Lua constant tables, inlines table-wrapper functions (`fn(idx)` / `tbl[idx]`), unpacks VM constant pools, auto-recovers single-byte and multi-byte XOR encryption keys, folds builtin operations (`string.reverse`, `string.sub`, `string.char`, `table.concat`), and extracts network IOCs, Roblox services, and Remotes. Operates fully offline.
- **`dumper`** (`target`, `maxDepth`, `saveToFile`): Recursively dump Roblox DataModel hierarchies (ReplicatedStorage, StarterGui, StarterPlayer, Workspace) with attributes, Remote identifiers, and ValueBases into structured JSON for offline analysis or direct disk export.

### Web Search & Game Discovery
- **`rbxgame-search`** (`query`, `category`, `includeVotes`, `includeThumbnails`, `maxResults`): Search Roblox games across curated categories (`anime`, `pvp`, `horror`, `simulator`, `shooter`, `rpg`, `obby`, `survival`, `driving`, `roleplay`, `popular`, `all`) with live concurrent players, visits, ratings, and icon URLs.
- **`web-scripts`** (`query`, `platform`, `mode`, `useCurrentGame`, `fetchSource`, `maxResults`, `page`): Search ScriptBlox, RScripts, rblxscripts.net, and haxhell.com for scripts, with optional `fetchSource` for raw script contents. Supported platforms: `all`, `scriptblox`, `rscripts`, `rblxscripts`, `haxhell`. Reference: `robloxscripts.com`.
