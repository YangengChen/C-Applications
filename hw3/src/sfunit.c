#include <criterion/criterion.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../include/sfmm.h"


/**
 *  HERE ARE OUR TEST CASES NOT ALL SHOULD BE GIVEN STUDENTS
 *  REMINDER MAX ALLOCATIONS MAY NOT EXCEED 4 * 4096 or 16384 or 128KB
 */

 Test(sf_memsuite, Malloc_an_Integer, .init = sf_mem_init, .fini = sf_mem_fini) {
    int *x = sf_malloc(sizeof(int));
    *x = 4;
    cr_assert(*x == 4, "Failed to properly sf_malloc space for an integer!");
}

Test(sf_memsuite, Free_block_check_header_footer_values, .init = sf_mem_init, .fini = sf_mem_fini) {
    void *pointer = sf_malloc(sizeof(short));
    sf_free(pointer);
    pointer = pointer - 8;
    sf_header *sfHeader = (sf_header *) pointer;
    cr_assert(sfHeader->alloc == 0, "Alloc bit in header is not 0!\n");
    sf_footer *sfFooter = (sf_footer *) (pointer - 8 + (sfHeader->block_size << 4));
    cr_assert(sfFooter->alloc == 0, "Alloc bit in the footer is not 0!\n");
}

Test(sf_memsuite, PaddingSize_Check_char, .init = sf_mem_init, .fini = sf_mem_fini) {
    void *pointer = sf_malloc(sizeof(char));
    pointer = pointer - 8;
    sf_header *sfHeader = (sf_header *) pointer;
    cr_assert(sfHeader->padding_size == 15, "Header padding size is incorrect for malloc of a single char!\n");
}

Test(sf_memsuite, Check_next_prev_pointers_of_free_block_at_head_of_list, .init = sf_mem_init, .fini = sf_mem_fini) {
    int *x = sf_malloc(4);
    memset(x, 0, 4);
    cr_assert(freelist_head->next == NULL);
    cr_assert(freelist_head->prev == NULL);
}

Test(sf_memsuite, Coalesce_no_coalescing, .init = sf_mem_init, .fini = sf_mem_fini) {
    void *x = sf_malloc(4);
    void *y = sf_malloc(4);
    memset(y, 0xFF, 4);
    sf_free(x);
    cr_assert(freelist_head == x - 8);
    sf_free_header *headofx = (sf_free_header*) (x - 8);
    sf_footer *footofx = (sf_footer*) (x - 8 + (headofx->header.block_size << 4)) - 8;

    sf_blockprint((sf_free_header*)((void*)x - 8));
    // All of the below should be true if there was no coalescing
    cr_assert(headofx->header.alloc == 0);
    cr_assert(headofx->header.block_size << 4 == 32);
    cr_assert(headofx->header.padding_size == 0);

    cr_assert(footofx->alloc == 0);
    cr_assert(footofx->block_size << 4 == 32);
}

/*
//############################################
// STUDENT UNIT TESTS SHOULD BE WRITTEN BELOW
// DO NOT DELETE THESE COMMENTS
//############################################
*/
//=========================================================================
//1
Test(sf_memsuite, coalescing, .init = sf_mem_init, .fini = sf_mem_fini) {
    void *x = sf_malloc(4);
    void *y = sf_malloc(4);
    void *z = sf_malloc(4);
    sf_free(x);
    sf_free(z);
    sf_free(y); // Coalescing two adjacent blocks
    cr_assert(freelist_head == x-8); 
    sf_free_header *headofx = (sf_free_header*)(x-8);
    sf_free_header *headofy = (sf_free_header*)(y-8);
    sf_free_header *headofz = (sf_free_header*)(z-8);
    cr_assert(headofz->prev == NULL); // Check the pointers
    cr_assert(headofz->next == NULL);
    cr_assert(headofx->prev == NULL);
    cr_assert(headofx->next == NULL);
    cr_assert(headofy->prev == NULL);
    cr_assert(headofy->next == NULL);

    sf_header* headerofx = (sf_header*)(x-8);
    cr_assert(headerofx->block_size<<4 == 4096);
}

