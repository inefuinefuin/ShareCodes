#include <stdio.h>

#include <stdlib.h>
#include <stdint.h>

#define SLABS 8

typedef struct Node{
    uint8_t id;
    int data;
    struct Node* next;
}Node;

typedef struct Block{
    struct Block* next;
    Node slabs[SLABS];
    Node* headptr;
    uint8_t bits;
}Block;

/* allocate a block */
Block* __alloc_block(){
    Block* p = (Block*)calloc(1,sizeof(Block));
    if(!p) return NULL;
    int i=0;
    while(i<SLABS) { p->slabs[i].id = 1<<i; i++; }
    return p;
}

/* allocate n blocks */
Block* __alloc_nblock(size_t n){
    if(!n) return NULL;
    Block dummy;
    Block** p = &dummy.next;
    while(n--){
        *p = __alloc_block();
        if(!*p) break;
        p = &(*p)->next;
    }
    return dummy.next;
}

// /* allocate n blocks */
// /* error: in mem_scan_rearg() : __gc_node() error-operator */
// Block* __alloc_nblock(size_t n){
//     Block* p = (Block*)calloc(n,sizeof(Block));
//     if(!p) return NULL;
//     Block* curr = p;
//     for(int _=0; _<n; _++){
//         int i=0;
//         while(i<SLABS) { (p+_)->slabs[i].id = 1<<i; i++; }
//         if(_!=n-1){
//             curr->next = curr+1;
//             curr = curr->next;
//         }
//     }
//     return p;
// }


/* garbage single block */
void __gc_block(Block* b){ free(b); }

// int __built_frz_u8(uint8_t n){
//     uint8_t tbit = 0x01; 
//     if(n==0xFF) return -1;
//     int idx = 0;
//     while(n&tbit){ n = n>>1; idx++; }
//     return idx;
// }
// Node* __alloc_node(Block* b){
//     int idx = __built_frz_u8(b->bits);
//     if(idx==-1) return NULL;
//     b->bits |= b->slabs[idx].id;
//     return &b->slabs[idx];
// }

/* allocate node but not initialize value */
Node* __alloc_node(Block* b){
    int n = ~b->bits;
    if(!n) return NULL;
    int idx = __builtin_ffs(n)-1;
    b->bits |= b->slabs[idx].id;
    return &b->slabs[idx];
}

/* garbage collect(node), delete value */
void __gc_node(Block* b,Node* n){
    uint8_t s = b->bits & n->id;
    if(s!=n->id) return;
    b->bits &= ~s;
    n->data = 0;
    n->next = NULL;
}

/* reduce pieces and merge block data based on a percentage */
void mem_scan_rearg(Block** b,double pct){
    if(!b || !*b) return;
    if(pct>1 && pct<0) return;
    Block dummy; 
    dummy.next = *b;
    Block* p_f = &dummy;
    Block* p = *b;
    while(p){
        int cnt = __builtin_popcount(p->bits);
        double rlpct = (double)cnt/SLABS;
        
        Block* p_b = p->next;
        if(pct<=rlpct){
            p_f = p;
            p = p_b;
            continue;
        }
        if(!p_b) break;
        
        p_f->next = p_b;
        p->next = p_b->next;
        p_b->next = p;

        Block* tmp = p;
        p = p_b;
        p_b = tmp;

        int remain = SLABS - __builtin_popcount(p->bits);
        if(!remain) {
            p_f = p;
            p = p_b;
            continue;
        }

        int times = cnt>remain ? remain : cnt;
        int i = 0;
        while(i<times){
            Node* n = __alloc_node(p);
            Node* swn = p_b->headptr;

            n->data = swn->data;
            n->next = p->headptr;
            p->headptr = n;

            p_b->headptr = swn->next;
            __gc_node(p_b,swn);
            i++;
        }
        if(cnt>remain){
            p_f = p;
            p = p_b;
        }else{
            p->next = p_b->next;
            __gc_block(p_b);
        }
    }
    *b = dummy.next;
}

