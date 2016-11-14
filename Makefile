BUILD_DIR := out
CMAKE_BUILD_TYPE ?= Debug
FLAGS := -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
EXTRA_FLAGS ?=

BUILD_TYPE ?= $(shell (type ninja > /dev/null 2>&1 && echo "Ninja") || echo "Unix Makefiles")

ifeq (,$(BUILD_TOOL))
  ifeq (Ninja,$(BUILD_TYPE))
    ifneq ($(shell cmake --help 2>/dev/null | grep Ninja),)
      BUILD_TOOL := ninja
    else
      BUILD_TOOL = $(MAKE)
      BUILD_TYPE := Unix Makefiles
    endif
  else
    BUILD_TOOL = $(MAKE)
  endif
endif

ifneq ($(VERBOSE),)
  ifeq ($(BUILD_TYPE),Ninja)
    VERBOSE_FLAG := -v
  endif
endif

BUILD_CMD = $(BUILD_TOOL) $(VERBOSE_FLAG)

USE_BUNDLED_DEPS ?=

ifneq (,$(USE_BUNDLED_DEPS))
  BUNDLED_CMAKE_FLAG := -DUSE_BUNDLED=$(USE_BUNDLED_DEPS)
endif

all: sb test sb-makekey sb-pluginkey

sb: build/.ran-cmake deps
	+$(BUILD_CMD) -C build sb

cmake:
	touch CMakeLists.txt
	$(MAKE) build/.ran-cmake

build/.ran-cmake: | deps
	cd build && cmake -G '$(BUILD_TYPE)' $(CMAKE_FLAGS) $(CMAKE_EXTRA_FLAGS) ..
	touch $@

deps: | build/.ran-third-party-cmake
ifeq ($(call filter-true,$(USE_BUNDLED_DEPS)),)
	+$(BUILD_CMD) -C .deps
endif

build/.ran-third-party-cmake:
ifeq ($(call filter-true,$(USE_BUNDLED_DEPS)),)
		mkdir -p .deps
		cd .deps && \
			cmake -G '$(BUILD_TYPE)' $(BUNDLED_CMAKE_FLAG) $(BUNDLED_LUA_CMAKE_FLAG) \
			$(DEPS_CMAKE_FLAGS) ../third-party
endif
		mkdir -p build
		touch $@

test: build/.ran-cmake deps
	+$(BUILD_CMD) -C build sb-test

sb-makekey: build/.ran-cmake deps
	+$(BUILD_CMD) -C build sb-makekey

sb-pluginkey: build/.ran-cmake deps
	+$(BUILD_CMD) -C build sb-pluginkey

clean:
	+test -d build && $(BUILD_CMD) -C build clean || true

distclean: clean
	rm -rf .deps build

install: | sb
	+$(BUILD_CMD) -C out install

.PHONY: test clean distclean sb install sb-makekey deps cmake
