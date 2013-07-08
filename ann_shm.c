#include "ann_shm.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>
#include <errno.h>


//#define FAST_M

#define CELLS_RESERVED 8

#define PAUSE() \
     asm volatile ("pause\r\n" : : : "memory");

#define NOP() \
     asm volatile ("nop\r\n" : : : "memory");

#define ALIGN_TO(x, val)                   if ((x) % (val)) (x) += ((val) - ((x) % (val)))

static uint32_t ann_calculate_stage_size(uint32_t cells, const struct ann_stage_def *stinfo)
{
    uint32_t basic;
    uint32_t finsz;

    switch (stinfo->stage_lock_type) {
    case ANN_STL_SPIN: {
        switch (stinfo->stage_concurrency) {
        case ANN_STC_MIN_MOUT:
            basic = sizeof(struct ann_stage_counters_m64);
            break;
        default:
            basic = sizeof(struct ann_stage_counters64);
            break;
        }
        break;
    }
    case ANN_STL_POSIX_SEM: {
        switch (stinfo->stage_concurrency) {
        case ANN_STC_MIN_MOUT:
            basic = sizeof(struct ann_stage_counters_sem_m64);
            break;

        default:
            basic = sizeof(struct ann_stage_counters_sem64);
            break;
        }
        break;
    }
    default:                basic = 0;
    }

    switch (stinfo->stage_concurrency) {
    case ANN_STC_MIN_MOUT:  finsz = cells * sizeof(ann_stage_finalizer_t); break;
    default:                finsz = 0;  break;
    }

    ALIGN_TO(basic, ANN_BLOCK_ALIGN);
    ALIGN_TO(finsz, ANN_BLOCK_ALIGN);

    return basic + finsz;
}

static void ann_init_stage(void* mem, const struct ann_stage_info *st, uint32_t readys, int shmuse)
{
    const struct ann_stage_def *stinfo = &st->def;
    memset(mem, 0, st->stage_size);

    switch (stinfo->stage_lock_type) {
    case ANN_STL_SPIN: {
        switch (stinfo->stage_concurrency) {
        case ANN_STC_MIN_MOUT: {
            struct ann_stage_counters_m64* pcnt = (struct ann_stage_counters_m64*)mem;
            pcnt->ready_no = readys;
            pcnt->available_count = readys;
            pcnt->progress_no = 0;
            pcnt->cnt_fre = 0;

#ifdef FAST_M
            ((struct ann_stage_counters_m32*)pcnt)->emb_avail = readys;
#endif
            break;
        }
        default: {
            struct ann_stage_counters64* pcnt = (struct ann_stage_counters64*)mem;
            pcnt->ready_no = readys;
            pcnt->progress_no = 0;
            break;
        }
        }
        break;
    }
    case ANN_STL_POSIX_SEM: {
        switch (stinfo->stage_concurrency) {
        case ANN_STC_MIN_MOUT: {
            struct ann_stage_counters_sem_m64* pcnt = (struct ann_stage_counters_sem_m64*)mem;
            pcnt->ready_no = readys;
            pcnt->progress_no = 0;
            sem_init(&pcnt->sem, shmuse, readys);
            pcnt->cnt_fre = 0;
            break;
        }
        default: {
            struct ann_stage_counters_sem64* pcnt = (struct ann_stage_counters_sem64*)mem;
#ifdef SEM_DEBUG
            pcnt->ready_no = readys;
#endif
            pcnt->progress_no = 0;
            sem_init(&pcnt->sem, shmuse, readys);
            break;
        }
        }
        break;
    }
    }
}

size_t ann_shm_calc_size(uint32_t cells, uint8_t stages, uint32_t msg_size, const struct ann_stage_def *stinfo)
{
    size_t buff_off;
    size_t stages_off;
    size_t end_off;
    int i;

    buff_off = sizeof(struct ann_header);
    ALIGN_TO(buff_off, ANN_BLOCK_ALIGN);

    stages_off = buff_off + cells * msg_size;
    ALIGN_TO(stages_off, ANN_BLOCK_ALIGN);

    end_off = stages_off; // + stages * STAGE_SIZE;
    for (i = 0; i < stages; i++) {
        end_off += ann_calculate_stage_size(cells, &stinfo[i]);
    }
    return   end_off;
}

