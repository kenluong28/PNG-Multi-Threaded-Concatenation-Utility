#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include "crc.h"
#include "zutil.h"

typedef unsigned char U8;
typedef unsigned int  U32;

typedef struct recv_buf {
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes */
    size_t max_size; /* max capacity of buf in bytes */
    int seq; /* img sequence number */
} RECV_BUF;

void header_create(U8 *array);
void IHDR_create(U8 *array, U32 height_png, U32 width_png);
void IDATA_create(U8 *array, U32 idat_data_size, U32 *idat_data_field);
void IEND_create(U8 *array, U32 idat_data_size);
int catpng2(int argc, char **argv, struct recv_buf *arg);
