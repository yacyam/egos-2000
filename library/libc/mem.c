#include "mem.h"

#define YNULL (void*)0
#define SBRK_PAD 0x100

struct segment_info {
  uint size;
	struct segment_info *next;
};

struct free_list {
  struct segment_info *head;
};

struct free_list flst = {
	.head = YNULL
};

void *brk = (void*)STACK_VBOTTOM + PAGE_SIZE;

void flst_init() {
	struct segment_info *head = (void*)STACK_VBOTTOM;
	head->size = (uint)brk - (uint)head - sizeof(struct segment_info);
	head->next = YNULL;
	flst.head = head;
}

void flst_push(struct segment_info *seg) {
	seg->next = flst.head;
	flst.head = seg;
}

struct segment_info *flst_find(uint size) {
	struct segment_info *head = flst.head;

	while (head != YNULL) {
		if (head->size >= size + sizeof(struct segment_info))
			return head;
		head = head->next;
	}
	head = brk;
	brk += size + SBRK_PAD + sizeof(struct segment_info);
	head->size = size + SBRK_PAD;
	flst_push(head);
	return head;
}

void *ymalloc(uint size) {
	if (flst.head == YNULL)
		flst_init();
	
	struct segment_info *in_use, *seg = flst_find(size);
	seg->size -= size + sizeof(struct segment_info);

	in_use = (void*)seg + sizeof(struct segment_info) + seg->size;
	in_use->size = size;
	in_use->next = YNULL;
	return (void*)in_use + sizeof(struct segment_info);
}

void yfree(void *loc) {
	struct segment_info *seg = loc - sizeof(struct segment_info);
	flst_push(seg);
}