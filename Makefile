CC = gcc
CFLAGS = -D_POSIX_C_SOURCE=200809L -Wall -Wextra -O2 -pthread
LDFLAGS = -lrtlsdr -lusb-1.0 -lm -lpthread

SRCDIR = src
OBJDIR = obj
TESTDIR = tests
BINDIR = .

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))
TARGET = $(BINDIR)/weather-usrp

TEST_SRCS = $(wildcard $(TESTDIR)/test_*.c)
TEST_BINS = $(patsubst $(TESTDIR)/%.c, $(OBJDIR)/%, $(TEST_SRCS))

LIB_OBJS = $(filter-out $(OBJDIR)/main.o, $(OBJS))

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/test_%: $(TESTDIR)/test_%.c $(LIB_OBJS) | $(OBJDIR)
	$(CC) $(CFLAGS) -I$(SRCDIR) -o $@ $< $(LIB_OBJS) $(LDFLAGS)

test: $(TEST_BINS)
	@echo "=== Running tests ==="
	@fail=0; \
	for t in $(TEST_BINS); do \
		echo "--- $$t ---"; \
		$$t || fail=1; \
	done; \
	if [ $$fail -eq 0 ]; then echo "\n=== ALL TESTS PASSED ==="; \
	else echo "\n=== SOME TESTS FAILED ===" && exit 1; fi

clean:
	rm -rf $(OBJDIR) $(TARGET)
