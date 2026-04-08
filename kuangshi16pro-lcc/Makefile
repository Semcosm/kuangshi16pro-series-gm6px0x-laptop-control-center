CC ?= cc
PKG_CONFIG ?= pkg-config
INSTALL ?= install

SYSTEMD_CFLAGS := $(shell $(PKG_CONFIG) --cflags libsystemd 2>/dev/null)
SYSTEMD_LIBS := $(shell $(PKG_CONFIG) --libs libsystemd 2>/dev/null)
POLKIT_CFLAGS := $(shell $(PKG_CONFIG) --cflags polkit-gobject-1 2>/dev/null)
POLKIT_LIBS := $(shell $(PKG_CONFIG) --libs polkit-gobject-1 2>/dev/null)
POLKIT_AGENT_AVAILABLE := $(shell $(PKG_CONFIG) --exists polkit-agent-1 && printf 'yes')
ifeq ($(POLKIT_AGENT_AVAILABLE),yes)
POLKIT_AGENT_CFLAGS := $(shell $(PKG_CONFIG) --cflags polkit-agent-1 2>/dev/null)
POLKIT_AGENT_LIBS := $(shell $(PKG_CONFIG) --libs polkit-agent-1 2>/dev/null)
POLKIT_AGENT_CPPFLAGS := -DLCC_HAVE_POLKIT_AGENT=1 -DPOLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
else
POLKIT_AGENT_CFLAGS :=
POLKIT_AGENT_LIBS :=
POLKIT_AGENT_CPPFLAGS :=
endif

CPPFLAGS ?= -Iinclude -Isrc $(SYSTEMD_CFLAGS) $(POLKIT_CFLAGS) $(POLKIT_AGENT_CFLAGS) $(POLKIT_AGENT_CPPFLAGS)
CFLAGS_BASE := -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wformat=2
RELEASE_CFLAGS := -O2
DEBUG_CFLAGS := -O0 -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
LDFLAGS ?=
LDLIBS ?=

SRC_DIR := src
TEST_DIR := tests
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN := $(BUILD_DIR)/lccctl
DAEMON_BIN := $(BUILD_DIR)/lccd
TEST_BIN := $(BUILD_DIR)/test_kuangshi16pro_lcc
PREFIX ?= /usr
DESTDIR ?=
BINDIR ?= $(PREFIX)/bin
LIBEXECDIR ?= $(PREFIX)/lib/kuangshi16pro-lcc
SYSTEMDUNITDIR ?= /etc/systemd/system
DBUSSYSTEMSERVICEDIR ?= $(PREFIX)/share/dbus-1/system-services
DBUSSYSTEMCONFDIR ?= /etc/dbus-1/system.d
POLKITACTIONSDIR ?= $(PREFIX)/share/polkit-1/actions
SYSTEMD_UNIT_SRC := systemd/lccd.service
DBUS_SERVICE_SRC := systemd/dbus-io.github.semcosm.Lcc1.service
DBUS_CONFIG_SRC := dbus/io.github.semcosm.Lcc1.conf
POLKIT_POLICY_SRC := dbus/io.github.semcosm.Lcc1.policy

COMMON_SRCS := \
	$(SRC_DIR)/common/lcc_error.c \
	$(SRC_DIR)/common/lcc_log.c \
	$(SRC_DIR)/backends/backend.c \
	$(SRC_DIR)/backends/amw0/backend.c \
	$(SRC_DIR)/backends/converged/backend.c \
	$(SRC_DIR)/backends/amw0/transport.c \
	$(SRC_DIR)/backends/amw0/wmbc-pack.c \
	$(SRC_DIR)/backends/amw0/ec-addr-map.c \
	$(SRC_DIR)/backends/amw0/ecmg.c \
	$(SRC_DIR)/backends/standard/backend.c \
	$(SRC_DIR)/backends/standard/hwmon.c \
	$(SRC_DIR)/backends/standard/thermal.c \
	$(SRC_DIR)/backends/standard/platform_profile.c \
	$(SRC_DIR)/backends/standard/powercap.c

