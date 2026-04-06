CC ?= cc

CPPFLAGS ?= -Isrc
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
TEST_BIN := $(BUILD_DIR)/test_linux_control_center

COMMON_SRCS := \
	$(SRC_DIR)/common/lcc_error.c \
	$(SRC_DIR)/common/lcc_log.c

APP_SRCS := \
	$(COMMON_SRCS) \
	$(SRC_DIR)/backend-amw0/amw0_backend.c \
	$(SRC_DIR)/profile-service/profile_service.c \
	$(SRC_DIR)/profile-service/profile_file.c \
	$(SRC_DIR)/cli/main.c

TEST_SRCS := \
	$(COMMON_SRCS) \
	$(SRC_DIR)/backend-amw0/amw0_backend.c \
	$(SRC_DIR)/profile-service/profile_service.c \
	$(SRC_DIR)/profile-service/profile_file.c \
	$(TEST_DIR)/test_linux_control_center.c

APP_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(APP_SRCS))
TEST_OBJS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(TEST_SRCS))

CFLAGS ?= $(CFLAGS_BASE) $(RELEASE_CFLAGS)

.PHONY: all clean debug test run

all: $(BIN)

debug: CFLAGS := $(CFLAGS_BASE) $(DEBUG_CFLAGS)
debug: clean all

test: $(TEST_BIN)
	$(TEST_BIN)

run: $(BIN)
	$(BIN) --help

clean:
	rm -rf $(BUILD_DIR)

$(BIN): $(APP_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(TEST_BIN): $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LDLIBS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
