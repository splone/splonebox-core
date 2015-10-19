BUILD_DIR := out
CMAKE_BUILD_TYPE ?= Debug
FLAGS := -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
EXTRA_FLAGS ?=

# ninja
BUILD_TOOL := ninja
BUILD_TYPE := Ninja
# make
#BUILD_TOOL = $(MAKE)
#BUILD_TYPE := Unix Makefiles

ifneq ($(VERBOSE),)
  ifeq ($(BUILD_TYPE),Ninja)
    VERBOSE_FLAG := -v
  endif
endif

BUILD_CMD = $(BUILD_TOOL) $(VERBOSE_FLAG)

all: sb test sb-plugin

sb:
	test -d $(BUILD_DIR) || mkdir $(BUILD_DIR)
	cd out && cmake -G '$(BUILD_TYPE)' $(FLAGS) $(EXTRA_FLAGS) ..
	$(BUILD_CMD) -C out sb

test:
	test -d $(BUILD_DIR) || mkdir $(BUILD_DIR)
	cd out && cmake -G '$(BUILD_TYPE)' $(FLAGS) $(EXTRA_FLAGS) ..
	$(BUILD_CMD) -C out sb-test

sb-plugin:
	test -d $(BUILD_DIR) || mkdir $(BUILD_DIR)
	cd out && cmake -G '$(BUILD_TYPE)' $(FLAGS) $(EXTRA_FLAGS) ..
	$(BUILD_CMD) -C out sb-plugin

clean:
	+test -d out && $(BUILD_CMD) -C out clean || true

distclean: clean
	rm -rf out

install: | sb
	+$(BUILD_CMD) -C out install

.PHONY: test clean distclean sb install sb-plugin