APP_SRCS := \
	$(COMMON_SRCS) \
	$(SRC_DIR)/core/profile/plan.c \
	$(SRC_DIR)/core/profile/mode-profile.c \
	$(SRC_DIR)/core/profile/loader.c \
	$(SRC_DIR)/core/profile/validator.c \
	$(SRC_DIR)/core/power/limits.c \
	$(SRC_DIR)/core/fan/table.c \
	$(SRC_DIR)/core/fan/table-apply.c \
	$(SRC_DIR)/cli/amw0_cli.c \
	$(SRC_DIR)/cli/dbus_client.c \
	$(SRC_DIR)/cli/main.c \
	$(SRC_DIR)/cli/cmd-state.c \
	$(SRC_DIR)/cli/cmd-debug.c \
	$(SRC_DIR)/cli/cmd-mode.c \
	$(SRC_DIR)/cli/cmd-power.c \
	$(SRC_DIR)/cli/cmd-fan.c \
	$(SRC_DIR)/cli/cmd-profile.c

DAEMON_SRCS := \
	$(COMMON_SRCS) \
	$(SRC_DIR)/backends/mock/mock-backend.c \
	$(SRC_DIR)/core/capabilities/detect.c \
	$(SRC_DIR)/core/capabilities/model-map.c \
	$(SRC_DIR)/core/profile/plan.c \
	$(SRC_DIR)/core/profile/mode-profile.c \
	$(SRC_DIR)/core/profile/loader.c \
	$(SRC_DIR)/core/profile/validator.c \
	$(SRC_DIR)/core/power/limits.c \
	$(SRC_DIR)/core/fan/table.c \
	$(SRC_DIR)/core/fan/table-apply.c \
	$(SRC_DIR)/core/state/model.c \
	$(SRC_DIR)/core/state/reader.c \
	$(SRC_DIR)/daemon/app.c \
	$(SRC_DIR)/daemon/main.c \
	$(SRC_DIR)/daemon/manager.c \
	$(SRC_DIR)/daemon/transaction.c \
	$(SRC_DIR)/daemon/errors.c \
	$(SRC_DIR)/dbus/policy.c \
	$(SRC_DIR)/dbus/server.c

TEST_SRCS := \
	$(COMMON_SRCS) \
	$(SRC_DIR)/backends/mock/mock-backend.c \
	$(SRC_DIR)/core/capabilities/detect.c \
	$(SRC_DIR)/core/capabilities/model-map.c \
	$(SRC_DIR)/core/profile/plan.c \
	$(SRC_DIR)/core/profile/mode-profile.c \
	$(SRC_DIR)/core/profile/loader.c \
	$(SRC_DIR)/core/profile/validator.c \
	$(SRC_DIR)/core/power/limits.c \
	$(SRC_DIR)/core/fan/table.c \
	$(SRC_DIR)/core/fan/table-apply.c \
	$(SRC_DIR)/core/state/model.c \
	$(SRC_DIR)/core/state/reader.c \
	$(SRC_DIR)/daemon/manager.c \
	$(SRC_DIR)/daemon/transaction.c \
	$(TEST_DIR)/unit/test_backend_amw0.c \
	$(TEST_DIR)/unit/test_backend_converged.c \
	$(TEST_DIR)/unit/test_backend_standard.c \
	$(TEST_DIR)/unit/test_backend_mock.c \
	$(TEST_DIR)/unit/test_profiles.c \
	$(TEST_DIR)/unit/test_state_model.c \
	$(TEST_DIR)/unit/test_manager.c \
	$(TEST_DIR)/unit/test_transaction.c

APP_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(APP_SRCS))
DAEMON_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(DAEMON_SRCS))
TEST_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_SRCS))

CFLAGS ?= $(CFLAGS_BASE) $(RELEASE_CFLAGS)

.PHONY: all clean debug test run daemon install uninstall install-bin install-assets install-smoke

all: $(BIN) $(DAEMON_BIN)

