#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include "crc.h"
#include "zutil.h"

#define PNG_SIG_SIZE    8 /* number of bytes of png image signature data */
#define CHUNK_LEN_SIZE  4 /* chunk length field size in bytes */
#define CHUNK_TYPE_SIZE 4 /* chunk type field size in bytes */
#define CHUNK_CRC_SIZE  4 /* chunk CRC field size in bytes */
#define DATA_IHDR_SIZE 13 /* IHDR chunk data field size */

typedef unsigned char U8;
typedef unsigned int  U32;

void header_create(U8 *array);
void IHDR_create(U8 *array, U32 height_png, U32 width_png);
void IDATA_create(U8 *array, U32 idat_data_size, U32 *idat_data_field);
void IEND_create(U8 *array, U32 idat_data_size);

// create new PNG file Header
void header_create(U8 *array) {
    U8 header_data[PNG_SIG_SIZE] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}; // [0...7] -> stores the PNG file header

    memcpy(array, &header_data, PNG_SIG_SIZE);

    return;
}

// create new PNG file IHDR
void IHDR_create(U8 *array, U32 height_png, U32 width_png) {
    U32 ihdr_data_size = DATA_IHDR_SIZE;
    U32 ihdr_crc;

    height_png = ntohl(height_png);
    width_png = ntohl(width_png);
    ihdr_data_size = ntohl(ihdr_data_size);

    U8 *new_height_png = &height_png;
    U8 *new_width_png = &width_png;
    U8 *new_ihdr_data_size = &ihdr_data_size;

    // IHDR Length
    memcpy(array+8, new_ihdr_data_size, CHUNK_LEN_SIZE); // [8...11] -> stores value DATA_IHDR_SIZE (IHDR chunk Length = 4bytes) (chunk Data field length)

    // IHDR Type
    array[12] = 'I';
    array[13] = 'H';
    array[14] = 'D';
    array[15] = 'R'; // [12...15] -> stores IHDR chunk Type

    // IHDR Data
    memcpy(array+16, new_width_png, 4); // [16...19] -> stores IHDR chunk width Data field
    memcpy(array+20, new_height_png, 4); // [20...23] -> stores IHDR chunk height Data field

    array[24] = 8;
    array[25] = 6;
    array[26] = 0;
    array[27] = 0;
    array[28] = 0; // [24...28] -> stores the rest of IHDR chunk Data fields

    // IHDR CRC
    ihdr_crc = crc(&array[12], 17); // computes the IHDR PNG chunk's CRC value
    ihdr_crc = ntohl(ihdr_crc);
    U8 *new_ihdr_crc = &ihdr_crc;

    memcpy(array+29, new_ihdr_crc, CHUNK_CRC_SIZE); // [29...32] -> stores the IHDR chunk CRC

    return;
}

// create new PNG file IDAT
void IDATA_create(U8 *array, U32 idat_data_size, U32 *idat_data_field) {
    U32 idat_crc;

    idat_data_size = ntohl(idat_data_size);

    U8 *new_idat_data_size = &idat_data_size;

    // IDATA Length
    memcpy(array+33, new_idat_data_size, CHUNK_LEN_SIZE); // [33...36] -> stores the IDAT chunk Length (chunk Data field length)

    // IDATA Type
    array[37] = 'I';
    array[38] = 'D';
    array[39] = 'A';
    array[40] = 'T'; // [37...40] -> stores the IDAT chunk Type

    idat_data_size = ntohl(idat_data_size);

    // IDATA Data
    memcpy(array+41, idat_data_field, idat_data_size); // [41...n] -> stores the IDAT chunk Data fields

    // IDATA CRC
    idat_crc = crc(&array[37], idat_data_size+CHUNK_TYPE_SIZE); // computes the IDAT PNG chunk's CRC value
    idat_crc = ntohl(idat_crc);
    U8 *new_idat_crc = &idat_crc;

    memcpy(array+41+idat_data_size, new_idat_crc, CHUNK_CRC_SIZE); // [n...n+4] -> stores the IDAT chunk CRC

    return;
}

// create new PNG file IEND
void IEND_create(U8 *array, U32 idat_data_size) {
    U32 iend_data_size = 0;
    U8 *new_iend_data_size = &iend_data_size;
    U32 iend_crc;
    U32 iend_start_idx = idat_data_size + 45;

    // IEND Length
    memcpy(array+iend_start_idx, new_iend_data_size, CHUNK_LEN_SIZE); // [...] -> stores the IEND chunk Length (chunk Data field length)

    // IEND Type
    U8 iend_type[CHUNK_TYPE_SIZE] = {'I', 'E', 'N', 'D'};

    memcpy(array+iend_start_idx+4, iend_type, CHUNK_TYPE_SIZE); // [...] -> stores the IEND chunk Type

    // IEND has an empty Data field

    // IEND CRC
    iend_crc = crc(&array[iend_start_idx+4], CHUNK_TYPE_SIZE); // computes the IEND PNG chunk's CRC value
    iend_crc = ntohl(iend_crc);
    U8 *new_iend_crc = &iend_crc;

    memcpy(&array[iend_start_idx+8], new_iend_crc, CHUNK_CRC_SIZE); // [...] -> stores the IEND chunk CRC

    return;
}

