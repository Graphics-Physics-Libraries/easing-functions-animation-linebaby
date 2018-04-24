UNAME_S := $(shell uname -s)

BUILD_DIR ?= build

CFLAGS += -Wall -Wno-unknown-pragmas
CFLAGS += -Ibuild/include -Isrc

DEBUG ?= 1
ifeq ($(DEBUG), 1)
	CFLAGS += -DDEBUG -DTEST -g
else
	CFLAGS += -DNDEBUG -O3
endif

CXXFLAGS += $(CFLAGS)

.PHONY: all
ifeq ($(UNAME_S),Darwin)
	all: $(BUILD_DIR)/bin/Linebaby.app
else ifeq ($(UNAME_S),Linux)
	all: $(BUILD_DIR)/bin/linebaby
endif

$(BUILD_DIR)/assets/%.c: src/assets/%
	mkdir -p $(@D)
	xxd -i $< $@

LINEBABY_ASSETS := $(shell find src/assets -type f)
LINEBABY_ASSETS_PROCESSED := $(patsubst src/assets/%,$(BUILD_DIR)/assets/%.c,$(LINEBABY_ASSETS))
.SECONDARY: $(LINEBABY_ASSETS_PROCESSED)

LINEBABY_SOURCES := $(shell find src -type f -name '*.c' -o -name '*.cpp')
LINEBABY_OBJECTS := $(patsubst src/%.cpp,$(BUILD_DIR)/obj/%.o,$(patsubst src/%.c,$(BUILD_DIR)/obj/%.o,$(LINEBABY_SOURCES)))

$(BUILD_DIR)/obj/%.o: src/%.c $(LINEBABY_ASSETS_PROCESSED) | $(BUILD_DIR)/vendor
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/obj/%.o: src/%.cpp $(LINEBABY_ASSETS_PROCESSED) | $(BUILD_DIR)/vendor
	mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

EXEC_LIBS :=
ifeq ($(UNAME_S),Darwin)
	EXEC_LIBS += -framework Cocoa -framework IOKit -framework CoreFoundation -framework CoreVideo -framework OpenGL
endif

$(BUILD_DIR)/bin/linebaby: LDFLAGS += -L$(BUILD_DIR)/lib
$(BUILD_DIR)/bin/linebaby: LDLIBS += -lstdc++ -limgui $(shell env PKG_CONFIG_PATH=./build/lib/pkgconfig pkg-config --static --libs-only-l glfw3 glew) $(EXEC_LIBS)
$(BUILD_DIR)/bin/linebaby: $(LINEBABY_OBJECTS) | $(BUILD_DIR)/vendor
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@

$(BUILD_DIR)/bin/Linebaby.app: $(BUILD_DIR)/bin/linebaby src/misc/macos/*
	mkdir -p $@/Contents
	mkdir -p $@/Contents/MacOS
	mkdir -p $@/Contents/Resources
	
	cp -f src/misc/macos/Info.plist $@/Contents/Info.plist
	cp -f src/misc/macos/Linebaby.icns $@/Contents/Resources/Linebaby.icns
	cp -f $(BUILD_DIR)/bin/linebaby $@/Contents/MacOS/linebaby

$(BUILD_DIR)/linebaby.linux.zip: $(BUILD_DIR)/bin/linebaby src/misc/LICENSES.txt
	mkdir -p $(@D)
	zip -j $@ $^
	
# --- VENDOR ---

.PHONY: vendor-package
vendor-package:
	tar -czf vendor.tar.gz vendor

.PHONY: vendor
vendor: $(BUILD_DIR)/vendor

$(BUILD_DIR)/vendor: vendor.tar.gz
	mkdir -p $(BUILD_DIR)/{bin,include,lib}
	tar -xzf $^ -C $(BUILD_DIR); touch $@ # touch overrides the tar's original creation date, fixing make's dependency order 
	
	cd $@/glfw-*; mkdir build; cd build; cmake -DCMAKE_INSTALL_PREFIX=$(abspath $(BUILD_DIR)) ..; $(MAKE); $(MAKE) install
	
	cd $@/glew-*/build; cmake -DCMAKE_INSTALL_LIBDIR=$(abspath $(BUILD_DIR)/lib) -DCMAKE_INSTALL_PREFIX=$(abspath $(BUILD_DIR)) ./cmake; $(MAKE); $(MAKE) install
	rm -f $(BUILD_DIR)/lib/libGLEW*.so # Delete the shared library. TODO: Possible to only build static version of GLEW?
	rm -f $(BUILD_DIR)/lib/libGLEW*.dylib
	
	mkdir -p $(BUILD_DIR)/include/imgui
	cp -r $@/imgui-*/*.h $(BUILD_DIR)/include/imgui
	cd $@/imgui-*; $(CXX) $(CXXFLAGS) -I. *.cpp -c
	cd $@/imgui-*; $(AR) $(ARFLAGS) ../../lib/libimgui.a *.o
	
	cp $@/*.h $(BUILD_DIR)/include
	
# --- MISC ---

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)/bin $(BUILD_DIR)/assets $(BUILD_DIR)/obj
