CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -Iinclude -Iinclude/core -Iinclude/game -Iinclude/ai -Iinclude/story -Iinclude/generation
LDFLAGS = -lGL -lglfw -lm

SRCDIR = src
OBJDIR = obj
BINDIR = bin
LIBDIR = lib
INCDIR = include
TESTDIR = tests

# Core engine sources
CORE_SOURCES = $(wildcard $(SRCDIR)/core/*.c)
CORE_OBJECTS = $(CORE_SOURCES:$(SRCDIR)/core/%.c=$(OBJDIR)/core_%.o)

# Game system sources
GAME_SOURCES = $(wildcard $(SRCDIR)/game/*.c)
GAME_OBJECTS = $(GAME_SOURCES:$(SRCDIR)/game/%.c=$(OBJDIR)/game_%.o)

# AI system sources
AI_SOURCES = $(wildcard $(SRCDIR)/ai/*.c)
AI_OBJECTS = $(AI_SOURCES:$(SRCDIR)/ai/%.c=$(OBJDIR)/ai_%.o)

# Story system sources
STORY_SOURCES = $(wildcard $(SRCDIR)/story/*.c)
STORY_OBJECTS = $(STORY_SOURCES:$(SRCDIR)/story/%.c=$(OBJDIR)/story_%.o)

# Generation system sources
GEN_SOURCES = $(wildcard $(SRCDIR)/generation/*.c)
GEN_OBJECTS = $(GEN_SOURCES:$(SRCDIR)/generation/%.c=$(OBJDIR)/gen_%.o)

# Demo sources
DEMO_SOURCES = $(wildcard $(SRCDIR)/demos/*.c)
DEMO_OBJECTS = $(DEMO_SOURCES:$(SRCDIR)/demos/%.c=$(OBJDIR)/demo_%.o)

# Main sources
MAIN_SOURCES = $(wildcard $(SRCDIR)/*.c)
MAIN_OBJECTS = $(MAIN_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# All library objects (everything except mains)
LIB_OBJECTS = $(CORE_OBJECTS) $(GAME_OBJECTS) $(AI_OBJECTS) $(STORY_OBJECTS) $(GEN_OBJECTS)

# All objects
ALL_OBJECTS = $(LIB_OBJECTS) $(DEMO_OBJECTS) $(MAIN_OBJECTS)

# Targets
TARGET = $(BINDIR)/cengine
PHYSICS_TARGET = $(BINDIR)/physics_demo
GRID_TARGET = $(BINDIR)/grid_demo

# Test sources
LEGACY_TEST_SOURCES = $(wildcard $(TESTDIR)/unit_legacy/*.c)
LEGACY_TEST_OBJECTS = $(LEGACY_TEST_SOURCES:$(TESTDIR)/unit_legacy/%.c=$(OBJDIR)/legacy_test_%.o)
LEGACY_TEST_TARGETS = $(LEGACY_TEST_SOURCES:$(TESTDIR)/unit_legacy/test_%.c=$(BINDIR)/legacy_test_%)

TEST_SOURCES = $(wildcard $(TESTDIR)/unit/*.c)
TEST_OBJECTS = $(TEST_SOURCES:$(TESTDIR)/unit/%.c=$(OBJDIR)/test_%.o)
TEST_TARGETS = $(TEST_SOURCES:$(TESTDIR)/unit/%.c=$(BINDIR)/%)

INTEGRATION_TEST_SOURCES = $(wildcard $(TESTDIR)/integration/*.c)
INTEGRATION_TEST_OBJECTS = $(INTEGRATION_TEST_SOURCES:$(TESTDIR)/integration/%.c=$(OBJDIR)/int_test_%.o)
INTEGRATION_TEST_TARGETS = $(INTEGRATION_TEST_SOURCES:$(TESTDIR)/integration/%.c=$(BINDIR)/int_%)

.PHONY: all clean run test test-legacy test-integration physics run-physics grid run-grid clean-test tools

all: $(TARGET)

# Main targets
$(TARGET): $(MAIN_OBJECTS) $(LIB_OBJECTS) | $(BINDIR)
	$(CC) $(MAIN_OBJECTS) $(LIB_OBJECTS) -o $@ $(LDFLAGS)

physics: $(PHYSICS_TARGET)
$(PHYSICS_TARGET): $(OBJDIR)/demo_physics_demo.o $(LIB_OBJECTS) | $(BINDIR)
	$(CC) $(OBJDIR)/demo_physics_demo.o $(LIB_OBJECTS) -o $@ $(LDFLAGS)

grid: $(GRID_TARGET)
$(GRID_TARGET): $(OBJDIR)/demo_grid_demo.o $(LIB_OBJECTS) | $(BINDIR)
	$(CC) $(OBJDIR)/demo_grid_demo.o $(LIB_OBJECTS) -o $@ $(LDFLAGS)

# Object file rules
$(OBJDIR)/core_%.o: $(SRCDIR)/core/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/game_%.o: $(SRCDIR)/game/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/ai_%.o: $(SRCDIR)/ai/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/story_%.o: $(SRCDIR)/story/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/gen_%.o: $(SRCDIR)/generation/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/demo_%.o: $(SRCDIR)/demos/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Directory creation
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Run targets
run: $(TARGET)
	./$(TARGET)

run-physics: $(PHYSICS_TARGET)
	./$(PHYSICS_TARGET)

run-grid: $(GRID_TARGET)
	./$(GRID_TARGET)

# Test targets
test: $(TEST_TARGETS)
	@echo "Running unit tests..."
	@for test in $(TEST_TARGETS); do \
		echo "Running $$test"; \
		$$test || exit 1; \
	done

test-legacy: $(LEGACY_TEST_TARGETS)
	@echo "Running legacy tests..."
	@for test in $(LEGACY_TEST_TARGETS); do \
		echo "Running $$test"; \
		$$test || exit 1; \
	done

test-integration: $(INTEGRATION_TEST_TARGETS)
	@echo "Running integration tests..."
	@for test in $(INTEGRATION_TEST_TARGETS); do \
		echo "Running $$test"; \
		$$test || exit 1; \
	done

test-all: test test-legacy test-integration

# Test compilation rules
$(BINDIR)/test_%: $(OBJDIR)/test_%.o $(LIB_OBJECTS) | $(BINDIR)
	$(CC) $< $(LIB_OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/test_%.o: $(TESTDIR)/unit/test_%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/legacy_test_%: $(OBJDIR)/legacy_test_%.o $(LIB_OBJECTS) | $(BINDIR)
	$(CC) $< $(LIB_OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/legacy_test_%.o: $(TESTDIR)/unit_legacy/test_%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/int_%: $(OBJDIR)/int_test_%.o $(LIB_OBJECTS) | $(BINDIR)
	$(CC) $< $(LIB_OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/int_test_%.o: $(TESTDIR)/integration/test_%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build configurations
debug: CFLAGS += -DDEBUG
debug: $(TARGET)

release: CFLAGS += -O3 -DNDEBUG
release: clean $(TARGET)

# Tools (future development tools)
tools:
	@echo "Tools will be implemented in future phases"

# Cleanup
clean:
	rm -rf $(OBJDIR) $(BINDIR)

clean-test:
	rm -f $(OBJDIR)/test_*.o $(OBJDIR)/legacy_test_*.o $(OBJDIR)/int_test_*.o $(BINDIR)/test_* $(BINDIR)/legacy_* $(BINDIR)/int_*

# Help target
help:
	@echo "Available targets:"
	@echo "  all           - Build main engine"
	@echo "  physics       - Build physics demo"
	@echo "  grid          - Build grid demo"
	@echo "  test          - Run unit tests"
	@echo "  test-legacy   - Run legacy tests"
	@echo "  test-integration - Run integration tests"
	@echo "  test-all      - Run all tests"
	@echo "  run           - Run main engine"
	@echo "  run-physics   - Run physics demo"
	@echo "  run-grid      - Run grid demo"
	@echo "  debug         - Build with debug flags"
	@echo "  release       - Build optimized release"
	@echo "  clean         - Clean all build files"
	@echo "  clean-test    - Clean test files"
	@echo "  tools         - Build development tools"
	@echo "  help          - Show this help"