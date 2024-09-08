#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <pthread.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>
#include "lib/catpng.h"
#include "lib/shm_stack.h"

#define IMG_URL "http://ece252-1.uwaterloo.ca:2530/image?img="
#define IMG_URL_2 "http://ece252-2.uwaterloo.ca:2530/image?img="
#define IMG_URL_3 "http://ece252-3.uwaterloo.ca:2530/image?img="
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define SHM_SIZE 256
#define SEM_PROC 1
#define NUM_SEMS 5
#define PNG_SIG_SIZE 8 /* number of bytes of png image signature data */

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

/* global variables */
sem_t *sems;
int *cntr;
int x; // 'X' = # of ms that a consumer sleeps before it starts to process the image data
char *cat_buf; // global data structure that holds the concatenated image data
struct int_stack *pstack; // global buffer

/* function declarations */
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
void producer(int n, int m);
void consumer(int i);

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

    if (realsize > strlen(ECE252_HEADER) &&
	strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {

        /* extract img sequence number */
	p->seq = atoi(p_recv + strlen(ECE252_HEADER));

    }
    return realsize;
}

int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;

    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	    return 2;
    }

    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;
    return 0;
}

int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL) {
	    return 1;
    }

    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}

size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size) { /* hope this rarely happens */
        fprintf(stderr, "User buffer is too small, abort...\n");
        abort();
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        fprintf(stderr, "write_file: imcomplete write!\n");
        return -3;
    }
    return fclose(fp);
}

/* producer function -> make requests to the lab web server and fetch all 50 distinct image segments */
void producer(int n, int m)
{
    char *ret_t = NULL;
    char url[256];

    while (1) {
        sem_wait(&sems[0]);
        int seq_num = cntr[0]++;
        sem_post(&sems[0]);

        if (seq_num >= 50) {
            //printf("exceeded maximum attempts to fulfill the request \n");
            break;
        }

        /* selecting machine */
        if (m == 0) {
            sprintf(url, "%s%d%s%d", IMG_URL, n, "&part=", seq_num);
        } else if (m == 1) {
            sprintf(url, "%s%d%s%d", IMG_URL_2, n, "&part=", seq_num);
        } else {
            sprintf(url, "%s%d%s%d", IMG_URL_3, n, "&part=", seq_num);
        }

        /* start of CURL request */
        CURL *curl_handle;
        CURLcode res;
        RECV_BUF recv_buf;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_handle = curl_easy_init();

        if (curl_handle == NULL) {
            fprintf(stderr, "curl_easy_init: returned NULL\n");
            return (void*) ret_t;
        }

        recv_buf_init(&recv_buf, BUF_SIZE);

        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        /* register write call back function to process received data */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl);
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);
            /* register header call back function to process received header data */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);
        /* some servers requires a user-agent field */
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        /* get it! */
        res = curl_easy_perform(curl_handle);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            recv_buf_cleanup(&recv_buf);
            curl_easy_cleanup(curl_handle);
            return (void*) ret_t;
        }

        if (res != 0) {
            fprintf(stderr, "One of the threads failed with an error \n");
            return (void*) ret_t;
        }
        /* end of CURL request */

        /* push items into global buffer */
        sem_wait(&sems[4]);
        sem_wait(&sems[1]);
        push(pstack, &recv_buf);
        sem_post(&sems[1]);
        sem_post(&sems[3]);

        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
    }

    return (void*) ret_t;
}

