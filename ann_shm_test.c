#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <pthread.h>

#include "ann_shm.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <semaphore.h>

#ifdef USE64
typedef uint64_t ann_no_t;
#define ann_wait ann_wait64
#define ann_get  ann_get64
#define ann_next ann_next64
#define VER        "64bit"
#define LOCK_TYPE  ANN_STL_SPIN
#define CONCUR     ANN_STC_SIN_SOUT
#elif defined(USE16)
typedef uint16_t ann_no_t;
#define ann_wait ann_wait16
#define ann_get  ann_get16
#define ann_next ann_next16
#define VER "16bit"
#define LOCK_TYPE  ANN_STL_SPIN
#define CONCUR     ANN_STC_SIN_SOUT
#elif defined(USE32)
typedef uint32_t ann_no_t;
#define ann_wait ann_wait32
#define ann_get  ann_get32
#define ann_next ann_next32
#define VER "32bit"
#define LOCK_TYPE  ANN_STL_SPIN
#define CONCUR     ANN_STC_SIN_SOUT
#elif defined(USE_SEM32)
typedef uint32_t ann_no_t;
#define ann_wait ann_wait_sem32
#define ann_get  ann_get32
#define ann_next ann_next_sem32
#define VER "32bit(sem)"
#define LOCK_TYPE  ANN_STL_POSIX_SEM
#define CONCUR     ANN_STC_SIN_SOUT
#elif defined(USE_SEM_M32)
typedef uint32_t ann_no_t;
#define ann_wait ann_wait_sem_m32
#define ann_get  ann_get32
#define ann_next ann_next_sem_m32
#define VER "32bit(sem mthread)"
#define LOCK_TYPE  ANN_STL_POSIX_SEM
#define CONCUR     ANN_STC_MIN_MOUT
#elif defined(USE_M32)
typedef uint32_t ann_no_t;
#define ann_wait ann_wait_m32
#define ann_get  ann_get32
#define ann_next ann_next_m32
#define VER "32bit(mthread)"
#define LOCK_TYPE  ANN_STL_SPIN
#define CONCUR     ANN_STC_MIN_MOUT
#else
#error Unknown configuration to run
#endif

int verbose_output = 0;
int cpu_bench = 0;
int group_thres = 100; // 10.0%

uint32_t cells = 256;
uint32_t msg_size = 8;
uint32_t count = 1000000;

struct annihilator *pa = NULL;
volatile int started = 0;
sem_t sync_sem;

struct stat_data {
    int64_t ns;
};

struct stat_data *adj_matrix;
int numCPU;
int vec_i;
int vec_j;

void set_task(int j)
{
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(j, &cpuset);

    int s = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (s != 0) {
        perror("pthread_getaffinity_np");
        exit(2);
    }
}

