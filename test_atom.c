#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <stdint.h>
#include <pthread.h>

#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <semaphore.h>

#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#include <xmmintrin.h>
#include <assert.h>




#define MAX_CPUS     64
static pthread_t g_thr[MAX_CPUS];
static int g_cpu_afinity_list[MAX_CPUS];
static int g_cpu_cnt = 0;
static int g_thread_id = 0;

static volatile int started = 0;
static int threads = 1;
static int cpu_bench = 0;
static int verbose_output = 0;

static uint64_t count = 500*1000000;

enum AtomicTests {
    AT_XADD,
    AT_BOOL_CAS,
    AT_BOOL_DCAS,
    AT_XCHG
};

static enum AtomicTests test = AT_XADD;

struct stat_data {
    int64_t ns;
};

static struct stat_data *adj_matrix;
static int numCPU;
static int vec_i;
static int vec_j;

struct Test {
    char cache_skip[64];
    union {
        uint64_t data;
    };
    uint64_t data2;
    char cache_d[64 - 8 - 8];
} __attribute__((aligned(64)));

struct Test g_t;

static void set_task(int j)
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


static void *thread(void* arg)
{
    uint64_t i;

    if (cpu_bench) {
        set_task(vec_i);
    } else if (g_cpu_cnt > 0) {
        int id = __sync_fetch_and_add(&g_thread_id, 1);
        set_task(g_cpu_afinity_list[id]);
    }

    __sync_fetch_and_add(&started, 1);

    while (started != threads) {
        _mm_pause();
    }

    struct timespec ts1, ts2;
    int res = clock_gettime(CLOCK_MONOTONIC, &ts1);

    switch (test) {
    case AT_XADD:
        for (i = 0; i < count; ++i) {
            __sync_fetch_and_add(&g_t.data, 1);
        }
        break;

    case AT_BOOL_CAS:
        for (i = 0; i < count; ++i) {
            __sync_bool_compare_and_swap(&g_t.data, 1, 0);
        }
        break;

    case AT_BOOL_DCAS:
        for (i = 0; i < count; ++i) {
            uint64_t ret0 = i;
            uint64_t ret1 = i;
            uint64_t cmp0 = 0;
            uint64_t cmp1 = i;
            char result;
            __asm__ __volatile__
                    (
                        "lock cmpxchg16b   %1 \n\t"
                        "setz %0"
                        : "=q" ( result )
                        , "+m" ( g_t.data )
                        , "+d" ( cmp0 )
                        , "+a" ( cmp1 )
                        : "c" ( ret0 )
                        , "b" ( ret1 )
                        : "cc"
                        );
        }
        break;
    case AT_XCHG:
        for (i = 0; i < count; ++i) {
            uint64_t ret = i;
            __asm__ __volatile__("xchgq %1, %0"
                                  : "=r"(ret)
                                  : "m"(g_t.data), "0"(ret)
                                  : "memory");
        }
    break;
    }

    res = clock_gettime(CLOCK_MONOTONIC, &ts2);
    (void)res;

    int64_t ns = (ts2.tv_sec -ts1.tv_sec) * 1000000000 +  (ts2.tv_nsec -ts1.tv_nsec);
    return ns;
}




int main(int argc, char** argv)
{
    int k;
    int opt;
    int res;

    while ((opt = getopt(argc, argv, "f:t:n:a:b")) != -1) {
        switch (opt) {
        case 'f':
            threads = atoi(optarg);
            break;
        case 't':
            test = atoi(optarg);
            break;
        case 'n':
            count = atoi(optarg);
            break;
        case 'b':
            cpu_bench = 1;
            break;
        case 'a':
        {
            char *p;
            char *sub;
            p = strtok_r(optarg, ",", &sub);
            if (p == NULL)
                break;

            g_cpu_afinity_list[g_cpu_cnt++] = atoi(p);
            for (;;) {
                p = strtok_r(NULL, ",", &sub);
                if (p == NULL)
                    break;

                g_cpu_afinity_list[g_cpu_cnt++] = atoi(p);
            }
            break;
        }
        default: /* '?' */
            fprintf(stderr, "Usage: %s [-c calls] [-m msg_size]\n",
                    argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (g_cpu_cnt > 0) {
        if (g_cpu_cnt != threads) {
            printf("CPU AFINITY incorrect count: %d, Threads: %d\n",
                   g_cpu_cnt, threads);
        }
        if (verbose_output) {
            int k;
            printf("CPU AFINITY: ");
            for ( k = 0; k < g_cpu_cnt; k++ ) {
                printf(" %d", g_cpu_afinity_list[k]);
            }
            printf("\n");
        }
    }

    for (k = 0; k < threads; k++) {
        res = pthread_create(&g_thr[k], NULL, thread, NULL);
        if (res) {
            exit(3);
        }
    }

    void* ret;
    for (k = 0; k < threads; k++) {
        pthread_join(g_thr[k], &ret);
        if (!cpu_bench || verbose_output) {
            int64_t ns = (int64_t)ret;
            printf("THREAD[%d]: AVG : %.3f M/s\n", k, (((double)count * 1000)) / ns);
        }
    }

    return 0;
}


