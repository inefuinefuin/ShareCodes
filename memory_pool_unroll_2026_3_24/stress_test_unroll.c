#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>

#define TEST_
#include "unroll_link_lists_bitmap.c"

#define ITERATIONS 20000

static int count_nodes(Block* b){
    int total = 0;
    while(b){
        total += __builtin_popcount(b->bits);
        b = b->next;
    }
    return total;
}

static void verify_invariants(Block* pool){
    // 1) the popcount sum equals actual traversed nodes
    int pop_sum = 0, trav = 0;
    Block* b = pool;
    while(b){
        pop_sum += __builtin_popcount(b->bits);
        Node* cur = b->headptr;
        while(cur){
            trav++;
            // node's id bit must be set in block bits
            assert((b->bits & cur->id) == cur->id);
            cur = cur->next;
        }
        b = b->next;
    }
    assert(pop_sum == trav);
}

int main(void){
    srand((unsigned)time(NULL));
    // Block* pool = __alloc_nblock(10);
    Block* pool = __alloc_block();
    if(!pool){
        fprintf(stderr,"alloc failed\n");
        return 2;
    }

    /* start timing here so the program reports its own elapsed time */
    clock_t __t0 = clock();

    for(int it=0; it<ITERATIONS; ++it){
        int r = rand() % 100;
        if(r < 60){
            // insert
            InsertElem(pool, rand());
        }else if(r < 80){
            // delete by value (random small value)
            DeleteElems(pool, rand() % 50);
        }else if(r < 95){
            // delete index in a random block if possible
            Block* b = pool;
            int bi = rand() % 4; // try first few blocks
            while(bi-- && b->next) b = b->next;
            int cnt = __builtin_popcount(b->bits);
            if(cnt>0){
                size_t idx = rand() % cnt;
                DeleteIndex(b, idx);
            }
        }else{
            // memory reorg occasionally
            mem_scan_rearg(&pool, 0.75);
        }

        if((it & 0x3FF) == 0){
            // check invariants every 1024 iterations
            verify_invariants(pool);
        }
    }

    // final checks
    verify_invariants(pool);
    double __ms = (double)(clock() - __t0) / CLOCKS_PER_SEC * 1000.0;
    printf("Stress test completed: total nodes = %d\n", count_nodes(pool));
    printf("Elapsed ms: %.3f\n", __ms);

    FreeBlocks(pool);
    return 0;
}