int main(int argc, char **argv) {
    U32 png_height;
    U32 png_width;
    U32 catpng_height;
    U64 idat_data_size;
    U32 new_png_size;

    U8 **total_PNG_chunk = malloc(argc*sizeof(U8*));
    total_PNG_chunk[0] = NULL;

    U8 inf_buffer[1024*1024];
    U64 inf_buffer_length = 0;

    U8 cat_filtered_pxl_arr[1024*1024];

    U8 def_buffer[1024*1024];
    U64 def_buffer_length = 0;

    // retrieve each PNG width/height and IDAT data field (compressed)
    for(U32 i = 1; i < argc; i++) {
        FILE *fp;

        // checks if file can be opened
        if(access(argv[i], F_OK) == 0) {
            fp = fopen(("%s", argv[i]), "rb");
        }

        // retrieve PNG width
        fseek(fp, 16, SEEK_SET);
        fread(&png_width, 4, 1, fp);
        png_width = ntohl(png_width);

        // retrieve the height of each PNG strip
        fseek(fp, 20, SEEK_SET);
        fread(&png_height, 4, 1, fp);
        catpng_height += ntohl(png_height);

        // retrieve PNG IDAT chunk data field (compressed)
        fseek(fp, 33, SEEK_SET); // offset 33 -> start of IDAT (Header + IHDR)
        fread(&idat_data_size, 4, 1, fp); //  retrieves the IDAT chunk Length (chunk Data field length)
        idat_data_size = ntohl(idat_data_size);

        U8 *idat_chunk_data = malloc(idat_data_size);

        fseek(fp, 41, SEEK_SET); // offset 41 -> start of IDAT chunk Data field (Header + IHDR + IDAT chunk Length + IDAT chunk Type)
        fread(idat_chunk_data, 1, idat_data_size, fp); // retrieves the IDAT chunk Data

        total_PNG_chunk[i] = idat_chunk_data;

        fclose(fp);
    }

    // uncompress IDAT data and create concatentated filtered pixel array
    U64 total_inf_buffer = 0;
    for(U32 i = 1; i < argc; i++) {
        FILE *fp;

        // checks if file can be opened
        if(access(argv[i], F_OK) == 0) {
            fp = fopen(("%s", argv[i]), "rb");
        }

        // retrieve PNG IDAT chunk Length
        fseek(fp, 33, SEEK_SET); // offset 33 -> start of IDAT (Header + IHDR)
        fread(&idat_data_size, 4, 1, fp); //  retrieves the IDAT chunk Length (chunk Data field length)
        idat_data_size = ntohl(idat_data_size);

        int ret = mem_inf(inf_buffer, &inf_buffer_length, total_PNG_chunk[i], idat_data_size);

        if(ret == 0) {
            memcpy(cat_filtered_pxl_arr+total_inf_buffer, inf_buffer, inf_buffer_length);
            total_inf_buffer += inf_buffer_length;
        } else {
            zerr(ret);
            fprintf(stderr, "catpng: Input file in not a PNG ... ~exiting gracefully\n");
        }
        fclose(fp);
    }

    // compress IDAT data to create IDAT chunk
    mem_def(def_buffer, &def_buffer_length, cat_filtered_pxl_arr, total_inf_buffer, Z_DEFAULT_COMPRESSION);

    // create new PNG for the concatenated images
    new_png_size = PNG_SIG_SIZE+25+12+def_buffer_length+12; // Header(8) + IHDR(25) + IDAT(12+def_buffer_length) + IEND(12)

    U8 *new_png = malloc(new_png_size);

    header_create(new_png);
    IHDR_create(new_png, catpng_height, png_width);
    IDATA_create(new_png, def_buffer_length, def_buffer);
    IEND_create(new_png, def_buffer_length);

    // write concatenate the listed PNG images vertically to all.png
    FILE *fp = fopen("all.png", "w");
    fwrite(new_png, 1, new_png_size, fp);
    fclose(fp);

    // deallocation
    for(U32 i = 0; i < argc; i++) {
        free(total_PNG_chunk[i]);
    }

    free(total_PNG_chunk);
    free(new_png);
    return 0;
}
