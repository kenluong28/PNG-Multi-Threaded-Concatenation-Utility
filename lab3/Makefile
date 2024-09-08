
# Makefile, ECE252
# Yiqing Huang, 2019/06/12

CC = gcc       # compiler
CFLAGS = -Wall -g -std=c99 -Ilib # compilation flg
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lz -pthread -lcurl      # link with libz

# For students
OBJ_DIR = obj
SRC_DIR = lib
LIB_UTIL = $(OBJ_DIR)/catpng.o $(OBJ_DIR)/crc.o $(OBJ_DIR)/shm_stack.o $(OBJ_DIR)/zutil.o
SRCS   = catpng.c crc.c zutil.c paster2.c
OBJS  = $(OBJ_DIR)/paster2.o $(LIB_UTIL)

TARGETS= paster2

all: ${TARGETS}

paster2: $(OBJS)
	mkdir -p $(OBJ_DIR)
	$(LD) -o $@ $^ $(CFLAGS) $(LDLIBS) $(LDFLAGS)

$(OBJ_DIR)/paster2.o: paster2.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@ $(LDLIBS)

.PHONY: clean
clean:
	rm -f *.d *.o $(OBJ_DIR)/*.d $(OBJ_DIR)/*.o $(TARGETS)
