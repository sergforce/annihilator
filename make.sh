#!/bin/sh

gcc -flto -O3 -g -Wall ann_shm.c ann_shm_test.c -DNDEBUG -DUSE64 -I. -o test_gcc_64 -lrt
gcc -flto  -O3 -g -Wall ann_shm.c ann_shm_test.c -DNDEBUG -DUSE32 -I. -o test_gcc_32 -lrt
gcc -flto  -O3 -g -Wall ann_shm.c ann_shm_test.c -DNDEBUG -DUSE16 -I. -o test_gcc_16 -lrt

gcc -flto  -O3 -g -Wall ann_shm.c ann_shm_test.c -DNDEBUG -DUSE_SEM32 -I. -o test_gcc_sem_32 -lrt
gcc -flto  -O3 -g -Wall ann_shm.c ann_shm_test.c -DNDEBUG -DUSE_SEM_M32 -I. -o test_gcc_sem_m_32 -lrt
gcc -flto  -O3 -g -Wall ann_shm.c ann_shm_test.c -DNDEBUG -DUSE_M32 -I. -o test_gcc_m_32 -lrt

