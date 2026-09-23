CC = gcc
GIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null || echo "unknown")
CFLAGS = -D_POSIX_C_SOURCE=200809L -DGIT_HASH=\"$(GIT_HASH)\" -Wall -Wextra -O2 -pthread
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

.PHONY: all clean test install install-bin install-service install-config uninstall FORCE

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

# ---------------------------------------------------------------------------
# Installation  (install vars are distinct from the build-time BINDIR = .)
# ---------------------------------------------------------------------------
PREFIX          ?= /usr/local
INSTALL_BINDIR  ?= $(PREFIX)/bin
CONFDIR         ?= /etc/weather-usrp
SYSTEMD_DIR     ?= $(PREFIX)/lib/systemd/system

UNIT_IN  = weather-usrp.service.in
UNIT_OUT = $(OBJDIR)/weather-usrp.service

install: install-bin install-service install-config
	@echo "Installed. Next steps:"
	@echo "  sudo systemctl daemon-reload"
	@echo "  sudo systemctl enable --now weather-usrp"
	@echo "Review $(CONFDIR)/config.ini before starting (FIPS codes)."

install-bin: $(TARGET)
	install -d $(DESTDIR)$(INSTALL_BINDIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(INSTALL_BINDIR)/weather-usrp

$(UNIT_OUT): $(UNIT_IN) FORCE
	sed 's|@BINDIR@|$(INSTALL_BINDIR)|g' $< > $@

install-service: $(UNIT_OUT)
	install -d $(DESTDIR)$(SYSTEMD_DIR)
	install -m 0644 $(UNIT_OUT) $(DESTDIR)$(SYSTEMD_DIR)/weather-usrp.service

install-config: config.ini
	install -d $(DESTDIR)$(CONFDIR)
	@if [ -f $(DESTDIR)$(CONFDIR)/config.ini ]; then \
		echo "config.ini already installed - not overwriting"; \
	else \
		install -m 0644 config.ini $(DESTDIR)$(CONFDIR)/config.ini; \
	fi

uninstall:
	rm -f $(DESTDIR)$(INSTALL_BINDIR)/weather-usrp
	rm -f $(DESTDIR)$(SYSTEMD_DIR)/weather-usrp.service
	@echo "Left $(CONFDIR)/config.ini in place (your settings)."
