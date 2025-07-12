# Project settings
TARGET = 0xdead-type
SRC = game.c

# Native build settings
CC = gcc
CFLAGS = $(shell pkg-config --cflags raylib)
LDFLAGS = $(shell pkg-config --libs raylib) -lm -lpthread -ldl

# WebAssembly (Emscripten) settings
EMCC = emcc
RAYLIB_PATH = ./raylib
EMFLAGS = -O3 -Wall -DPLATFORM_WEB -s USE_GLFW=3 -s ASYNCIFY --preload-file assets
INCLUDE = -I$(RAYLIB_PATH)/src
LIBS = $(RAYLIB_PATH)/src/libraylib.a -s USE_GLFW=3 -s ASYNCIFY -s TOTAL_MEMORY=67108864 -s ALLOW_MEMORY_GROWTH=1


# Default rule (build for native)
.DEFAULT: build

build: $(TARGET)

# Run the native build
run: $(TARGET)
	./$(TARGET)

# Build for native (Linux/macOS)
$(TARGET): $(SRC)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

# Build for WebAssembly (Emscripten)
web: $(SRC)
	$(EMCC) -o web.html $^ $(LIBS) $(INCLUDE) $(EMFLAGS)

# Run WebAssembly build locally
serve: web
	emrun --no_browser --port 8080 .

# Clean rule
clean:
	rm -f $(TARGET) web.html index.js index.wasm
	rm -rf 0xdead-type/

# Package native build
bundle: build
	rm -rf 0xdead-type/ && rm -f $(TARGET).zip
	mkdir -p 0xdead-type && cp $(TARGET) assets/*.wav assets/vcr.ttf 0xdead-type/
	zip -r $(TARGET).zip 0xdead-type
	rm -rf 0xdead-type/

.PHONY: all clean web serve bundle
