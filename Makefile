BUILD_DIR=./build
BINARY=$(BUILD_DIR)/main
CODE_DIRS=.
INCLUDE_DIRS=.
C_FILES=$(foreach D,$(CODE_DIRS),$(wildcard $(D)/*.cpp))
OBJECTS=$(patsubst %.cpp,$(BUILD_DIR)/%.o,$(C_FILES))
DEP_FILES=$(patsubst %.cpp,$(BUILD_DIR)/%.d,$(C_FILES))
CXX=$(shell which clang++)
CXX_FLAGS=-std=c++20 -g -DNDEBUG $(foreach D,$(INCLUDE_DIRS),-I$(D)) -MP -MD

.PHONY: all
all: $(BINARY)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)/*

$(BINARY): $(OBJECTS)
	$(CXX) -o $@ $^

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXX_FLAGS) -c -o $@ $<

-include $(DEP_FILES)