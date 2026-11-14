CXX ?= g++
CC ?= gcc
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -pthread
CFLAGS = -std=c11 -O2 -Wall -Wextra
LDFLAGS = -pthread -lwinhttp -static-libgcc -static-libstdc++

BUILD_DIR = build
OUT_NEW = $(BUILD_DIR)/real-mcp-plus-new.exe
DEST_EXE = target/release/real-mcp-plus.exe

CPP_SRCS = src/cpp/main.cpp src/cpp/bridge.cpp src/cpp/tools_registry.cpp \
           src/cpp/tools_recon.cpp src/cpp/tools_remotes.cpp \
           src/cpp/tools_world.cpp src/cpp/tools_web.cpp src/cpp/tools_deobf.cpp
CPP_OBJS = $(patsubst src/cpp/%.cpp,$(BUILD_DIR)/%.o,$(CPP_SRCS))
C_OBJ = $(BUILD_DIR)/win_bridge.o
LUAU_FILES = $(wildcard src/payloads/*.luau)

.PHONY: all clean

all: $(OUT_NEW)
	powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path 'target/release' | Out-Null; if (Test-Path '$(DEST_EXE)') { if (Test-Path '$(DEST_EXE).old') { Remove-Item -Force '$(DEST_EXE).old' -ErrorAction SilentlyContinue }; Move-Item -Force '$(DEST_EXE)' '$(DEST_EXE).old' -ErrorAction SilentlyContinue }; Copy-Item -Force '$(OUT_NEW)' '$(DEST_EXE)'"

$(BUILD_DIR):
	powershell -NoProfile -Command "New-Item -ItemType Directory -Force -Path '$(BUILD_DIR)' | Out-Null"

$(BUILD_DIR)/embed_luau.exe: tools/embed_luau.cpp | $(BUILD_DIR)
	$(CXX) -std=c++20 -O2 tools/embed_luau.cpp -o $@

src/cpp/payloads_gen.hpp: $(BUILD_DIR)/embed_luau.exe $(LUAU_FILES)
	./$(BUILD_DIR)/embed_luau.exe src/payloads src/cpp/payloads_gen.hpp

$(C_OBJ): src/c/win_bridge.c src/c/win_bridge.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/cpp/%.cpp src/cpp/payloads_gen.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUT_NEW): $(C_OBJ) $(CPP_OBJS)
	$(CXX) $(C_OBJ) $(CPP_OBJS) -o $@ $(LDFLAGS)

clean:
	powershell -NoProfile -Command "Remove-Item -Recurse -Force '$(BUILD_DIR)' -ErrorAction SilentlyContinue"
