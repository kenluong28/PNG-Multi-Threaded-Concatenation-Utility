#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include "crc.h"

#define PNG_SIG_SIZE    8 /* number of bytes of png image signature data */
#define CHUNK_LEN_SIZE  4 /* chunk length field size in bytes */
#define CHUNK_TYPE_SIZE 4 /* chunk type field size in bytes */
#define CHUNK_CRC_SIZE  4 /* chunk CRC field size in bytes */
#define DATA_IHDR_SIZE 13 /* IHDR chunk data field size */

typedef unsigned char U8;
typedef unsigned int  U32;
int isPNG = 0;

int is_png(U8 *buf, size_t n);
int get_png_height(FILE *fp);
int get_png_width(FILE *fp);
// int png_check(FILE *fp, U32* curr, U32* valid);
int png_info(char *path);
void findpng(char *directory);

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

    fseek(fp, 16, SEEK_SET);
    fread(&png_height, 4, 1, fp);

    png_height = ntohl(png_height);
    return png_height;
}

// retrieves PNG width
int get_png_width(FILE *fp) {
    U32 png_width;

    fseek(fp, 20, SEEK_SET);
    fread(&png_width, 4, 1, fp);

    png_width = ntohl(png_width);
    return png_width;
}

// check if PNG is corrupted
// int png_check(FILE *fp, U32* curr, U32* valid) {
//     fseek(fp, 8, SEEK_SET); // set file pointer to the start of IHDR

//     U32 curr_crc;
//     U32 valid_crc;
//     U32 data_chunk_size;
//     U32 chunk_length;

//     // checks the PNG chunk for IHDR, IDAT and IEND
//     for(int i = 0; i < 3; i++) {
//         fread(&data_chunk_size, 4, 1, fp); // retrieves size of PNG chunk Data field (Length)
//         data_chunk_size = ntohl(data_chunk_size);

//         chunk_length = CHUNK_TYPE_SIZE + data_chunk_size; // PNG chunk length to calculate CRC = "Type" + "Data"

//         U8 *crc_chunk = malloc(chunk_length);
//         fread(crc_chunk, 1, chunk_length, fp);

//         curr_crc = crc(crc_chunk, chunk_length); // calculates the crc value of the input PNG

//         fread(&valid_crc, 4, 1, fp); // retrieves the valid PNG crc value
//         valid_crc = ntohl(valid_crc);

//         if(valid_crc != curr_crc) {
//             *curr = curr_crc;
//             *valid = valid_crc;

//             free(crc_chunk);
//             return 1;
//         }

//         free(crc_chunk);
//     }

//     *curr = curr_crc;
//     *valid = valid_crc;
//     return 0;
// }

// checks if PNG file is valid and/or corrupted (acts as main function in pnginfo.c)
int png_info(char *path) {
    FILE *fp;
    U8 *buf = malloc(8);
    U32 curr_crc;
    U32 valid_crc;

    // checks if file can be opened
    if (access(path, F_OK) == 0) {
        fp = fopen(("%s", path), "rb");
    }else {
        free(buf);
        buf = NULL;
        return 0;
    }

    fread(buf, sizeof(U8), 8, fp); // read the file data into the buffer

    int result = is_png(buf, 8);

    // check if PNG file is valid and/or corrupted
    if(result == 1) {
        // int isCorrupted = png_check(fp, &curr_crc, &valid_crc);
        // if(isCorrupted == 0) {
            printf("%s\n", path);

            fclose(fp);
            free(buf);
            return 1;
        // } else {
        //     fclose(fp);
        //     free(buf);
        //     return 0;
        // }
    } else {
        fclose(fp);
        free(buf);
        return 0;
    }
}

// find PNG files within the directory
void findpng(char *directory) {
    struct dirent *dir_entries;
    DIR *dp;

    dp = opendir(directory);

    // cannot access directory
    if(dp == NULL) return;

    while((dir_entries = readdir(dp))) {
        if((strcmp(dir_entries->d_name, ".") != 0) && (strcmp(dir_entries->d_name, "..") != 0)) {  // check if "."-current directory or ".."-parent directory is passed (want to avoid infiinite loops)
            char path[1000];

            strcpy(path, directory);
            strcat(path, "/");
            strcat(path, dir_entries->d_name);

            // checks if PNG files are valid PNGs
            if(png_info(path) == 1) {
                isPNG = 1;
            }

            findpng(path);
        }
    }

    closedir(dp);
}

int main(int argc, char **argv) {
    char *path = argv[1];

    findpng(path);

    // no PNG file is found in the directory
    if(isPNG == 0) {
        printf("findpng: No PNG file found\n");
    }

    return 0;
}