int ann_shm_create(void* mem, uint32_t cells, uint8_t stages, uint32_t msg_size, const struct ann_stage_def *stinfo, int shmuse, struct annihilator* a)
{
    size_t buff_off;
    size_t stages_off;
    size_t end_off;
    struct ann_header * h;
    int i;
    uint32_t stoffs[ANN_MAX_STAGES];
    uint32_t stsz[ANN_MAX_STAGES];

    buff_off = sizeof(struct ann_header);
    ALIGN_TO(buff_off, ANN_BLOCK_ALIGN);

    stages_off = buff_off + cells * msg_size;
    ALIGN_TO(stages_off, ANN_BLOCK_ALIGN);

    end_off = stages_off; // + stages * STAGE_SIZE;
    for (i = 0; i < stages; i++) {
        stsz[i] = ann_calculate_stage_size(cells, &stinfo[i]);
        stoffs[i] = end_off;

        end_off += stsz[i];
    }

    h = (struct ann_header *)(mem);

    h->total_mem_size = end_off;
    h->total_cells_count = cells;
    h->total_stages_count = stages;
    h->total_buffer_size = stages_off - buff_off;
    h->fixed_msg_size = msg_size;
    h->attach_count = 0;

    a->pshm = h;
    a->data_buffer = (char*)h + buff_off;

    a->mask64 = cells - 1;
    a->msg_size = msg_size;
    a->stages_num = stages;
    a->cells = cells;

    for (i = 0; i < stages; i++) {
        h->stages_inf[i].stage_off  = stoffs[i];
        h->stages_inf[i].stage_size = stsz[i];
        h->stages_inf[i].def = stinfo[i];

        a->stages[i] = (char*)h + stoffs[i];//stages_off + i * STAGE_SIZE;

        ann_init_stage(a->stages[i], &h->stages_inf[i], (i == 0) ? cells  - CELLS_RESERVED : 0, shmuse);
    }

    return AE_OK;
}

struct annihilator* ann_create(uint32_t cells, uint8_t stages, uint32_t msg_size, const struct ann_stage_def *stinfo)
{
    size_t sz = ann_shm_calc_size(cells, stages, msg_size, stinfo);
    struct annihilator* a = (struct annihilator*)memalign(ANN_BLOCK_ALIGN, sz + ANN_BLOCK_ALIGN);
    if (a == NULL) {
        return NULL;
    }

    ann_shm_create((char*)a + ANN_BLOCK_ALIGN, cells, stages, msg_size, stinfo, 0, a);
    return a;
}

int ann_shm_open(void* shm, size_t available_sz, struct annihilator* a)
{
    size_t buff_off;
    //size_t stages_off;
    //size_t end_off;
    struct ann_header * h;
    int i;

    h = (struct ann_header *)(shm);
    if (h->total_mem_size < available_sz) {
        return AE_INVALID_SIZE;
    }
    memset(a, 0, sizeof(*a));

    uint8_t stages     = h->total_stages_count;
    uint32_t cells     = h->total_cells_count;

    buff_off = sizeof(struct ann_header);
    ALIGN_TO(buff_off, ANN_BLOCK_ALIGN);

    //stages_off = buff_off + h->total_buffer_size;
    //end_off = stages_off;

    a->pshm = h;
    a->data_buffer = (char*)h + buff_off;

    a->mask64 = cells - 1;
    a->msg_size = h->fixed_msg_size;
    a->stages_num = stages;
    a->cells = cells;

    for (i = 0; i < stages; i++) {
        a->stages[i] =  (char*)h +  h->stages_inf[i].stage_off;
    }

    __sync_fetch_and_add(&h->attach_count, 1);
    return AE_OK;
}

void     ann_destroy(struct annihilator* ann)
{
    free(ann);
}

uint32_t ann_wait32(struct annihilator* ann, uint8_t stage)
{
    struct ann_stage_counters32* c = ( struct ann_stage_counters32*)ann->stages[stage];

    while (c->ready_no == c->progress_no) {
        PAUSE();

        //__sync_synchronize();

    }
    return c->progress_no++;
}

void     ann_next32(struct annihilator* ann, uint8_t stage, uint32_t no)
{
    if (stage + 1 == ann->stages_num) {
        struct ann_stage_counters32* c = ( struct ann_stage_counters32*)ann->stages[0];
        //Release
        //check endNo
        uint32_t old = c->ready_no++;
        assert (old - no == ann->cells);
        (void)old;
    } else {
        struct ann_stage_counters32* c = ( struct ann_stage_counters32*)ann->stages[stage + 1];
        ++c->ready_no;
    }
}


