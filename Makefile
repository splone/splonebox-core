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

all: sb test sb-makekey

sb:
	test -d $(BUILD_DIR) || mkdir $(BUILD_DIR)
	cd out && cmake -G '$(BUILD_TYPE)' $(FLAGS) $(EXTRA_FLAGS) ..
	$(BUILD_CMD) -C out sb

test:
	test -d $(BUILD_DIR) || mkdir $(BUILD_DIR)
	cd out && cmake -G '$(BUILD_TYPE)' $(FLAGS) $(EXTRA_FLAGS) ..
	$(BUILD_CMD) -C out sb-test

sb-makekey:
	test -d $(BUILD_DIR) || mkdir $(BUILD_DIR)
	cd out && cmake -G '$(BUILD_TYPE)' $(FLAGS) $(EXTRA_FLAGS) ..
	$(BUILD_CMD) -C out sb-makekey

clean:
	+test -d out && $(BUILD_CMD) -C out clean || true

distclean: clean
	rm -rf out

install: | sb
	+$(BUILD_CMD) -C out install

.PHONY: test clean distclean sb install sb-makekey