#define mem_rearg(p) mem_scan_rearg(p,0.75)

/* insert val into element and auto grow block */
int InsertElem(Block* b,int val){
    if(!b) return -1;
    Block** curr = &b;
    Block* entry = NULL;
    while((*curr)->bits==0xFF){
        entry = *curr;
        curr = &(*curr)->next;
        if(!*curr) *curr = __alloc_block();
    }
    Node* p = __alloc_node(*curr);
    p->data = val;
    p->next = (*curr)->headptr;
    (*curr)->headptr = p;
    return 0;
}

/* delete elements with same val in all blocks */
int DeleteElems(Block* b,int val){
    if(!b) return -1;
    Block* curr_b = b;
    while(curr_b){
        Node* curr = curr_b->headptr;
        Node* entry = NULL;
        int i=0; 
        int cnt = __builtin_popcount(curr_b->bits);
        while(i<cnt){
            if(curr->data!=val){
                entry = curr;
                curr = curr->next;
            }else{
                if(!entry) curr_b->headptr = curr->next;
                else entry->next = curr->next;
                __gc_node(curr_b,curr);
                curr = entry ? entry->next : curr_b->headptr;
            }
            i++;
        }
        curr_b = curr_b->next;
    }
    return 0;
}

/* delete elements with same val in single block */
int DeleteElem(Block* b,int val){
    if(!b) return -1;
    Node* curr = b->headptr;
    Node* entry = NULL;
    int i = 0; 
    int cnt = __builtin_popcount(b->bits);
    while(i<cnt){
        if(curr->data!=val){
            entry = curr;
            curr = curr->next;
        }else{
            if(!entry) b->headptr = curr->next;
            else entry->next = curr->next;
            __gc_node(b,curr);
            curr = entry ? entry->next : b->headptr;
        }
        i++;
    }
    return 0;
}


/* delete element that index is idx in a block */
int DeleteIndex(Block* b,size_t idx){
    if(!b) return -1;
    int cnt = __builtin_popcount(b->bits);
    if(cnt<=idx) return -2;
    Node* curr = b->headptr;
    Node* entry = NULL;
    int i=0;
    while(i<idx){
        entry = curr;
        curr = curr->next;
        i++;
    }
    if(!entry) b->headptr = curr->next;
    else entry->next = curr->next;
    __gc_node(b,curr);
    return 0;
}

/* free all blocks */
void FreeBlocks(Block* b){
    while(b){
        Block* p = b->next;
        free(b);
        b = p;
    }
}

void Percentage(Block* b) {
    while (b) {
        int cnt = __builtin_popcount(b->bits);
        printf("%d %lf\n",cnt,(double)cnt/SLABS);
        b = b->next;
    }
}





#ifndef TEST_
int main(){
    int arr[10] = {1,2,3,4,5,6,5,4,9,10,};
    int arr2[10] = {11,12,13,4,15,6,5,44,19,10,};
    int arr3[] = {31,23,42,52,3,42,5,3,6,4};
    int arr4[] = {4,5,3,11,34,6,7,2,1,5};
    // Block* block = __alloc_block();
    Block* block = __alloc_nblock(4);
    for(int i=0; i<10; i++){
        InsertElem(block,arr[i]);
        InsertElem(block,arr2[i]);
        InsertElem(block,arr3[i]);
        InsertElem(block,arr4[i]);
    }
    DeleteIndex(block,7);
    DeleteIndex(block->next,2);
    DeleteElems(block,5);
    DeleteElems(block,3);
    DeleteElems(block,4);
    DeleteIndex(block,1);
    DeleteIndex(block,1);
    DeleteIndex(block,1);
    DeleteIndex(block,1);
    Percentage(block);
    // InsertElem(block,33);
    // mem_scan_rearg(&block,1);
    mem_rearg(&block);
    // mem_scan_rearg(&block,0.2);
    printf("======\n");
    Percentage(block);
    FreeBlocks(block);
}
#endif
