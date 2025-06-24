CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -Iinclude
LDFLAGS = -lGL -lglfw -lm

SRCDIR = src
OBJDIR = obj
BINDIR = bin
LIBDIR = lib
INCDIR = include
TESTDIR = tests

SOURCES = $(wildcard $(SRCDIR)/*.c)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
TARGET = $(BINDIR)/cengine

LIB_SOURCES = $(filter-out $(SRCDIR)/main.c $(SRCDIR)/main_physics.c, $(SOURCES))
LIB_OBJECTS = $(LIB_SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

PHYSICS_TARGET = $(BINDIR)/physics_demo

TEST_SOURCES = $(wildcard $(TESTDIR)/unit/*.c)
TEST_OBJECTS = $(TEST_SOURCES:$(TESTDIR)/unit/%.c=$(OBJDIR)/test_%.o)
TEST_TARGETS = $(TEST_SOURCES:$(TESTDIR)/unit/%.c=$(BINDIR)/%)

.PHONY: all clean run test clean-test physics run-physics

all: $(TARGET)

physics: $(PHYSICS_TARGET)

$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(PHYSICS_TARGET): $(LIB_OBJECTS) $(OBJDIR)/main_physics.o | $(BINDIR)
	$(CC) $(LIB_OBJECTS) $(OBJDIR)/main_physics.o -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

run: $(TARGET)
	./$(TARGET)

run-physics: $(PHYSICS_TARGET)
	./$(PHYSICS_TARGET)

debug: CFLAGS += -DDEBUG
debug: $(TARGET)

release: CFLAGS += -O3 -DNDEBUG
release: clean $(TARGET)

# Test targets
test: $(TEST_TARGETS)
	@echo "Running tests..."
	@for test in $(TEST_TARGETS); do \
		echo "Running $$test"; \
		$$test || exit 1; \
	done

$(BINDIR)/test_%: $(OBJDIR)/test_%.o $(LIB_OBJECTS) | $(BINDIR)
	$(CC) $< $(LIB_OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/test_%.o: $(TESTDIR)/unit/test_%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean-test:
	rm -f $(OBJDIR)/test_*.o $(BINDIR)/test_*