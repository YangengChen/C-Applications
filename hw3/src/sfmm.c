#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "sfmm.h"

/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
 void* initialize_newpage_block(sf_free_header** free_ptr);

 void *coalescing(sf_header** head_ptr);

 void removePointers(sf_free_header** ptr);

 sf_free_header* freelist_head = NULL;

 static size_t internal;
 static size_t external;
 static size_t allocations;
 static size_t frees;
 static size_t coalesces;
 static void* lowerLimit = (void*)-1;
 static void* upperLimit;
 static int flag = 0;

 void *sf_malloc(size_t size){
 	if(size > 4*4096 - 16 || size <= 0) {
 		errno = EINVAL;
 		return NULL;
 	}
 	if(lowerLimit == (void*)-1) {
 		lowerLimit = sf_sbrk(0);
 	}
 	size_t divisor = size / 16;
 	size_t modulo = size % 16;
 	size_t padding_size = 16 - modulo;
 	size_t block_size = ((divisor * 16) + modulo +padding_size + 16);
 	if(modulo == 0) {
 		block_size = size + 16;
 		padding_size = 0;
 	}

 	if(freelist_head == NULL) {
 		if(initialize_newpage_block(&freelist_head) != 0) {
 			errno = ENOMEM;
 			return NULL;
 		}
 	}
 	sf_header* header_ptr = &freelist_head->header;

 	sf_free_header* temp_ptr = freelist_head;
 	while((header_ptr->block_size<<4) - 16 < size) {
 		if(temp_ptr->next != NULL) {
 			temp_ptr = temp_ptr->next;
 			header_ptr = &temp_ptr->header;
 		}
 		else {
 			sf_free_header* freelist_new = NULL;
 			while((header_ptr->block_size<<4) - 16 < size) {
 				if(initialize_newpage_block(&freelist_new) == 0){
 					header_ptr = coalescing(&header_ptr);
 					if(flag == 0) {
 						freelist_head->prev = freelist_new;
 						freelist_new->next = freelist_head;
 						freelist_head = freelist_new;
 						header_ptr = (sf_header*)((void*)freelist_head);
 					}
 				} else {
 					errno = ENOMEM;
 					return NULL;
 				}
 			}
 		}
 	}

 	size_t freeblock_size = header_ptr->block_size<<4;
 	size_t remaining_space = freeblock_size - block_size;

 	if(remaining_space < 32){ // CAUSES SPLINTER
 		header_ptr->alloc = 1;
 		header_ptr->block_size = freeblock_size>>4;
 		header_ptr->padding_size = padding_size;

 		sf_footer* footer_ptr = (sf_footer*)((void*)header_ptr + freeblock_size - 8);
 		footer_ptr->alloc = 1;
 		footer_ptr->block_size = freeblock_size>>4;

 		sf_free_header* new_freelist_head = (sf_free_header*)header_ptr;
 		removePointers(&new_freelist_head);
 		if(new_freelist_head->next == NULL && new_freelist_head->prev == NULL) {
 			freelist_head = NULL;
 		}
 		new_freelist_head->next = NULL;
 		new_freelist_head->prev = NULL;
 	} else {
 	  	//spliting & moving to head of the list
 		header_ptr->alloc = 1;
 		header_ptr->block_size = block_size>>4;
 		header_ptr->padding_size = padding_size;

 		void* footer_space = (void*)header_ptr + block_size - 8;
 		sf_footer* footer_ptr = (sf_footer*)footer_space;
 		footer_ptr->alloc = 1;
 		footer_ptr->block_size = block_size>>4;

 		sf_free_header* new_freelist_head = (sf_free_header*)((void*)header_ptr + block_size);
 		sf_free_header* current_header = (sf_free_header*)header_ptr;
 		removePointers(&current_header);
 		new_freelist_head->prev = NULL;

 		if(current_header == freelist_head)
 			new_freelist_head->next = NULL;
 		
 		freelist_head = new_freelist_head;

 		sf_header* split_header_ptr = &freelist_head->header;
 		split_header_ptr->alloc = 0;
 		split_header_ptr->padding_size = 0;
 		split_header_ptr->block_size = remaining_space>>4; 

 		footer_space = (void*)split_header_ptr + remaining_space - 8;
 		sf_footer* split_footer_ptr = (sf_footer*)footer_space;
 		split_footer_ptr->alloc = 0;
 		split_footer_ptr->block_size = remaining_space>>4;
 	}
 	internal += 16 + padding_size;
 	allocations++;
 	return (void*)header_ptr + 8;
 }

 void* initialize_newpage_block(sf_free_header** free_ptr) {
	//Initialize a block header first
 	// pages++;
 	// available_space += 4096;
 	sf_free_header* temp = *free_ptr;
 	if( (*free_ptr =  sf_sbrk(1) )!= (void*)-1) {
 		*free_ptr = sf_sbrk(0) - 4096;
 		(*free_ptr)->header.alloc = 0;
 		(*free_ptr)->header.block_size = 4096>>4;
 		(*free_ptr)->next = NULL;
 		(*free_ptr)->prev = NULL;
	//Initialize the block footer
 		void* footer_space = (void*)*free_ptr + 4096 - 8;
 		sf_footer* footer_ptr = (sf_footer*)footer_space;
 		footer_ptr->alloc = 0;
 		footer_ptr->block_size = 4096>>4;

 		upperLimit = sf_sbrk(0);
 	} else {
 		*free_ptr = temp;
 		return (void*)-1;
 	}
 	return 0;
 }

 void sf_free(void *ptr){
 	if(ptr != NULL && (ptr - 8 >= lowerLimit && ptr < upperLimit) && ((sf_header*)(ptr-8))->alloc == 1) {
 		sf_header* header = (sf_header*)(ptr - 8);
 		internal -= header->padding_size + 16;
 		header->alloc = 0;
 		header->padding_size = 0;
 		sf_footer* footer = (sf_footer*)((void*)header + (header->block_size<<4) - 8);
 		footer->alloc = 0;
 		sf_free_header* free_header = (sf_free_header*)header;
 		free_header->next = NULL;
 		free_header->prev = NULL;
 		coalescing(&header);
 		frees++;
 	}
 }

 void *sf_realloc(void *ptr, size_t size){
 	if(ptr == NULL) 
 		return sf_malloc(size);
 	if(size == 0) {
 		errno = EINVAL;
 		return NULL;
 	}

 	if(ptr - 8 < lowerLimit || ptr > upperLimit || ((sf_header*)(ptr - 8))->alloc != 1) {
 		errno = EINVAL;
 		return NULL;
 	}
 	sf_header* current_header = (sf_header*)(ptr - 8);
 	size_t current_psize = current_header->padding_size;
 	// BLOCK TO REALLOC
 	size_t current_block_size = (current_header->block_size<<4);
 	size_t payload_size = current_block_size - 16;
  	// ----------------
 	// INFO FROM SIZE
 	size_t divisor = size / 16;
 	size_t modulo = size % 16;
 	size_t padding_size = 16 - modulo;
 	size_t block_size_to_allocate = ((divisor * 16) + modulo +padding_size + 16); 
 	if(modulo == 0){
 		block_size_to_allocate = size + 16;
 		padding_size = 0;
 	}
  	// ----------------

 	size_t remaining_space = current_block_size - block_size_to_allocate;

   	if(payload_size >= size) { // REALLOC SIZE IS SMALLER THAN ALLOCATED BLOCK PAYLOAD
   		size_t difference = payload_size - size;
   		if(difference < 16) { // IF DIFFERENCE BETWEEN PAYLOADS < 16, KEEP BLOCK
   			current_header->padding_size = difference;
   			internal -= current_psize + 16;
   			internal += difference + 16;
   		} else {
   			if(remaining_space >= 32) {  // DOES NOT CAUSE A SPLINTER, SPLIT AND FREE REMAINING BLOCK
   				current_header->block_size = block_size_to_allocate>>4;
   				current_header->alloc = 1;
   				current_header->padding_size = padding_size;
   				sf_footer* footer = (sf_footer*)((void*)current_header + (current_header->block_size<<4) - 8);
   				footer->block_size = current_header->block_size;
   				footer->alloc = 1;
  				//NOW WE SPLIT
   				sf_header* next_header = (sf_header*)((void*)footer + 8);
   				next_header->alloc = 0;
   				next_header->block_size = remaining_space>>4;
   				next_header->padding_size  = 0;	
   				// sf_footer* next_footer = (void*)next_header + remaining_space - 8;
   				// next_footer->alloc = 0;
   				// next_footer->block_size = next_header->block_size;
   				sf_free_header* free_header = (sf_free_header*)next_header;
   				free_header->prev = NULL;
   				free_header->next = NULL;
   				frees++;
   				coalescing(&next_header); 

   				internal -= current_psize + 16;
   				internal += padding_size + 16;
   			}
   			else{ // CAUSES A SPLINTER
   				sf_header* next_block = (sf_header*)((void*)current_header + current_block_size);
   				if(next_block->alloc == 0) {  // NEXT BLOCK IS FREE, MERGE THE SPLINTER, AND SET IT AS THE NEW HEAD
   					current_header->block_size = block_size_to_allocate>>4;
   					current_header->alloc = 1;
   					current_header->padding_size = padding_size;
   					sf_footer* footer = (sf_footer*)((void*)current_header + (current_header->block_size<<4) - 8);
   					footer->block_size = current_header->block_size;
   					footer->alloc = 1;

   					sf_free_header* next_block_header = (sf_free_header*)next_block;
   					removePointers(&next_block_header);
   					sf_header* splinter = (sf_header*)((void*)next_block - remaining_space);
   					sf_free_header* splinter_header = (sf_free_header*)splinter;
   					splinter->block_size = ((next_block->block_size<<4) + remaining_space)>>4;
   					splinter->alloc = 0;
   					sf_footer* splinter_footer = (sf_footer*)((void*)splinter + (splinter->block_size<<4) - 8);
   					splinter_footer->block_size = splinter->block_size;
   					splinter_footer->alloc = 0;
   					if(freelist_head != NULL && next_block_header != freelist_head) {
   						splinter_header->next = freelist_head;
   						freelist_head->prev = splinter_header;
   					} else {
   						splinter_header->next = NULL;
   					}
   					splinter_header->prev = NULL;
   					freelist_head = splinter_header;
   					internal -= current_psize + 16;
   					internal += padding_size + 16;	
   				} 
   				else{ // TAKE WHOLE BLOCK
   					current_header->padding_size = padding_size;
   					internal -= current_psize + 16;
   					internal += padding_size + 16;
   				}
   			}
   		}
   	} else { // REALLOC SIZE IS BIGGER THAN ALLOCATED BLOCK PAYLOAD
   		sf_header* next_header = (sf_header*)((void*)current_header + (current_header->block_size<<4));
   		if((void*)next_header < upperLimit) { // CHECK IF NEXT HEADER IS OUT OF BOUND
   			size_t combined_payload_size = (current_header->block_size<<4) + (next_header->block_size<<4) - 16;
   			if(next_header->alloc == 0 && combined_payload_size >= size) { // CHECK IF NEXT BLOCK IS FREE AND HAVE ENOUGH SPACE FOR REALLOCATED SIZE
   				size_t combined_size = combined_payload_size + 16;
   				// COMBINE THE BLOCKS
   				current_header->block_size = combined_size>>4;
   				current_header->padding_size = padding_size;
   				sf_footer* new_footer = (sf_footer*)((void*)current_header + (current_header->block_size<<4) - 8);
   				new_footer->block_size = current_header->block_size;
   				new_footer->alloc = 1;
   				size_t leftovers_size = combined_size - block_size_to_allocate;
   				sf_free_header* free_next_header = (sf_free_header*)next_header;
   				removePointers(&free_next_header);
   				if(leftovers_size >= 32) { // DOES NOT CAUSE A SPLINTER, SPLIT AND FREE REMAINING
   					current_header->block_size = block_size_to_allocate>>4;
   					sf_footer* smaller_footer  = (sf_footer*)((void*)current_header + (current_header->block_size<<4) - 8);
   					smaller_footer->block_size = current_header->block_size;
   					smaller_footer->alloc = 1;
   					// NOW SPLIT
   					sf_header* next_new_header = (sf_header*)((void*)smaller_footer + 8);
   					sf_free_header* free_next_new_header = (sf_free_header*)next_new_header;
   					next_new_header->block_size = leftovers_size>>4;
   					next_new_header->alloc = 0;
   					next_new_header->padding_size = 0;
   						// ADJUST POINTERS FUNCTION
   					// removePointers(&free_next_header);
   					free_next_header->prev = NULL;
   					free_next_header->next = NULL;
   					free_next_new_header->prev = NULL;
   					free_next_new_header->next = NULL;
   					//-------------------------
   					coalescing(&next_new_header);
   					internal -= current_psize + 16;
   					internal += padding_size + 16;
   				} else { // CAUSES A SPLINTER, TAKE WHOLE BLOCK
   					// REMOVE POINTERS OF next_header
   					// removePointers(&free_next_header);
   					//-------------------------------
   					if(free_next_header->prev == NULL && free_next_header->next == NULL) {
   						freelist_head = NULL;
   					}
   					sf_footer* new_footer = (sf_footer*)((void*)current_header + (current_header->block_size<<4) - 8);
   					new_footer->block_size = current_header->block_size;
   					new_footer->alloc = 1;
   					internal -= current_psize + 16;
   					internal += padding_size + 16;
   				}
   		} else { // MALLOC, MEMCPY, FREE
   			void* payload_ptr = sf_malloc(size);
   			if(payload_ptr != NULL) {
   				memcpy(payload_ptr, ptr, payload_size);
   				sf_free(ptr);
   				internal -= current_psize + 16;
   				internal += padding_size + 16;
   			}
   			return payload_ptr;
   		}
   	} else { // MALLOC, MEMCPY, FREE
   		void* payload_ptr = sf_malloc(size);
   		if(payload_ptr != NULL) {
   			memcpy(payload_ptr, ptr, payload_size);
   			sf_free(ptr);
   			internal -= current_psize + 16;
   			internal += padding_size + 16;
   		}
   		return payload_ptr;
   	}
   }
   return ptr;

}
int sf_info(info* meminfo){
	if(meminfo == NULL) {
		return -1;
	} else {
		sf_free_header* temp;
		external = 0; 
		if(freelist_head != NULL) {
			for(temp = freelist_head; temp != NULL; temp = temp->next){
				external += temp->header.block_size<<4;
			}
		}
		meminfo->internal = internal;
		meminfo->external = external;
		meminfo->allocations = allocations;
		meminfo->frees = frees;
		meminfo->coalesce = coalesces;
	}
	return 0;
}