//2
Test(sf_memsuite, Test_realloc, .init = sf_mem_init, .fini = sf_mem_fini) {
    sf_malloc(48);
    sf_malloc(48);
    void *x = sf_malloc(48);
    void *y = sf_malloc(48);
    void *z = sf_malloc(48);
    sf_malloc(3760); // WHOLE PAGE ALLOCATED
    cr_assert(freelist_head == NULL);
    sf_free(x);
    cr_assert(freelist_head == (sf_free_header*)(x-8));
    sf_free(z);
    cr_assert(freelist_head == (sf_free_header*)(z-8));
    cr_assert(freelist_head->next == (sf_free_header*)(x-8));
    sf_realloc(y, 100); // 128 - (112 payload_size + 16) = 0

    sf_footer* zfooter = (sf_footer*)(z-8+64-8);
    sf_header* yheader = (sf_header*)(y-8);
    cr_assert(yheader->block_size<<4 == 128);
    cr_assert(zfooter->alloc = 1);
    cr_assert(zfooter->block_size<<4 == 128);
    cr_assert(x-8 == freelist_head);
}

//3
Test(sf_memsuite, malloc_errno_check, .init = sf_mem_init, .fini = sf_mem_fini) {
    sf_malloc(32000); // more than 4 pages, set errno

    cr_assert(errno != 0);
    errno = 0;

    sf_malloc(0);
    cr_assert(errno != 0);
    errno = 0;

    sf_malloc(-2);
    cr_assert(errno != 0);

    sf_malloc(16000);
    sf_malloc(2000);

    cr_assert(errno == 12); // errno == ENOMEM
}
//4
Test(sf_memsuite, realloc_bound_check, .init = sf_mem_init, .fini = sf_mem_fini) {
    void* x = malloc(32); 

    sf_realloc(x-8, 16); // set errno, invalid ptr
    cr_assert(errno != 0);
    errno = 0;

    sf_realloc(NULL, 32); //should perform malloc
    cr_assert(errno == 0);    
    sf_realloc(NULL, 0); // set errno
    cr_assert(errno != 0);
}

//5
Test(sf_memsuite, Realloc_shrink_splinter, .init = sf_mem_init, .fini = sf_mem_fini) {
    void* a = sf_malloc(32); //48
    void* b = sf_malloc(32); //48
    sf_malloc(32);

    sf_free(b);
    sf_realloc(a, 1); //Cause splinter 48-32 = 16, coalesce with b

    sf_header* header_a = (sf_header*)(a-8);
    cr_assert(header_a->block_size<<4 == 32);

    sf_header* header_b = (sf_header*)((void*)header_a + (header_a->block_size<<4));
    cr_assert(header_b->block_size<<4 == 64);
    cr_assert(freelist_head == (sf_free_header*)header_b);
}

//6
Test(sf_memsuite, Coalescing_right, .init = sf_mem_init, .fini = sf_mem_fini) {
    void* a = sf_malloc(32);
    void* b = sf_malloc(32); 
    void* c = sf_malloc(32); 
    void* d = sf_malloc(32); 

    sf_header* header_a = (sf_header*)(a-8);
    sf_header* header_b = (sf_header*)(b-8);
    sf_header* header_c = (sf_header*)(c-8);
    sf_header* header_d = (sf_header*)(d-8);
    sf_free(d);
    cr_assert(freelist_head == (sf_free_header*)header_d);
    cr_assert(header_d->block_size<<4 == 3904+48);
    sf_free(c);
    cr_assert(freelist_head == (sf_free_header*)header_c);
    cr_assert(header_c->block_size<<4 == 3904+48+48);
    sf_free(b);
    cr_assert(freelist_head == (sf_free_header*)header_b);
    cr_assert(header_b->block_size<<4 == 3904+48+48+48);
    sf_free(a);


    cr_assert((sf_free_header*)header_a == freelist_head);
    cr_assert(header_a->block_size<<4 == 4096); // one page
    cr_assert(freelist_head->next == NULL);
}

//7
Test(sf_memsuite, Check_meminfo, .init = sf_mem_init, .fini = sf_mem_fini) {
    void* x = sf_malloc(1); // Padding = 15, internal = 15 + 16 = 31
    sf_malloc(40); // Padding = 8, internal = 31 + 8 + 16 = 55 
    sf_malloc(32); //padding = 0; internal = 55 + 16 = 71 
    sf_malloc(31); // padding = 1; internal = 71 + 1 + 16 = 88

    sf_free(x); // internal = 88 - 31 = 57


    info memo;

    info* meminfo = &memo;
    sf_info(meminfo);

    cr_assert(meminfo->allocations == 4);
    cr_assert(meminfo->frees == 1);
    cr_assert(meminfo->internal == 57);
    cr_assert(meminfo->external == 4096-64-48-48);
}

