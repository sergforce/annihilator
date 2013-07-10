#!/bin/sh

CC="gcc"
DEBUG="-g -DNDEBUG"
OPT="-flto -O3 -march=native "

$CC $OPT $DEBUG -Wall ann_shm.c ann_shm_test.c -DUSE64 -I. -o test_gcc_64 -lrt -lpthread    || exit 1
$CC $OPT $DEBUG -Wall ann_shm.c ann_shm_test.c -DUSE32 -I. -o test_gcc_32 -lrt -lpthread
$CC $OPT $DEBUG -Wall ann_shm.c ann_shm_test.c -DUSE16 -I. -o test_gcc_16 -lrt -lpthread

$CC $OPT $DEBUG -Wall ann_shm.c ann_shm_test.c -DUSE_SEM32 -I. -o test_gcc_sem_32 -lrt  -lpthread
$CC $OPT $DEBUG -Wall ann_shm.c ann_shm_test.c -DUSE_SEM_M32 -I. -o test_gcc_sem_m_32 -lrt  -lpthread
$CC $OPT $DEBUG -Wall ann_shm.c ann_shm_test.c -DUSE_M32 -I. -o test_gcc_m_32 -lrt  -lpthread

$CC $OPT $DEBUG -Wall ann_shm.c ann_shm_test.c -DUSE_M32 -DFAST_M -I. -o test_gcc_fast_m_32 -lrt  -lpthread


$CC $OPT $DEBUG -Wall ann_shm.c ann_shm_test.c -DUSE_M32_SC -I. -o test_gcc_m_32_scmp -lrt  -lpthread

$CC $OPT $DEBUG -Wall ann_shm.c ann_shm_test.c -DUSE_M32_SP -I. -o test_gcc_m_32_mcsp -lrt  -lpthread

