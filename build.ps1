$ErrorActionPreference = "Stop"

$env:Path = "$env:LOCALAPPDATA\Microsoft\WinGet\Links;$env:Path"

New-Item -ItemType Directory -Force -Path "build", "target\release" | Out-Null

& g++ -std=c++20 -O2 tools\embed_luau.cpp -o build\embed_luau.exe
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& .\build\embed_luau.exe src\payloads src\cpp\payloads_gen.hpp
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& gcc -std=c11 -O2 -Wall -Wextra -c src\c\win_bridge.c -o build\win_bridge.o
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$cppFiles = @(
    "main", "bridge", "tools_registry",
    "tools_recon", "tools_remotes", "tools_world", "tools_web", "tools_deobf"
)

$objs = @("build\win_bridge.o")
foreach ($f in $cppFiles) {
    $obj = "build\$f.o"
    & g++ -std=c++20 -O2 -Wall -Wextra -pthread -c "src\cpp\$f.cpp" -o $obj
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $objs += $obj
}

$outNew = "build\real-mcp-plus-new.exe"
& g++ @objs -o $outNew -pthread -lwinhttp -static-libgcc -static-libstdc++
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$dest = "target\release\real-mcp-plus.exe"
if (Test-Path $dest) {
    Remove-Item -Force "$dest.old" -ErrorAction SilentlyContinue
    Move-Item -Force $dest "$dest.old" -ErrorAction SilentlyContinue
}
Copy-Item -Force $outNew $dest
Remove-Item -Recurse -Force "build" -ErrorAction SilentlyContinue
Write-Host "Built $dest successfully."
