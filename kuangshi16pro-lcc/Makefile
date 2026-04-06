CC ?= cc
PKG_CONFIG ?= pkg-config

SYSTEMD_CFLAGS := $(shell $(PKG_CONFIG) --cflags libsystemd 2>/dev/null)
SYSTEMD_LIBS := $(shell $(PKG_CONFIG) --libs libsystemd 2>/dev/null)

CPPFLAGS ?= -Iinclude -Isrc $(SYSTEMD_CFLAGS)
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

COMMON_SRCS := \
	$(SRC_DIR)/common/lcc_error.c \
	$(SRC_DIR)/common/lcc_log.c \
	$(SRC_DIR)/backends/backend.c \
	$(SRC_DIR)/backends/amw0/backend.c \
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
	$(TEST_DIR)/unit/test_backend_standard.c \
	$(TEST_DIR)/unit/test_backend_mock.c \
	$(TEST_DIR)/unit/test_profiles.c \
	$(TEST_DIR)/unit/test_manager.c \
	$(TEST_DIR)/unit/test_transaction.c

APP_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(APP_SRCS))
DAEMON_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(DAEMON_SRCS))
TEST_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_SRCS))

CFLAGS ?= $(CFLAGS_BASE) $(RELEASE_CFLAGS)

.PHONY: all clean debug test run daemon

all: $(BIN) $(DAEMON_BIN)

debug: CFLAGS := $(CFLAGS_BASE) $(DEBUG_CFLAGS)
debug: clean all

test: $(TEST_BIN)
	$(TEST_BIN)

run: $(BIN)
	$(BIN) --help

daemon: $(DAEMON_BIN)
	$(DAEMON_BIN) --help

clean:
	rm -rf $(BUILD_DIR)

$(BIN): $(APP_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS) $(SYSTEMD_LIBS)

$(DAEMON_BIN): $(DAEMON_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS) $(SYSTEMD_LIBS)

$(TEST_BIN): $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
