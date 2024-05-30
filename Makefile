BUILD_DIR = ./build
BINARY = $(BUILD_DIR)/main
CODE_DIRS = .
INCLUDE_DIRS = .
C_FILES := $(foreach D,$(CODE_DIRS),$(wildcard $(D)/*.cpp))
OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(C_FILES))
DEP_FILES := $(patsubst %.cpp,$(BUILD_DIR)/%.d,$(C_FILES))
CXX := $(shell which clang++)
SANITIZER_FLAGS := #-fsanitize=address
DEFINES := -DNDEBUG
CPP_FLAGS := $(DEFINES)
CXX_FLAGS :=
override CXX_FLAGS += -std=c++20 -g $(CPP_FLAGS) $(SANITIZER_FLAGS) $(foreach D,$(INCLUDE_DIRS),-I$(D)) -MMD -MP

.PHONY: all
all: $(BINARY)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)/*

$(BINARY): $(OBJECTS)
	$(CXX) $(SANITIZER_FLAGS) -o $@ $^

$(OBJECTS): $(BUILD_DIR)/%.o: %.cpp $(BUILD_DIR)/%.d | $(BUILD_DIR) 
	$(CXX) $(CXX_FLAGS) -c -o $@ $<
ifneq ($(findstring clang,$(CXX)),) # clang's .o file is older than the .d file (which is not what we want), gcc's .o is newer
	touch $@
endif

$(BUILD_DIR):
	mkdir -p $@

$(DEP_FILES):

-include $(DEP_FILES)