/* consumer function -> reads image segments out of the buffer, then sleeps for X ms specified by the user */
/* validate received image segment and inflate the received IDAT data and copy the inflated data into it's proper place inside the memory */
void consumer(int i)
{
    char* ret_t = NULL;
    int data_size;
    int inf_buffer_length;

    while (1) {
        sem_wait(&sems[2]);
        int items = cntr[1]++;
        sem_post(&sems[2]);

        if (items >= 50) {
            //printf("exceeded maximum attempts to fulfill the request \n");
            break;
        }

        /* fetch image data from global buffer */
        sem_wait(&sems[3]);
        sem_wait(&sems[1]);

        struct recv_buf_flat item;
        pop(pstack, &item);

        data_size = 0;

        memcpy(&data_size, item.buf+33, 4);

        data_size = ntohl(data_size);

        char chunk_data[data_size];
        memcpy(chunk_data, item.buf+41, data_size);

        int ret = mem_inf(inf_buffer, &inf_buffer_length, chunk_data, data_size);
        memcpy(cat_buf+(item.seq*9606), inf_buffer, inf_buffer_length);

        sem_post(&sems[1]);
        sem_post(&sems[4]);

        usleep(x*1000); // 'X' -> # of ms that a consumer sleeps before it starts to process the image data
    }

    return (void*) ret_t;
}

