#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "crc.h"

#define PNG_SIG_SIZE    8 /* number of bytes of png image signature data */
#define CHUNK_LEN_SIZE  4 /* chunk length field size in bytes */
#define CHUNK_TYPE_SIZE 4 /* chunk type field size in bytes */
#define CHUNK_CRC_SIZE  4 /* chunk CRC field size in bytes */
#define DATA_IHDR_SIZE 13 /* IHDR chunk data field size */

typedef unsigned char U8;
typedef unsigned int  U32;

typedef struct chunk {
    U32 length;  /* length of data in the chunk, host byte order */
    U8  type[4]; /* chunk type */
    U8  *p_data; /* pointer to location where the actual data are */
    U32 crc;     /* CRC field  */
} *chunk_p;

/* note that there are 13 Bytes valid data, compiler will padd 3 bytes to make
   the structure 16 Bytes due to alignment. So do not use the size of this
   structure as the actual data size, use 13 Bytes (i.e DATA_IHDR_SIZE macro).
 */
typedef struct data_IHDR {// IHDR chunk data
    U32 width;        /* width in pixels, big endian   */
    U32 height;       /* height in pixels, big endian  */
    U8  bit_depth;    /* num of bits per sample or per palette index.
                         valid values are: 1, 2, 4, 8, 16 */
    U8  color_type;   /* =0: Grayscale; =2: Truecolor; =3 Indexed-color
                         =4: Greyscale with alpha; =6: Truecolor with alpha */
    U8  compression;  /* only method 0 is defined for now */
    U8  filter;       /* only method 0 is defined for now */
    U8  interlace;    /* =0: no interlace; =1: Adam7 interlace */
} *data_IHDR_p;

/* A simple PNG file format, three chunks only*/
typedef struct simple_PNG {
    struct chunk *p_IHDR;
    struct chunk *p_IDAT;  /* only handles one IDAT chunk */
    struct chunk *p_IEND;
} *simple_PNG_p;

int is_png(U8 *buf, size_t n);
int get_png_height(FILE *fp);
int get_png_width(FILE *fp);
int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence);
int png_check(FILE *fp, U32* curr, U32* valid);

// checks if png is valid
int is_png(U8 *buf, size_t n) {
    U8 valid_data[] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

    // checks if PNG file Header matches a valid PNG file Header
    for(int i = 0; i < n; i++) {
        if(buf[0] != valid_data[0]) return -1;
    }

    return 1;
}

// retrieves PNG height
int get_png_height(FILE *fp) {
    U32 png_height;

    fseek(fp, 20, SEEK_SET);
    fread(&png_height, 4, 1, fp);

    png_height = ntohl(png_height);
    return png_height;
}

// retrieves PNG width
int get_png_width(FILE *fp) {
    U32 png_width;

    fseek(fp, 16, SEEK_SET);
    fread(&png_width, 4, 1, fp);

    png_width = ntohl(png_width);
    return png_width;
}

// check if PNG is corrupted
int png_check(FILE *fp, U32* curr, U32* valid) {
    fseek(fp, 8, SEEK_SET); // set file pointer to the start of IHDR

    U32 curr_crc;
    U32 valid_crc;
    U32 data_chunk;
    U32 chunk_length;

    // checks the PNG chunk for IHDR, IDAT and IEND
    for(int i = 0; i < 3; i++) {
        fread(&data_chunk, 4, 1, fp); // retrieves "Data" field of PNG chunk
        data_chunk = ntohl(data_chunk);

        chunk_length = 4 + data_chunk; // PNG chunk length = "Type" + "Data"

        U8 *crc_chunk = malloc(chunk_length);
        fread(crc_chunk, 1, chunk_length, fp);

        curr_crc = crc(crc_chunk, chunk_length); // calculates the crc value of the input PNG

        fread(&valid_crc, 4, 1, fp); // retrieves the valid PNG crc value
        valid_crc = ntohl(valid_crc);

        if(valid_crc != curr_crc) {
            *curr = curr_crc;
            *valid = valid_crc;

            free(crc_chunk);
            return 1;
        }

        free(crc_chunk);
    }

    *curr = curr_crc;
    *valid = valid_crc;
    return 0;
}

int main(int argc, char **argv) {
    U32 curr_crc;
    U32 valid_crc;

    FILE *fp = fopen(argv[1], "r");
    U8 *buf = malloc(8);

    fread(buf, sizeof(U8), 8, fp);

    int result = is_png(buf, 8); // checks if PNG is a valid PNG file
    int isCorrupted = png_check(fp, &curr_crc, &valid_crc); // return value if PNG file is corrupted

    // check if PNG file is valid and/or corrupted
    if(result == 1 && isCorrupted == 0) {
        printf("%s: %d x %d\n", argv[1], get_png_height(fp), get_png_width(fp));
    } else if(result == 1 && isCorrupted == 1) {
        printf("%s: %d x %d\n", argv[1], get_png_height(fp), get_png_width(fp));
        printf("IDAT chunk CRC error: computed %x, expected %x\n", curr_crc, valid_crc);
    } else {
        printf("%s: Not a PNG file\n", argv[1]);
    }

    fclose(fp);
    free(buf);
    return 0;
}
