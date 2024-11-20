# Project-specific settings
PROJECT := Project4

# Identify compiler to use
CXX := clang++

# Flags to ALWAYs use
CFLAGS_all := -Wall -Wextra -std=c++20

# Flags based on compilation type.
#   Default flags turn on optimizations
#   Use "make debug" to turn on debugger flag
#   Use "make grumpy" to get extra warnings during compilation
CFLAGS := -O3 -DNDEBUG $(CFLAGS_all)
CFLAGS_debug := -g $(CFLAGS_all)
CFLAGS_grumpy := -pedantic -Wconversion -Weffc++ $(CFLAGS_all)

default: $(PROJECT)
all: $(PROJECT)

debug:	CFLAGS := $(CFLAGS_debug)
debug:	$(PROJECT)

grumpy:	CFLAGS := $(CFLAGS_grumpy)
grumpy:	$(PROJECT)

tests: $(PROJECT)
	@echo "Running tests..."
	cd tests && ./run_tests.sh
	@echo "Tests completed."
	
# Always run the tests, even if nothing has changed
.PHONY: tests

# List any files here that should trigger full recompilation when they change.
KEY_FILES := lexer.hpp

$(PROJECT):	$(PROJECT).cpp $(KEY_FILES)
	$(CXX) $(CFLAGS) $(PROJECT).cpp -o $(PROJECT)

clean:
	rm -f $(PROJECT) *.o tests/test-??.wasm tests/test-??.wat tests/P3-test-??.wasm tests/P3-test-??.wat
	rm -rf $(PROJECT).dSYM

# Debugging information
print-%: ; @echo '$(subst ','\'',$*=$($*))'
