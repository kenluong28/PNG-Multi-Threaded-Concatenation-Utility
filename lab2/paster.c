#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include <pthread.h>
#include <getopt.h>
#include "catpng.h"

// definitions
#define IMG_URL "http://ece252-1.uwaterloo.ca:2520/image?img="
#define IMG_URL_2 "http://ece252-2.uwaterloo.ca:2520/image?img="
#define IMG_URL_3 "http://ece252-3.uwaterloo.ca:2520/image?img="
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define ECE252_HEADER "X-Ece252-Fragment: "

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

// struct to hold thread arguments
struct pthread_arg {
    int ptid; // thread id
    int server; // disgnated web server for PNG
};

// global variables
int num_strips; // track number of strips
pthread_mutex_t lock; // mutex initialization
struct recv_buf strips_buf[50]; // hold PNG strips
int strip_ids[50]; // array to keep track of the PNG strips (flag for having a full PNG image)

// function declarations
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
void create_pthreads(pthread_t *thread, void *arg, int server, int num_threads);
void *run(void *arg);

/* write callback function to save a copy of received data in RAM. */
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

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

/* output data in memory to a file. */
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
        fprintf(stderr, "write_file: incomplete write!\n");
        return -3;
    }
    return fclose(fp);
}

/* header callbacks */
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

    /* look for X-Ece252-fragment for strip number */
    if (realsize > strlen(ECE252_HEADER) && strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0)
    {
        /* extract img sequence number */
        p->seq = atoi(p_recv + strlen(ECE252_HEADER));
    }

    return realsize;
}

// use created threads to retrieve all image segments
void *run(void *arg) {
    int id;
    int server;
    int ctr_strip;
    int max_strip;

    struct pthread_arg *t_arg = arg;
    char * ret_t = NULL;

    id = t_arg->ptid;
    server = t_arg->server;

    pthread_mutex_lock(&lock);
    ctr_strip = num_strips;
    pthread_mutex_unlock(&lock);

    max_strip = 50;

    CURL *curl_handle;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_DEFAULT);

    char url[256];
    RECV_BUF recv_buf;

    // distribute threads to servers
    if (id % 3 == 0) {
        sprintf(url, "%s%d", IMG_URL, server);
    } else if (id % 3 == 1) {
        sprintf(url, "%s%d", IMG_URL_2, server);
    } else {
        sprintf(url, "%s%d", IMG_URL_3, server);
    }

    while (ctr_strip < max_strip)
    {
        /* init a curl session */
        curl_handle = curl_easy_init();

        if (curl_handle == NULL) {
            fprintf(stderr, "curl_easy_init: returned NULL\n");
            return (void*) ret_t;
        }

        recv_buf_init(&recv_buf, BUF_SIZE);

        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);

        /* register write call back function to process received data */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
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

        if( res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            recv_buf_cleanup(&recv_buf);
            curl_easy_cleanup(curl_handle);
            return (void*) ret_t;
        } else if (res != 0) {
            fprintf(stderr, "One of the threads failed with an error \n");
            return (void*) ret_t;
        } else {
            // if curl was a success
            pthread_mutex_lock(&lock);

            if ((strip_ids[recv_buf.seq] == 0) && (num_strips <= max_strip)) // if strip is not yet found and keep going to all 50 strips are found
            {
                strip_ids[recv_buf.seq] = 1; // mark that specific strip as found
                strips_buf[recv_buf.seq] = recv_buf; // populate the stip into the strips_buf

                num_strips++;
                ctr_strip = num_strips;
            } else {
                ctr_strip = num_strips;

                curl_easy_cleanup(curl_handle);
                recv_buf_cleanup(&recv_buf);
                pthread_mutex_unlock(&lock);

                continue;
            }

            pthread_mutex_unlock(&lock);
            curl_easy_cleanup(curl_handle);
        }

    }

    /* cleaning up */
    curl_global_cleanup();

    return (void*) ret_t;
}

// create threads
void create_pthreads(pthread_t *thread, void *arg, int server, int num_threads)
{
    struct pthread_arg *t_arg = arg;

    for (int i = 0; i < num_threads; i++)
    {
        t_arg[i].ptid = i;
        t_arg[i].server = server;
        pthread_create(thread+i, NULL, run, t_arg+i);
    }

    return;
}

int main( int argc, char** argv )
{
    int c;
    int t = 1;
    int n = 1;
    char *str = "option requires an argument";

    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\n mutex init failed \n");
        return 1;
    }

    // retrieve t and n values from input
    while ((c = getopt (argc, argv, "t:n:")) != -1) {
        switch (c) {
        case 't':
	    t = strtoul(optarg, NULL, 10);
	    printf("option -t specifies a value of %d.\n", t);
	    if (t <= 0) {
                fprintf(stderr, "%s: %s > 0 -- 't'\n", argv[0], str);
                return -1;
            }
            break;
        case 'n':
            n = strtoul(optarg, NULL, 10);
	    printf("option -n specifies a value of %d.\n", n);
            if (n <= 0 || n > 3) {
                fprintf(stderr, "%s: %s 1, 2, or 3 -- 'n'\n", argv[0], str);
                return -1;
            }
            break;
        default:
            return -1;
        }
    }

    if (t > 50) {
        printf("exceeded maximum attempts to fulfill the request \n");
        return 0;
    }

    // create the processor threads
    pthread_t *thread = malloc(t * sizeof(pthread_t));
    struct pthread_arg thread_arg[t];
    create_pthreads(thread, &thread_arg, n, t);

    // waits for completion of all threads
    for (int i = 0; i < t; i++)
    {
        pthread_join(thread[i], NULL);
    }

    catpng2(argc, argv, strips_buf);

    for (int i = 0; i < 50; i++)
    {
        recv_buf_cleanup(&strips_buf[i]);
    }

    free(thread);
    pthread_mutex_destroy(&lock);
    return 0;
}
