#include <ann_abi.h>

#ifndef _ANN_SHM_H_
#define _ANN_SHM_H_

#ifdef __cplusplus
extern "C" {
#endif



struct annihilator
{
    struct ann_header *pshm;
    char* data_buffer;
    uint32_t msg_size;
    uint32_t stages_num;
    uint32_t cells;

    union {
        uint16_t mask16;
        uint32_t mask32;
        uint64_t mask64;
    };

    void* stages[ANN_MAX_STAGES];
};



struct annihilator* ann_create(uint32_t cells, uint8_t stages, uint32_t msg_size, const struct ann_stage_def* stinfo);
void     ann_destroy(struct annihilator* ann);

size_t ann_shm_calc_size(uint32_t cells, uint8_t stages, uint32_t msg_size, const struct ann_stage_def *stinfo);
int    ann_shm_create(void* mem, uint32_t cells, uint8_t stages, uint32_t msg_size, const struct ann_stage_def *stinfo, int shmuse, struct annihilator* a);
int    ann_shm_open(void* shm, size_t available_sz, struct annihilator* a);

uint32_t ann_wait32(struct annihilator* ann, uint8_t stage);
uint32_t ann_wait_sem32(struct annihilator* ann, uint8_t stage);
uint32_t ann_wait_sem_m32(struct annihilator* ann, uint8_t stage);
uint32_t ann_wait_m32(struct annihilator* ann, uint8_t stage);


void     ann_next32(struct annihilator* ann, uint8_t stage, uint32_t no);
void     ann_next_sem32(struct annihilator* ann, uint8_t stage, uint32_t no);
void     ann_next_sem_m32(struct annihilator* ann, uint8_t stage, uint32_t no);
void     ann_next_m32(struct annihilator* ann, uint8_t stage, uint32_t no);

void     ann_next_m32_simo(struct annihilator* ann, uint8_t stage, uint32_t no);
void     ann_next_m32_miso(struct annihilator* ann, uint8_t stage, uint32_t no);

void*    ann_get32(struct annihilator* ann, uint32_t no);


uint64_t ann_wait64(struct annihilator* ann, uint8_t stage);
void     ann_next64(struct annihilator* ann, uint8_t stage, uint64_t no);

void*    ann_get64(struct annihilator* ann, uint64_t no);


uint16_t ann_wait16(struct annihilator* ann, uint8_t stage);
void     ann_next16(struct annihilator* ann, uint8_t stage, uint16_t no);

void*    ann_get16(struct annihilator* ann, uint16_t no);




#ifdef __cplusplus
}
#endif
#endif // _ANN_SHM_H_
