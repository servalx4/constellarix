CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf glew libcurl)
LDFLAGS = $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf glew libcurl) -lGL
# Partial static: C++ runtime static, system libs dynamic (more portable)
LDFLAGS_STATIC = -static-libgcc -static-libstdc++ $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf glew libcurl) -lGL

SRC = src/main.cpp src/window.cpp src/camera.cpp src/renderer.cpp \
      src/graph.cpp src/physics.cpp src/http_client.cpp src/html_parser.cpp src/ui.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = constellarix

all: $(TARGET)

static: $(OBJ)
	$(CXX) -o $(TARGET) $^ $(LDFLAGS_STATIC)

# Generate embedded assets
src/star_png.h: star.png
	xxd -i $< > $@

src/font_ttf.h: font.ttf
	xxd -i $< > $@

# renderer.cpp depends on embedded assets
src/renderer.o: src/star_png.h src/font_ttf.h

$(TARGET): $(OBJ)
	$(CXX) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET) src/star_png.h src/font_ttf.h

.PHONY: all clean static
