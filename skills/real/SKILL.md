---
name: real
description: >-
  Complete guide for base RealMCP (real MCP server, 69 low-level executor tools).
  Use when interacting directly with the Roblox client executor, compiling or
  evaluating Luau code, inspecting instances, hooking metamethods, managing
  client processes, or searching decompiled scripts.
---

# Base RealMCP (`real`)

Low-level executor MCP server providing 69 direct runtime tools for interacting with Roblox client instances, memory, hooks, scripts, and processes.

- **Executable**: `real-mcp.exe`
- **MCP Server Name**: `real`
- **Tool Protocol**: stdio MCP JSON-RPC 2.0

## Routing & Rules

- Always pass `ServerName: "real"` for all 69 base executor tools.
- Read `<appDataDir>\mcp\real\<tool>.json` if inspecting detailed schemas.
- Tools requiring a live injected client will fail if Roblox is not running or not injected; tools like `search-api`, `read-file`, `launch-roblox`, and `list-clients` work offline.

## Base RealMCP Tools Reference (69 Tools)

### Client & Process Management
- **`list-clients`**: List active Roblox executor clients and their connection status.
- **`set-active-client`** (`clientId`): Switch the active executor client session.
- **`list-roblox-windows`**: Enumerate open Roblox client windows and process IDs.
- **`launch-roblox`** (`placeId`, `jobId`): Launch Roblox client to a specific game or server.
- **`inject`**: Inject executor DLL/payload into the active Roblox process.
- **`ping`**: Test MCP server responsiveness and connection health.
- **`get-env`**: Retrieve executor environment variables and version information.
- **`list-accounts`**: List saved Roblox accounts and auth credentials in executor storage.
- **`get-game-info`** (`includeDescription`): Get current PlaceId, GameId, JobId, universe, and place name.
- **`get-diagnostics`**: Inspect executor memory usage, FPS, ping, and internal state.
- **`get-console-output`** (`limit`): Fetch developer console output (prints, warnings, errors).
- **`get-app-log`** (`limit`): Fetch executor application and bootstrap log entries.
- **`screenshot-window`**: Capture screenshot of the active Roblox client window.

### Luau Execution & Input
- **`eval`** (`code`): Evaluate expression and return evaluated Luau value.
- **`execute`** (`code`): Execute Luau script asynchronously in the client context.
- **`execute-file`** (`path`): Execute script file located on disk.
- **`get-data-by-code`** (`code`): Execute script and return serialized return value.
- **`live-reload`** (`label`, `code`): Execute script under a persistent identifier, automatically cleaning up previous instances/connections on re-execution.
- **`search-api`** (`query`): Search supported executor environment functions, globals, and signatures.
- **`start-job`** (`code`): Start long-running asynchronous Luau task.
- **`await-job`** (`jobId`, `timeoutMs`): Wait for background Luau job completion and collect output.
- **`list-jobs`**: List active and completed background Luau jobs.
- **`cancel-job`** (`jobId`): Cancel active background Luau job.
- **`send-input`** (`type`, `keyCode`): Simulate raw mouse/keyboard input events.
- **`type-text-box`** (`instancePath`, `text`): Send simulated text input into a TextBox instance.
- **`click-button`** (`instancePath`): Trigger simulated mouse click on a GuiButton.

### Instances & Data Model
- **`get-descendants-tree`** (`root`, `depth`, `classFilter`): Inspect instance hierarchy tree.
- **`search-instances`** (`query`, `className`, `parent`): Search instances by name, class, or parent path.
- **`get-instance-properties`** (`path`): Dump all readable properties of a specific instance.
- **`get-property`** (`path`, `property`): Read a specific property of an instance.
- **`set-property`** (`path`, `property`, `value`): Modify a property of an instance.
- **`call-method`** (`path`, `method`, `args`): Call an instance method directly (e.g. `TakeDamage`, `Destroy`).
- **`get-tagged`** (`tag`): Return all instances tagged with CollectionService tag.
- **`list-special-instances`** (`type`, `recurseScripts`): List special collections (`"nil"`, `"running"`, `"actors"`).
- **`get-nil-instances`**: Retrieve instances parented to `nil`.
- **`dump-visible-ui`**: Enumerate on-screen UI elements and their absolute screen coordinates.
- **`list-players`**: List players currently in the server with team, character, and userId.

### Script Inspection & Decompilation
- **`build-script-index`**: Build memory index across all decompiled game scripts for instant regex search.
- **`search-scripts`** (`query`): Search indexed scripts by name or path.
- **`script-grep`** (`query`, `caseSensitive`, `maxResults`): Grep decompiled script sources for keywords or remotes.
- **`get-script-content`** (`path`): Decompile and return full Luau source code of a LocalScript or ModuleScript.
- **`get-script-strings`** (`path`): Extract embedded strings and constants from bytecode (useful for obfuscated scripts).
- **`open-script`** (`path`): Open script in external script editor.
- **`check-script`** (`code`): Perform static syntax check on Luau code.
- **`decode-buffer`** (`buffer`, `format`): Decode binary buffer into string or hex format.

### Remotes, Metamethods & Garbage Collection
- **`fire-remote`** (`path`, `method`, `args`): Fire RemoteEvent or invoke RemoteFunction with arguments.
- **`find-remote-callers`** (`remoteName`, `maxResults`): Locate call sites in decompiled scripts that trigger a remote.
- **`remote-spy`** (`operation`, `nameFilter`, `limit`): Intercept, log, block, or ignore outgoing `FireServer`/`InvokeServer` calls.
- **`inbound-spy`** (`operation`, `remoteName`, `path`, `limit`): Hook OnClientEvent listeners to observe incoming server messages.
- **`spy-closure`** (`operation`, `filter`): Trace argument values and callers for a specific Luau closure.
- **`spy-namecall`** (`operation`, `methodFilter`): Intercept and inspect `__namecall` metamethod calls.
- **`check-hook`** (`target`): Verify if a function or metamethod has been modified or hooked.
- **`dump-anticheat-hooks`**: Inspect game-installed metamethod hooks and detections.
- **`list-connections`** (`signalPath`): Inspect signals and connected callback closures.
- **`gc-scan`** (`kind`, `constants`, `keys`, `limit`): High-performance native `filtergc` scanner for tables and functions.
- **`watch-value`** (`path`, `property`, `pollMs`): Monitor property changes over time.
- **`profile-frames`** (`durationFrames`): Profile frame times and script execution delays.
- **`record-session`** (`operation`): Record network traffic and events for playback or offline analysis.

### Persistence, Files & UI Libraries
- **`remember-game`** (`text`, `tags`): Save facts and notes about the current game universe.
- **`recall-game-memory`** (`query`): Query saved knowledge for the current game.
- **`update-game-memory`** (`key`, `value`): Update saved key-value memory records.
- **`forget-game-memory`** (`key`): Delete memory record.
- **`game-feedback`** (`feedback`): Submit feedback or bug report for game compatibility.
- **`read-file`** (`path`): Read file from executor workspace folder.
- **`write-file`** (`path`, `content`): Write file to executor workspace folder.
- **`list-files`** (`folder`): List files in executor workspace folder.
- **`list-ui-libraries`**: Enumerate available UI component libraries (Rayfield, Linoria, etc.).
- **`preview-ui-libraries`** (`name`): Preview sample implementation of a UI library.
- **`get-ui-library`** (`name`): Fetch loader script or source for a specific UI library.
