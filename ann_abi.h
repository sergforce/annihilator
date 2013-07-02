#ifndef _ANN_ABI_H_
#define _ANN_ABI_H_
#include <stdint.h>
#include <stddef.h>
#ifdef __linux
#include <semaphore.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/** @file ann_abi.h Basic Language-Independent memory representation
 *
 */


#define ANN_MAX_STAGE_NAME    20
#define ANN_MAX_NAME    16
#define ANN_MAX_STAGES  16

#define ANN_BLOCK_ALIGN     64


#define STAGE_SIZE      ANN_BLOCK_ALIGN * 2

typedef char ann_stage_finalizer_t;

enum ann_stage_lock_types {
    ANN_STL_SPIN,     /**< Spin locker */
    ANN_STL_FUTEX,    /**< Linux futex locker */
    ANN_STL_KEYED,    /**< Windows NtKey* api locker */
    ANN_STL_POSIX_SEM /**< Posix semaphore locker */
};

enum ann_stage_types {
    ANN_STC_SIN_SOUT,
    ANN_STC_SIN_MOUT,
    ANN_STC_MIN_SOUT,
    ANN_STC_MIN_MOUT
};

struct ann_stage_def {
    uint8_t in_routes;
    uint8_t self_routes;
    uint8_t stage_lock_type;   /**< see @ref ann_stage_lock_types */
    uint8_t stage_concurrency; /**< see @ref ann_stage_types */
};

struct ann_stage_info {
    struct ann_stage_def def;
    uint32_t stage_off;
    uint32_t stage_size;
    char     stage_name[ANN_MAX_STAGE_NAME];
};

struct ann_header {
    char      name[ANN_MAX_NAME]; /**< Name of this Annihilator */
    uint64_t  total_mem_size;     /**< Overall memory size in SHM */
    uint32_t  total_cells_count;  /**< Total cells count, must be power of 2 */
    uint32_t  total_stages_count;
    uint64_t  total_buffer_size;  /**< Overall size for data buffer */

    struct ann_stage_info stages_inf[ANN_MAX_STAGES];


};

struct ann_stage_counters16 {
    int16_t ready_no;
    char _dummy[64 - 2];
    int16_t progress_no;
};

struct ann_stage_counters32 {
    int32_t ready_no;
    char _dummy[64 - 4];
    int32_t progress_no;
};

struct ann_stage_counters64 {
    int64_t ready_no;
    char _dummy[64 - 8];
    int64_t progress_no;
};

#ifdef __linux

struct ann_stage_counters_sem32 {
#ifdef SEM_DEBUG
    int32_t ready_no;
    char _dummy[64 - 4];
#endif
    int32_t progress_no;
    char _dummy2[64 - 4];
    sem_t sem;
};

struct ann_stage_counters_sem64 {
#ifdef SEM_DEBUG
    int64_t ready_no;
    char _dummy[64 - 8];
#endif
    int64_t progress_no;
    char _dummy2[64 - 8];
    sem_t sem;
};


struct ann_stage_counters_sem_m32 {
    int32_t ready_no;
    char _dummy[64 - 4];
    int32_t progress_no;
    char _dummy2[64 - 4];
    sem_t sem;
    int32_t cnt_fre;
    char _dummy3[64 - 4];
};

struct ann_stage_counters_sem_m64 {
    int64_t ready_no;
    char _dummy[64 - 8];
    int64_t progress_no;
    char _dummy2[64 - 8];
    sem_t sem;
    int32_t cnt_fre;
    char _dummy3[64 - 4];
};

#endif


#ifdef __cplusplus
}
#endif
#endif // _ANN_ABI_H_
