CC ?= cc
PKG_CONFIG ?= pkg-config

BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin

PACKAGES := vips json-c openssl
PACKAGE_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(PACKAGES) 2>/dev/null)
PACKAGE_LIBS := $(shell $(PKG_CONFIG) --libs $(PACKAGES) 2>/dev/null)

CPPFLAGS ?=
CPPFLAGS += -D_POSIX_C_SOURCE=200809L -Iinclude $(PACKAGE_CFLAGS)
CFLAGS ?= -O2
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -MMD -MP
LDFLAGS ?=
LDLIBS ?=
LDLIBS += $(PACKAGE_LIBS) -pthread -lm

COMMON_SOURCES := \
	src/common/error.c \
	src/common/util.c

STORAGE_SOURCES := \
	src/storage/image_content.c \
	src/storage/image_dedup.c \
	src/storage/imgfs_create.c \
	src/storage/imgfs_delete.c \
	src/storage/imgfs_insert.c \
	src/storage/imgfs_list.c \
	src/storage/imgfs_read.c \
	src/storage/imgfs_tools.c

NETWORK_SOURCES := \
	src/network/http_net.c \
	src/network/http_prot.c \
	src/network/socket_layer.c

CLI_SOURCES := \
	src/cli/imgfscmd.c \
	src/cli/imgfscmd_functions.c

SERVER_SOURCES := \
	src/server/imgfs_server.c \
	src/server/imgfs_server_service.c

COMMON_OBJECTS := $(COMMON_SOURCES:%.c=$(OBJ_DIR)/%.o)
STORAGE_OBJECTS := $(STORAGE_SOURCES:%.c=$(OBJ_DIR)/%.o)
NETWORK_OBJECTS := $(NETWORK_SOURCES:%.c=$(OBJ_DIR)/%.o)
CLI_OBJECTS := $(CLI_SOURCES:%.c=$(OBJ_DIR)/%.o)
SERVER_OBJECTS := $(SERVER_SOURCES:%.c=$(OBJ_DIR)/%.o)

CLI_BIN := $(BIN_DIR)/imgfscmd
SERVER_BIN := $(BIN_DIR)/imgfs_server

TCP_CLIENT_BIN := $(BIN_DIR)/tcp-test-client
TCP_SERVER_BIN := $(BIN_DIR)/tcp-test-server
HTTP_SERVER_BIN := $(BIN_DIR)/http-test-server
TEST_BINS := $(TCP_CLIENT_BIN) $(TCP_SERVER_BIN) $(HTTP_SERVER_BIN)
TEST_OBJECTS := $(patsubst %.c,$(OBJ_DIR)/%.o,$(wildcard tests/*.c))

.PHONY: all check check-deps clean test-tools

all: $(CLI_BIN) $(SERVER_BIN)

$(CLI_BIN): $(COMMON_OBJECTS) $(STORAGE_OBJECTS) $(CLI_OBJECTS) | check-deps
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(SERVER_BIN): $(COMMON_OBJECTS) $(STORAGE_OBJECTS) $(NETWORK_OBJECTS) $(SERVER_OBJECTS) | check-deps
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(TCP_CLIENT_BIN): $(OBJ_DIR)/tests/tcp-test-client.o $(COMMON_OBJECTS) $(OBJ_DIR)/src/network/socket_layer.o
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $^ -o $@

$(TCP_SERVER_BIN): $(OBJ_DIR)/tests/tcp-test-server.o $(COMMON_OBJECTS) $(OBJ_DIR)/src/network/socket_layer.o
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $^ -o $@

$(HTTP_SERVER_BIN): $(OBJ_DIR)/tests/http-test-server.o $(COMMON_OBJECTS) $(NETWORK_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $^ -pthread -o $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(STORAGE_OBJECTS) $(CLI_OBJECTS) $(SERVER_OBJECTS): | check-deps

check-deps:
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || { \
		echo "error: pkg-config is required to build ImgFS" >&2; \
		exit 1; \
	}
	@missing=""; \
	for package in $(PACKAGES); do \
		$(PKG_CONFIG) --exists $$package || missing="$$missing $$package"; \
	done; \
	if [ -n "$$missing" ]; then \
		echo "error: missing build dependencies:$$missing" >&2; \
		echo "Install their development packages and retry." >&2; \
		exit 1; \
	fi

test-tools: $(TEST_BINS)

check: all test-tools
	@echo "Build checks completed successfully."

clean:
	rm -rf $(BUILD_DIR)

ALL_OBJECTS := $(COMMON_OBJECTS) $(STORAGE_OBJECTS) $(NETWORK_OBJECTS) \
	$(CLI_OBJECTS) $(SERVER_OBJECTS) $(TEST_OBJECTS)
-include $(ALL_OBJECTS:.o=.d)