void removePointers(sf_free_header** ptr) {
	sf_free_header* free_ptr = (sf_free_header*)*ptr;
	if(free_ptr->prev != NULL && free_ptr->next != NULL) { // CENTER
		free_ptr->prev->next = free_ptr->next;
		free_ptr->next->prev = free_ptr->prev;
	} else if (free_ptr->prev == NULL && free_ptr->next != NULL) { // HEAD
		free_ptr->next->prev = NULL;
		freelist_head = free_ptr->next;
	} else if(free_ptr->prev != NULL && free_ptr->next == NULL) { // TAIL
		free_ptr->prev->next = NULL;
	} else { // JUST ONE BLOCK
		free_ptr->prev = NULL;
		free_ptr->next = NULL;
	}
}

void *coalescing(sf_header** head_pointer) {
	flag = 0;
	sf_header* head_ptr = *head_pointer;
	if(((void*)head_ptr) + (head_ptr->block_size<<4) < upperLimit){ 
		sf_header* next_header_ptr = (sf_header*)(((void*)head_ptr) + (head_ptr->block_size<<4));
		if(next_header_ptr->alloc == 0) {
			flag++;
 			//combining blocks
			head_ptr->block_size = head_ptr->block_size + next_header_ptr->block_size;

			sf_footer* footer_ptr = (sf_footer*)((void*)head_ptr + (head_ptr->block_size<<4) - 8);
			footer_ptr->block_size = head_ptr->block_size;
			footer_ptr->alloc = 0;
 			//adjusting pointers 
			sf_free_header* free_head_ptr = (sf_free_header*)(head_ptr);
 		 	//case 1: new page is made
			if(free_head_ptr->prev != NULL) {
				free_head_ptr->prev->next = NULL;
				free_head_ptr->prev = NULL;
				if(free_head_ptr != freelist_head){
					free_head_ptr->next = freelist_head;
					freelist_head->prev = free_head_ptr;
				}
				else
					free_head_ptr->next = NULL;
				freelist_head = free_head_ptr;
				return head_ptr;
			}
			sf_free_header* free_next_head_ptr  = (sf_free_header*)(next_header_ptr);
			removePointers(&free_next_head_ptr);
			free_next_head_ptr->next = NULL;
			free_next_head_ptr->prev = NULL;

 			//case 2: block is being freed, and theres no adjacent free block behind it
			if(((void*)(head_ptr) - 8 < lowerLimit) || ((sf_footer*)((void*)head_ptr - 8))->alloc == 1){
				coalesces++;
				if(free_head_ptr != freelist_head && free_next_head_ptr != freelist_head){
					free_head_ptr->next = freelist_head;
					freelist_head->prev = free_head_ptr;
				}
				else
					free_head_ptr->next = NULL;
				free_head_ptr->prev = NULL;
				freelist_head = free_head_ptr;
			}
		}
	}
	if((void*)(head_ptr) - 8 > lowerLimit) {
		sf_footer* prev_footer_ptr = (sf_footer*)((void*)head_ptr - 8);
		if(prev_footer_ptr->alloc == 0) {
			flag++;
 			//combining blocks
			sf_header* prev_head_ptr = (sf_header*)((void*)prev_footer_ptr + 8 - (prev_footer_ptr->block_size<<4));
			prev_head_ptr->block_size = prev_footer_ptr->block_size + head_ptr->block_size;
			sf_footer* footer_ptr = (void*)prev_head_ptr + (prev_head_ptr->block_size<<4) - 8;
			footer_ptr->block_size = prev_head_ptr->block_size;
			footer_ptr->alloc = 0;
 			//adjusting pointers
			sf_free_header* free_prev_head_ptr = (sf_free_header*)(prev_head_ptr);
			removePointers(&free_prev_head_ptr);
			if(free_prev_head_ptr != freelist_head){
				if((void*)freelist_head < (void*)free_prev_head_ptr + (prev_head_ptr->block_size<<4) && (void*)freelist_head > (void*)free_prev_head_ptr){
					free_prev_head_ptr->next = NULL;
					freelist_head->prev = NULL;
				} else {				
					free_prev_head_ptr->next = freelist_head;
					freelist_head->prev = free_prev_head_ptr;
				}
			}
			else{
				removePointers(&free_prev_head_ptr);
			}
			free_prev_head_ptr->prev = NULL;
			freelist_head = free_prev_head_ptr; 

			coalesces++;
			return prev_footer_ptr;
		}
	}
	if(flag == 0) {
		sf_footer* footer_ptr = (sf_footer*)((void*)(head_ptr) + (head_ptr->block_size<<4) - 8);
		footer_ptr->alloc = 0;
		footer_ptr->block_size = head_ptr->block_size;
		sf_free_header* free_header_ptr = (sf_free_header*)(head_ptr);
		if(freelist_head != NULL) {
			free_header_ptr->next = freelist_head;
			freelist_head->prev = free_header_ptr;
		}
		free_header_ptr->prev = NULL;
		freelist_head = free_header_ptr;

	}
	return head_ptr;
}