//8
Test(sf_memsuite, Realloc_padding_size_check_newpage, .init = sf_mem_init, .fini = sf_mem_fini) {
    void* a = sf_malloc(32); //48 padding_size = 0
    sf_header* ahead = (sf_header*)(a-8);
    cr_assert(ahead->padding_size == 0);
    void* b = sf_malloc(16); //32
    sf_header* bhead = (sf_header*)(b-8);
    sf_realloc(a, 1); // padding_size = 15, also causes splinter

    cr_assert(ahead->block_size<<4 == 48);
    cr_assert(ahead->padding_size == 15);

    sf_realloc(b, 5000); // tail of the freelist should coalesc with new page

    cr_assert(bhead->alloc == 0);
    cr_assert((sf_free_header*)bhead == freelist_head);
    cr_assert(((sf_free_header*)bhead)->next != NULL);
}

//9
Test(sf_memsuite, Test_pointers, .init = sf_mem_init, .fini = sf_mem_fini) {
    void* a = sf_malloc(32); // 48
    sf_malloc(16);
    void* b = sf_malloc(96); // 112

    sf_malloc(4096-48-32-112-16); //malloc whole page

    cr_assert(freelist_head == NULL);

    void* c = sf_malloc(12); //32
    sf_free(c);
    sf_header* ahead = (sf_header*)(a-8);
    sf_header* bhead = (sf_header*)(b-8);
    sf_header* chead = (sf_header*)(c-8);
    cr_assert(freelist_head == (sf_free_header*)chead); // c becomes new head
    sf_free(a);
    cr_assert(freelist_head == (sf_free_header*)ahead); // a becomes new head
    cr_assert(freelist_head->next == (sf_free_header*)chead);
    cr_assert(((sf_free_header*)chead)->prev = (sf_free_header*)ahead);
    sf_free(b); 
    cr_assert(freelist_head == (sf_free_header*)bhead); //b becomes new head
    cr_assert(freelist_head->next == (sf_free_header*)ahead);
    cr_assert(((sf_free_header*)ahead)->prev = (sf_free_header*)bhead);
}

//10
Test(sf_memsuite, malloc_more_than_enough_heap, .init = sf_mem_init, .fini = sf_mem_fini) {
    int x;
    int pages = 0;
    for(x = 0; x <= 99; x++) { // Keep asking for more pages, should break and return errno
        sf_malloc(4096-16);
        if(errno == ENOMEM)
            break;
        pages++;           // Calculates how many pages of 4096 bytes the heap can hold
    }
    printf("Pages: %d\n", pages);
    cr_assert(errno == ENOMEM);
}

//11
Test(sf_memsuite, Check_realloc_copy, .init = sf_mem_init, .fini = sf_mem_fini) {
    char *a = sf_malloc(1); //32
    sf_malloc(64);
    *a = 'k'; // sets char 'k' into a

    char* b = sf_realloc(a, 20); 

    cr_assert(*b == 'k'); // checks if *b holds 'k'
}

//12
Test(sf_memsuite, Coalescing_pieces_into_one_block, .init = sf_mem_init, .fini = sf_mem_fini) {
    void* a = sf_malloc(32);
    void* b = sf_malloc(32);
    void* c = sf_malloc(32);
    void* d = sf_malloc(32);
    void* e = sf_malloc(32);

    sf_free(a);
    sf_free(c);
    sf_free(e);

    sf_header* ehead = (sf_header*)(e-8);
    cr_assert(ehead->block_size<<4 == 3904);

    sf_free(d);
    sf_header* chead = (sf_header*)(c-8);
    cr_assert(chead->block_size<<4 == 3904+48+48);

    sf_free(b);
    sf_header* ahead = (sf_header*)(a-8);
    cr_assert(ahead->block_size<<4 == 4096);
}

//13
Test(sf_memsuite, Check_free_cases, .init = sf_mem_init, .fini = sf_mem_fini) {
     void* a = sf_malloc(4); // 32
     sf_malloc(4);
     void* b = sf_malloc(4); // 32

    sf_free(a-8); // should do nothing
    cr_assert(freelist_head != (a-8));
    sf_free(a);
    cr_assert(freelist_head == a-8);
    
    sf_free(b);
    cr_assert(freelist_head == b-8);

    sf_free(a); // freeing a again, shouldn't do anything. b is still freelist_head
    cr_assert(freelist_head == b-8);   
}
//======================================================================================