void *wait_thread(void* arg)
{
    ann_no_t no;
    void* a;

    if (cpu_bench) {
        set_task(vec_i);
    }

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

void *prod_thread(void* arg)
{
    uint32_t i;
    void* a;
    ann_no_t no;
    pthread_t th;

    int res = pthread_create(&th, NULL, wait_thread, NULL);
    if (res) {
        exit(3);
    }

    if (cpu_bench) {
        set_task(vec_j);
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

    if (cpu_bench) {
        adj_matrix[vec_i * numCPU + vec_j].ns = ns;
        pthread_join(th, NULL);
        sem_post(&sync_sem);
    } else {
        printf("TEST(%s): C=%d M=%d N=%d took %12ld\n", VER,  cells, msg_size, count, ns);
        printf("AVG : %.3f M/s\n", (((double)count * 1000)) / ns);
    }

    return NULL;
}

static int uint64_t_cmp(const void *pa, const void *pb)
{
    uint64_t a, b;
    a = *(uint64_t*)pa;
    b = *(uint64_t*)pb;
    return a < b;
}

int main(int argc, char** argv)
{
    int opt;
    while ((opt = getopt(argc, argv, "t:vbc:m:n:h")) != -1) {
        switch (opt) {
        case 'v':
            verbose_output = 1;
            break;
        case 't':
            group_thres = atoi(optarg);
            break;
        case 'c':
            cells = atoi(optarg);
            break;
        case 'm':
            msg_size = atoi(optarg);
            break;
        case 'n':
            count = atoi(optarg);
            break;
        case 'b':
            cpu_bench = 1;
            break;
        default: /* '?' */
            fprintf(stderr, "Usage: %s [-c calls] [-m msg_size]\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    struct ann_stage_def nfo[2] = {
        { 1, 1, LOCK_TYPE, CONCUR },
        { 1, 1, LOCK_TYPE, CONCUR }
    };

    if (cpu_bench) {
        sem_init (&sync_sem, 0, 0);

        numCPU = sysconf( _SC_NPROCESSORS_ONLN );
        printf("Total CPUs: %d\n", numCPU);

        adj_matrix = malloc(sizeof(struct stat_data) * numCPU * numCPU );
        memset(adj_matrix, 0, sizeof(struct stat_data) * numCPU * numCPU);

        for (vec_i = 0; vec_i < numCPU; vec_i++) {
            for (vec_j = 0; vec_j < numCPU; vec_j++) {
                pthread_t th;

                if (vec_i == vec_j)
                    continue;

                pa = ann_create(cells, 2, msg_size, nfo);
                if (pa == NULL) {
                    exit(2);
                }

                int res = pthread_create(&th, NULL, prod_thread, NULL);
                if (res) {
                    exit(3);
                }

                sem_wait(&sync_sem);

                ann_destroy(pa);

                printf("."); fflush(stdout);
            }
        }

        printf("\n\n");

        int i, j;
        for (i = 0; i < numCPU; i++) {
            if (i == 0) {
                printf("    ");
                for (j = 0; j < numCPU; j++) {
                    printf(" %7d", j);
                }
                printf("\n");
            }

            printf(" %2d: ", i);
            for (j = 0; j < numCPU; j++) {
                uint64_t ns = adj_matrix[i * numCPU + j].ns;
                printf(" %7.3f",  (((double)count * 1000)) / ns);
            }
            printf("\n");
        }

        uint64_t *pns = malloc(sizeof(uint64_t) * numCPU * (numCPU - 1) / 2);
        uint8_t *grps = malloc(numCPU * (numCPU - 1) / 2);
        int k = 0;
        for (i = 0; i < numCPU; i++) {
            for (j = i + 1; j <numCPU; j++) {
                uint64_t ans, bns;
                ans = (uint64_t)count * 1000000  / adj_matrix[i * numCPU + j].ns;
                bns = (uint64_t)count * 1000000  / adj_matrix[j * numCPU + i].ns;

                pns[k++] = (ans > bns) ? ans : bns;
            }
        }

        qsort(pns, numCPU * (numCPU - 1) / 2, sizeof(uint64_t), uint64_t_cmp);
        if (verbose_output) {
            printf("Sorted: \n");
            for (k = 0; k < numCPU * (numCPU - 1) / 2; k++) {
                printf(" %7d", (int)pns[k]);
            }
            printf("\n");
        }

        i = 0; //Prev group start
        j = 0; //Current group index
        for (k = 0; k < numCPU * (numCPU - 1) / 2; k++) {
            uint64_t v = (pns[i] - pns[k]) * 1000 / pns[i];
            if (v < 0) v = -v;

            if (v > group_thres) {
                grps[j++] = k;
                i = k;
            }
        }
        grps[j++] = k - 1;

        int latency_groups = j;
        printf("\nLatency groups: %d\n", latency_groups);
        for (i = 0; i < latency_groups; i++) {
            int st = ( i == 0 ) ? 0 : grps[i-1];
            int en = grps[i] - 1;
            if (en < st) en = st;

            printf(" L%02d: %7d -- %7d\n", i, (int)pns[st],  (int)pns[en]);
        }


        return 0;

    } else {
        pa = ann_create(cells, 2, msg_size, nfo);
        if (pa == NULL) {
            exit(2);
        }

        prod_thread(NULL);

        ann_destroy(pa);
    }

    return 0;
}
