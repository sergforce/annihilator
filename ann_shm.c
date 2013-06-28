#include "ann_shm.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <malloc.h>

#define PAUSE() \
     asm volatile ("pause\r\n" : : : "memory");

#define NOP() \
     asm volatile ("nop\r\n" : : : "memory");

struct annihilator* ann_create(uint32_t cells, uint8_t stages, uint32_t msg_size)
{
    size_t buff_off;
    size_t stages_off;
    size_t end_off;
    struct annihilator* a;
    struct ann_header * h;
    int i;

    buff_off = sizeof(struct ann_header);
    if (buff_off % ANN_BLOCK_ALIGN) {
        buff_off += ANN_BLOCK_ALIGN - (buff_off % ANN_BLOCK_ALIGN);
    }

    stages_off = buff_off + cells * msg_size;
    if (stages_off % ANN_BLOCK_ALIGN) {
        stages_off += ANN_BLOCK_ALIGN - (stages_off % ANN_BLOCK_ALIGN);
    }

    end_off = stages_off + stages * STAGE_SIZE;
    a = (struct annihilator*)memalign(ANN_BLOCK_ALIGN, end_off + ANN_BLOCK_ALIGN);
    if (a == NULL) {
        return NULL;
    }

    h = (struct ann_header *)((char*)a + ANN_BLOCK_ALIGN);

    h->total_mem_size = end_off;
    h->total_cells_count = cells;
    h->total_stages_count = stages;
    h->total_buffer_size = stages_off - buff_off;

    a->pshm = h;
    a->data_buffer = (char*)h + buff_off;

    a->mask64 = cells - 1;
    a->msg_size = msg_size;
    a->stages_num = stages;
    a->cells = cells;

    for (i = 0; i < stages; i++) {
        a->stages[i] = (char*)h + stages_off + i * STAGE_SIZE;
    }

    memset(a->stages[0], 0, stages * ANN_BLOCK_ALIGN);
    (( struct ann_stage_counters32*)a->stages[0])->ready_no = cells;

    return a;
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

        __sync_synchronize();

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
