#include "ann_shm.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifdef USE64
typedef uint64_t ann_no_t;
#define ann_wait ann_wait64
#define ann_get  ann_get64
#define ann_next ann_next64
#define VER        "64bit"
#define LOCK_TYPE  ANN_STL_SPIN
#elif defined(USE16)
typedef uint16_t ann_no_t;
#define ann_wait ann_wait16
#define ann_get  ann_get16
#define ann_next ann_next16
#define VER "16bit"
#define LOCK_TYPE  ANN_STL_SPIN
#elif defined(USE32)
typedef uint32_t ann_no_t;
#define ann_wait ann_wait32
#define ann_get  ann_get32
#define ann_next ann_next32
#define VER "32bit"
#define LOCK_TYPE  ANN_STL_SPIN
#elif defined(USE_SEM32)
typedef uint32_t ann_no_t;
#define ann_wait ann_wait_sem32
#define ann_get  ann_get32
#define ann_next ann_next_sem32
#define VER "32bit(sem)"
#define LOCK_TYPE  ANN_STL_POSIX_SEM
#else
typedef uint32_t ann_no_t;
#define ann_wait ann_wait_sem_m32
#define ann_get  ann_get32
#define ann_next ann_next_sem32
#define VER "32bit(sem m_wait)"
#define LOCK_TYPE  ANN_STL_POSIX_SEM

#endif

struct annihilator *pa;
volatile int started;
void *wait_thread(void* arg)
{
    ann_no_t no;
    void* a;
    started = 1;

    for (;;) {
        no = ann_wait(pa, 1);
        a = ann_get(pa, no);
        if (*(char*)a == 1) {
            return 0;
        }
        ann_next(pa, 1, no);
    }
}

int main(int argc, char** argv)
{
    uint32_t cells = 256;
    uint32_t msg_size = 8;
    uint32_t count = 1000000;
    uint32_t i;
    int opt;
    void* a;
    ann_no_t no;

    while ((opt = getopt(argc, argv, "c:m:n:h")) != -1) {
        switch (opt) {
        case 'c':
            cells = atoi(optarg);
            break;
        case 'm':
            msg_size = atoi(optarg);
            break;
        case 'n':
            count = atoi(optarg);
            break;
        default: /* '?' */
            fprintf(stderr, "Usage: %s [-c calls] [-m msg_size]\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    struct ann_stage_def nfo[2] = {
        { 1, 1, LOCK_TYPE, 1 },
        { 1, 1, LOCK_TYPE, 1 }
    };

    pa = ann_create(cells, 2, msg_size, nfo);
    if (pa == NULL) {
        exit(2);
    }

    pthread_t th;
    int res = pthread_create(&th, NULL, wait_thread, NULL);
    if (res) {
        exit(3);
    }

    while (!started);

    struct timespec ts1, ts2;
    res = clock_gettime(CLOCK_REALTIME, &ts1);
    for (i = 0; i < count; i++) {
        no = ann_wait(pa, 0);
        a = ann_get(pa, no);
        (*(char*)a) = 0;
         ann_next(pa, 0, no);
    }
    no = ann_wait(pa, 0);
    a = ann_get(pa, no);
    (*(char*)a) = 1;
     ann_next(pa, 0, no);
    res = clock_gettime(CLOCK_REALTIME, &ts2);

    int64_t ns = (ts2.tv_sec -ts1.tv_sec) * 1000000000 +  (ts2.tv_nsec -ts1.tv_nsec);

    printf("TEST(%s): C=%d M=%d N=%d took %12ld\n", VER,  cells, msg_size, count, ns);
    printf("AVG : %.3f M/s\n", (((double)count * 1000)) / ns);

    return 0;
}
