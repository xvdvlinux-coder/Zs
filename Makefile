CC ?= gcc
CFLAGS ?= -Wall -Wextra -std=c11 -O2 -Iinclude -D_POSIX_C_SOURCE=200809L
BUILD_DIR = build

COMMON_SRCS = src/zs_arena.c src/zs_source.c src/zs_lexer.c src/zs_ast.c src/zs_parser.c src/zs_types.c src/zs_symtab.c src/zs_checker.c src/zs_codegen_arm64.c
COMMON_OBJS = $(patsubst src/%.c, $(BUILD_DIR)/%.o, $(COMMON_SRCS))

all: $(BUILD_DIR)/test_lexer $(BUILD_DIR)/test_parser $(BUILD_DIR)/test_checker $(BUILD_DIR)/test_codegen $(BUILD_DIR)/test_dual_defer $(BUILD_DIR)/zsc

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_lexer: $(COMMON_OBJS) $(BUILD_DIR)/test_lexer.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_lexer.o: src/test_lexer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_parser: $(COMMON_OBJS) $(BUILD_DIR)/test_parser.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_parser.o: src/test_parser.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_checker: $(COMMON_OBJS) $(BUILD_DIR)/test_checker.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_checker.o: src/test_checker.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_codegen: $(COMMON_OBJS) $(BUILD_DIR)/test_codegen.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_codegen.o: src/test_codegen.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_dual_defer: $(COMMON_OBJS) $(BUILD_DIR)/test_dual_defer.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_dual_defer.o: src/test_dual_defer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/zsc: $(COMMON_OBJS) $(BUILD_DIR)/zsc_main.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/zsc_main.o: src/zsc_main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_runtime_io: $(COMMON_OBJS) $(BUILD_DIR)/test_runtime_io.o
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/test_runtime_io.o: src/test_runtime_io.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

test-lexer: $(BUILD_DIR)/test_lexer
	./$(BUILD_DIR)/test_lexer

test-parser: $(BUILD_DIR)/test_parser
	./$(BUILD_DIR)/test_parser

test-checker: $(BUILD_DIR)/test_checker
	./$(BUILD_DIR)/test_checker

test-codegen: $(BUILD_DIR)/test_codegen
	./$(BUILD_DIR)/test_codegen

test-dual-defer: $(BUILD_DIR)/test_dual_defer
	./$(BUILD_DIR)/test_dual_defer

test-runtime-io: $(BUILD_DIR)/test_runtime_io $(BUILD_DIR)/zsc
	./$(BUILD_DIR)/test_runtime_io

test-all: test-lexer test-parser test-checker test-codegen test-dual-defer test-runtime-io

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean test-lexer test-parser test-checker test-codegen test-dual-defer test-runtime-io test-all
