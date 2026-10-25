CXX ?= g++
BUILD_DIR := build/make
BIN := $(BUILD_DIR)/kiz

# 版本号单一事实源：仓库根目录的VERSION文件（用 scripts/bump_version.py 更新）
VERSION := $(shell cat VERSION)
VERSION_MAJOR := $(word 1,$(subst ., ,$(VERSION)))
VERSION_MINOR := $(word 2,$(subst ., ,$(VERSION)))
VERSION_PATCH := $(word 3,$(subst ., ,$(VERSION)))

INCLUDES := -Isrc -Idepends -Ilibs -I$(BUILD_DIR)/include
CXXFLAGS ?= -std=c++20 -Ofast
LDFLAGS :=

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LDFLAGS += -framework CoreFoundation -framework CoreGraphics
endif

SRCS := $(wildcard src/*.cpp) \
        $(wildcard src/lexer/*.cpp) \
        $(wildcard src/parser/*.cpp) \
        $(wildcard src/ir_gen/*.cpp) \
        $(wildcard src/vm/*.cpp) \
        $(wildcard src/error/*.cpp) \
        $(wildcard src/repl/*.cpp) \
        $(wildcard libs/builtins/*.cpp) \
        $(wildcard libs/os/*.cpp)

OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

.PHONY: all run clean

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@


run: $(BIN)
	$(BIN)

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