uint32_t ann_wait_sem32(struct annihilator* ann, uint8_t stage)
{
    struct ann_stage_counters_sem32* c = ( struct ann_stage_counters_sem32*)ann->stages[stage];

    int res;
    for (;;) {
        res = sem_trywait(&c->sem);
        if (res == -1) {
            if (errno == EAGAIN) {
                PAUSE();
                continue;
            }
        } else {
            break;
        }
    }
    return c->progress_no++;
}

void     ann_next_sem32(struct annihilator* ann, uint8_t stage, uint32_t no)
{
    if (stage + 1 == ann->stages_num) {
        struct ann_stage_counters_sem32* c = ( struct ann_stage_counters_sem32*)ann->stages[0];
        //Release
        //check endNo
#ifdef SEM_DEBUG
        uint32_t old = c->ready_no++;
        assert (old - no == ann->cells);
        (void)old;
#endif
        sem_post(&c->sem);

    } else {
        struct ann_stage_counters_sem32* c = ( struct ann_stage_counters_sem32*)ann->stages[stage + 1];
        sem_post(&c->sem);
#ifdef SEM_DEBUG
        ++c->ready_no;
#endif
    }
}


uint32_t ann_wait_sem_m32(struct annihilator* ann, uint8_t stage)
{
    struct ann_stage_counters_sem_m32* c = ( struct ann_stage_counters_sem_m32*)ann->stages[stage];

    int res;
    for (;;) {
        res = sem_trywait(&c->sem);
        if (res == -1) {
            if (errno == EAGAIN) {
                PAUSE();
                continue;
            } else {
                abort();
            }
        } else {
            break;
        }
    }

    uint32_t r = __sync_fetch_and_add(&c->progress_no, 1);
    return r;
}

void     ann_next_sem_m32(struct annihilator* ann, uint8_t stage, uint32_t no)
{
    struct ann_stage_counters_sem_m32* c = (stage + 1 == ann->stages_num) ?
        ( struct ann_stage_counters_sem_m32*)ann->stages[0] :
        ( struct ann_stage_counters_sem_m32*)ann->stages[stage + 1];

    struct ann_stage_counters_sem_m32* z = ( struct ann_stage_counters_sem_m32*)ann->stages[stage];

    ann_stage_finalizer_t *f = (ann_stage_finalizer_t*)((char*)(c) + sizeof(struct ann_stage_counters_sem_m32));
    uint32_t wakecnt = 0;

    GET_STAGE_FIN(f[(no & ann->mask32)]) = 1;

    int lcnt = __sync_add_and_fetch(&c->cnt_fre, 1);
    if (lcnt > 1)
        return;

    uint32_t sno;
    if (stage + 1 == ann->stages_num)
        sno = c->ready_no - (ann->cells - CELLS_RESERVED);
    else
        sno = c->ready_no;

    for (;;) {
        uint32_t wcnt = 0;


        for (; sno < z->progress_no; sno++) {
            if (GET_STAGE_FIN(f[(sno & ann->mask32)]) == 0)
                break;

            GET_STAGE_FIN(f[(sno & ann->mask32)]) = 0;
            wcnt++;
        }

        if (wcnt) {
            wakecnt += wcnt;
            c->ready_no += wcnt;
        }

        if (__sync_bool_compare_and_swap(&c->cnt_fre, lcnt, 0)) {
            break;
        }

        PAUSE();

        lcnt = c->cnt_fre;
    }

    int w = wakecnt;
    for (;w != 0; w--) {
        sem_post(&c->sem);
    }

}


uint32_t ann_wait_m32(struct annihilator* ann, uint8_t stage)
{
    struct ann_stage_counters_m32* c = ( struct ann_stage_counters_m32*)ann->stages[stage];
#ifdef FAST_M
    for (;;) {
        union {
            struct {
                int32_t progress_no;
                int32_t emb_avail;
            };
            int64_t dcsw;
        } local;

        local.dcsw = c->dcsw;
        if (local.emb_avail != 0) {

            if (__sync_bool_compare_and_swap(&c->dcsw, local.dcsw, local.dcsw + 1 - 0x100000000)) {
                return local.progress_no;
            }

        }

        PAUSE();
    }
#else
    for (;;) {
        uint32_t cnt;
        while ((cnt = c->available_count) == 0) {
            PAUSE();
        }
        if (__sync_bool_compare_and_swap(&c->available_count, cnt, cnt - 1)) {
            return __sync_fetch_and_add(&c->progress_no, 1);
        }

        PAUSE();
    }
#endif
}