debug: CFLAGS := $(CFLAGS_BASE) $(DEBUG_CFLAGS)
debug: clean all

test: $(TEST_BIN)
	$(TEST_BIN)

run: $(BIN)
	$(BIN) --help

daemon: $(DAEMON_BIN)
	$(DAEMON_BIN) --help

install: all install-bin install-assets

install-bin: $(BIN) $(DAEMON_BIN)
	$(INSTALL) -d -m 0755 "$(DESTDIR)$(BINDIR)" "$(DESTDIR)$(LIBEXECDIR)"
	$(INSTALL) -m 0755 "$(BIN)" "$(DESTDIR)$(BINDIR)/lccctl"
	$(INSTALL) -m 0755 "$(DAEMON_BIN)" "$(DESTDIR)$(LIBEXECDIR)/lccd"

install-assets:
	$(INSTALL) -d -m 0755 \
		"$(DESTDIR)$(SYSTEMDUNITDIR)" \
		"$(DESTDIR)$(DBUSSYSTEMSERVICEDIR)" \
		"$(DESTDIR)$(DBUSSYSTEMCONFDIR)" \
		"$(DESTDIR)$(POLKITACTIONSDIR)"
	$(INSTALL) -m 0644 /dev/null "$(DESTDIR)$(SYSTEMDUNITDIR)/lccd.service"
	sed 's|^ExecStart=.*|ExecStart=$(LIBEXECDIR)/lccd --system|' \
		"$(SYSTEMD_UNIT_SRC)" >"$(DESTDIR)$(SYSTEMDUNITDIR)/lccd.service"
	$(INSTALL) -m 0644 "$(DBUS_SERVICE_SRC)" \
		"$(DESTDIR)$(DBUSSYSTEMSERVICEDIR)/io.github.semcosm.Lcc1.service"
	$(INSTALL) -m 0644 "$(DBUS_CONFIG_SRC)" \
		"$(DESTDIR)$(DBUSSYSTEMCONFDIR)/io.github.semcosm.Lcc1.conf"
	$(INSTALL) -m 0644 "$(POLKIT_POLICY_SRC)" \
		"$(DESTDIR)$(POLKITACTIONSDIR)/io.github.semcosm.Lcc1.policy"

uninstall:
	rm -f \
		"$(DESTDIR)$(BINDIR)/lccctl" \
		"$(DESTDIR)$(LIBEXECDIR)/lccd" \
		"$(DESTDIR)$(SYSTEMDUNITDIR)/lccd.service" \
		"$(DESTDIR)$(DBUSSYSTEMSERVICEDIR)/io.github.semcosm.Lcc1.service" \
		"$(DESTDIR)$(DBUSSYSTEMCONFDIR)/io.github.semcosm.Lcc1.conf" \
		"$(DESTDIR)$(POLKITACTIONSDIR)/io.github.semcosm.Lcc1.policy"
	-rmdir -p --ignore-fail-on-non-empty "$(DESTDIR)$(LIBEXECDIR)"
	-rmdir -p --ignore-fail-on-non-empty "$(DESTDIR)$(BINDIR)"
	-rmdir -p --ignore-fail-on-non-empty "$(DESTDIR)$(SYSTEMDUNITDIR)"
	-rmdir -p --ignore-fail-on-non-empty "$(DESTDIR)$(DBUSSYSTEMSERVICEDIR)"
	-rmdir -p --ignore-fail-on-non-empty "$(DESTDIR)$(DBUSSYSTEMCONFDIR)"
	-rmdir -p --ignore-fail-on-non-empty "$(DESTDIR)$(POLKITACTIONSDIR)"

install-smoke:
	bash "$(TEST_DIR)/integration/test_install_smoke.sh"

clean:
	rm -rf $(BUILD_DIR)

$(BIN): $(APP_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS) $(SYSTEMD_LIBS) $(POLKIT_AGENT_LIBS)

$(DAEMON_BIN): $(DAEMON_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS) $(SYSTEMD_LIBS) $(POLKIT_LIBS)

$(TEST_BIN): $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