int main(int argc, char** argv)
{
    int b = atoi(argv[1]); // buffer size
    int p = atoi(argv[2]); // # of producers
    int c = atoi(argv[3]); // # of consumers
    x = atoi(argv[4]); // # of ms that a consumer sleeps before it starts to process the image data
    int n = atoi(argv[5]); // image # requested to server

    const int NUM_CHILD = p+c;
    int STACK_SIZE = b;
    int cat_buf_size = 50*9606;

    int i=0;
    pid_t pid=0;
    pid_t cpids[NUM_CHILD];
    int state;
    int shm_size = sizeof_shm_stack(STACK_SIZE);

    /* measure the time before creating the first process */
    double times[2];
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;

    /* allocate four shared memory regions */
    int shmid_stack = shmget(IPC_PRIVATE, shm_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_sems = shmget(IPC_PRIVATE, sizeof(sem_t) * NUM_SEMS, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_buf = shmget(IPC_PRIVATE, cat_buf_size, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    int shmid_cntr = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    if (shmid_stack == -1 || shmid_sems == -1 || shmid_buf == -1 || shmid_cntr == -1) {
        perror("shmget");
        abort();
    }

    /* attach to shared memory regions */
    pstack = shmat(shmid_stack, NULL, 0);
    sems = shmat(shmid_sems, NULL, 0);
    cat_buf = shmat(shmid_buf, NULL, 0);
    cntr = shmat(shmid_cntr, NULL, 0);

    if (pstack == (void *) -1 || sems == (void *) -1 || cat_buf == (void *) -1 || cntr == (void *) -1) {
        perror("shmat");
        abort();
    }

    /* initialize shared memory */
    // sems = [ Mutex1 , Mutex2 , Mutex3 , Items , Spaces ]
    memset(cntr, 0, SHM_SIZE);
    init_shm_stack(pstack, STACK_SIZE);
    if (sem_init(&sems[0], SEM_PROC, 1) != 0) {
        perror("sem_init(sem[0])");
        abort();
    }
    if (sem_init(&sems[1], SEM_PROC, 1) != 0) {
        perror("sem_init(sem[1])");
        abort();
    }
    if (sem_init(&sems[2], SEM_PROC, 1) != 0) {
        perror("sem_init(sem[2])");
        abort();
    }
    if (sem_init(&sems[3], SEM_PROC, 0) != 0) {
        perror("sem_init(sem[3])");
        abort();
    }
    if (sem_init(&sems[4], SEM_PROC, b) != 0) {
        perror("sem_init(sem[4])");
        abort();
    }

    /* fork multiple producer processes and multiple consumer processes */
    for (i = 0; i < NUM_CHILD; i++) {
        pid = fork();
        if (pid > 0) {              /* parent proc */
            cpids[i] = pid;
        } else if (pid == 0) {      /* child proc */
            if (i < p) {    /* producers */
                /* give producer image # and image seq # */
                producer(n, i%3);

                if (shmdt(pstack->all_buf) != 0) {
                    perror("shmdt");
                    abort();
                }
                if (shmdt(pstack) != 0) {
                    perror("shmdt");
                    abort();
                }
                if (shmdt(sems) != 0) {
                    perror("shmdt");
                    abort();
                }
                if (shmdt(cat_buf) != 0) {
                    perror("shmdt");
                    abort();
                }
                if (shmdt(cntr) != 0) {
                    perror("shmdt");
                    abort();
                }
            } else {    /* consumers */
                /* give consumer */
                consumer(i-p);

                if (shmdt(pstack->all_buf) != 0) {
                    perror("shmdt");
                    abort();
                }
                if (shmdt(pstack) != 0) {
                    perror("shmdt");
                    abort();
                }
                if (shmdt(sems) != 0) {
                    perror("shmdt");
                    abort();
                }
                if (shmdt(cat_buf) != 0) {
                    perror("shmdt");
                    abort();
                }
                if (shmdt(cntr) != 0) {
                    perror("shmdt");
                    abort();
                }
            }
            break;
        } else {
            perror("fork");
            abort();
        }
    }

    /* parent process */
    /* wait for all the child processes to terminate and then start to process the data structure that holds the concatenated image data and create the final all.png file */
    if (pid > 0) {
        for (i = 0; i < NUM_CHILD; i++) {
            waitpid(cpids[i], &state, 0);
        }

        U32 png_width = 400;
        U32 catpng_height = 300;
        U64 def_buffer_length = 0;

        /* compress IDAT data to create IDAT chunk */
        int ret = mem_def(def_buffer, &def_buffer_length, cat_buf, cat_buf_size, Z_DEFAULT_COMPRESSION);

        if (ret == 0) { /* success */
            //printf("original len = %d, len_def = %lu\n", cat_buf_size, def_buffer_length);
        } else { /* failure */
            fprintf(stderr,"mem_def failed. ret = %d.\n", ret);
            return ret;
        }

        /* create new PNG for the concatenated images */
        U32 new_png_size = PNG_SIG_SIZE+25+12+def_buffer_length+12; // Header(8) + IHDR(25) + IDAT(12+def_buffer_length) + IEND(12)

        U8 *new_png = malloc(new_png_size);

        header_create(new_png);
        IHDR_create(new_png, catpng_height, png_width);
        IDATA_create(new_png, def_buffer_length, def_buffer);
        IEND_create(new_png, def_buffer_length);

        /* write concatenate the listed PNG images vertically to all.png */
        FILE *fp = fopen("all.png", "w");
        fwrite(new_png, 1, new_png_size, fp);
        fclose(fp);

        /* measure the time after the last image segment is consumed and the concatenated all.png image is generated */
        if (gettimeofday(&tv, NULL) != 0) {
            perror("gettimeofday");
            abort();
        }
        times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
        printf("paster2 execution time: %.6lf seconds\n", times[1] - times[0]);

        /* cleanup */
        free(new_png);

        /* store nested shmid before detach */
        int all_buf_id = pstack->shmid;

        if (shmdt(pstack->all_buf) != 0) {
            perror("shmdt");
            abort();
        }
        if ( shmdt(pstack) != 0 ) {
            perror("shmdt");
            abort();
        }
        if ( shmdt(sems) != 0 ) {
            perror("shmdt");
            abort();
        }
        if ( shmdt(cat_buf) != 0 ) {
            perror("shmdt");
            abort();
        }
        if ( shmdt(cntr) != 0 ) {
            perror("shmdt");
            abort();
        }

        /* We do not use free() to release the shared memory, use shmctl() */
        if (shmctl(all_buf_id, IPC_RMID, NULL) == -1) {
            perror("shmctl");
            abort();
        }
        if (shmctl(shmid_stack, IPC_RMID, NULL) == -1) {
            perror("shmctl");
            abort();
        }
        if (shmctl(shmid_sems, IPC_RMID, NULL) == -1) {
            perror("shmctl");
            abort();
        }
        if (shmctl(shmid_buf, IPC_RMID, NULL) == -1) {
            perror("shmctl");
            abort();
        }
        if (shmctl(shmid_cntr, IPC_RMID, NULL) == -1) {
            perror("shmctl");
            abort();
        }
    }

    return 0;
}