void     ann_next_m32(struct annihilator* ann, uint8_t stage, uint32_t no)
{
    struct ann_stage_counters_m32* c = (stage + 1 == ann->stages_num) ?
        ( struct ann_stage_counters_m32*)ann->stages[0] :
        ( struct ann_stage_counters_m32*)ann->stages[stage + 1];

    struct ann_stage_counters_m32* z = ( struct ann_stage_counters_m32*)ann->stages[stage];

    ann_stage_finalizer_t *f = (ann_stage_finalizer_t*)((char*)(c) + sizeof(struct ann_stage_counters_m32));

    GET_STAGE_FIN(f[(no & ann->mask32)]) = 1;
    int lcnt = __sync_fetch_and_add(&c->cnt_fre, 1);
    if (lcnt > 0)
        return;

    ++lcnt;

    uint32_t tcnt = 0;
    uint32_t lpos = 0;

    for (;;) {
        uint32_t sno;
        if (stage + 1 == ann->stages_num)
            sno = c->ready_no - (ann->cells - CELLS_RESERVED);
        else
            sno = c->ready_no;

        uint32_t wcnt = 0;

        for (; sno < z->progress_no; sno++) {
            if (GET_STAGE_FIN(f[(sno & ann->mask32)]) == 0)
                break;

            wcnt++;
        }

        if (wcnt) {
            lpos = (c->ready_no += wcnt);
            tcnt += wcnt;
        }

#ifdef FAST_M
        c->emb_avail += wcnt;
#else
        c->available_count += wcnt;
#endif

        if (__sync_bool_compare_and_swap(&c->cnt_fre, lcnt, 0)) {
            uint32_t j;
            for (j = tcnt; j > 0 ; --j) {
                GET_STAGE_FIN(f[((lpos - j) & ann->mask32)]) = 0;
            }

            break;
        }

        //__sync_synchronize();

        PAUSE();

        lcnt = c->cnt_fre;
    }
}


void*    ann_get32(struct annihilator* ann, uint32_t no)
{
    return ann->data_buffer + (no & ann->mask32) * ann->msg_size;
}



////////////////////////////////////////////////////////////



uint64_t ann_wait64(struct annihilator* ann, uint8_t stage)
{
    struct ann_stage_counters64* c = ( struct ann_stage_counters64*)ann->stages[stage];


    while (c->ready_no == c->progress_no) {
        PAUSE();

        __sync_synchronize();

    }
    return c->progress_no++;
}

void     ann_next64(struct annihilator* ann, uint8_t stage, uint64_t no)
{
    if (stage + 1 == ann->stages_num) {
        struct ann_stage_counters64* c = ( struct ann_stage_counters64*)ann->stages[0];
        //Release
        //check endNo
        uint64_t old = c->ready_no++;
        assert (old - no == ann->cells);
        (void)old;
    } else {
        struct ann_stage_counters64* c = ( struct ann_stage_counters64*)ann->stages[stage + 1];
        ++c->ready_no;
    }
}

void*    ann_get64(struct annihilator* ann, uint64_t no)
{
    return ann->data_buffer + (no & ann->mask64) * ann->msg_size;
}

////////////////////////////////////////////////////////////////////////



uint16_t ann_wait16(struct annihilator* ann, uint8_t stage)
{
    struct ann_stage_counters16* c = ( struct ann_stage_counters16*)ann->stages[stage];


    while (c->ready_no == c->progress_no) {
        PAUSE();

        __sync_synchronize();

    }
    return c->progress_no++;
}

void     ann_next16(struct annihilator* ann, uint8_t stage, uint16_t no)
{
    if (stage + 1 == ann->stages_num) {
        struct ann_stage_counters16* c = ( struct ann_stage_counters16*)ann->stages[0];
        //Release
        //check endNo
        uint16_t old = c->ready_no++;
        assert (old - no == ann->cells);
        (void)old;
    } else {
        struct ann_stage_counters16* c = ( struct ann_stage_counters16*)ann->stages[stage + 1];
        ++c->ready_no;
    }
}

void*    ann_get16(struct annihilator* ann, uint16_t no)
{
    return ann->data_buffer + (no & ann->mask16) * ann->msg_size;
}







