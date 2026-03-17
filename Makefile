# Output locations
BUILD_DIR := build
OBJ_DIR   := $(BUILD_DIR)/obj
BIN_DIR   := $(BUILD_DIR)/bin
TARGET    := $(BIN_DIR)/pgn-extract

# Keep your existing source list explicit (recommended)
SRCS := grammar.c lex.c map.c decode.c moves.c lists.c apply.c output.c eco.c \
        lines.c end.c main.c hashing.c argsfile.c mymalloc.c fenmatcher.c \
        taglines.c zobrist.c csvreader.c playerhashtable.c

OBJS := $(SRCS:%.c=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# Existing flags (remove -c from CFLAGS if you prefer, either works)
DEBUGINFO=-g
OPTIMISE=-O3
CC=gcc
LIBS=-lm

CFLAGS += -pedantic -Wall -Wshadow -Wformat -Wpointer-arith \
          -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings \
          -Wsign-compare -Wimplicit-function-declaration $(DEBUGINFO) \
          -I/usr/local/lib/ansi-include -std=c99 $(CPPFLAGS) $(OPTIMISE)

.PHONY: all clean distclean purify
all: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(DEBUGINFO) $(CPPFLAGS) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

# Generic compile rule: .c in root -> .o in build/obj
$(OBJ_DIR)/%.o: %.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

# Pull in auto-generated header deps
-include $(DEPS)

clean:
	rm -f $(OBJ_DIR)/*.o $(OBJ_DIR)/*.d $(TARGET)

distclean: clean
	rm -rf $(BUILD_DIR)

purify: $(OBJS) | $(BIN_DIR)
	purify $(CC) $(DEBUGINFO) $(OBJS) -o $(TARGET)