#include <stdio.h>
#include <spu_mfcio.h>

#define waitag(t) mfc_write_tag_mask(1<<t); mfc_read_tag_status_all();
#define BUFFER_SIZE 3072

struct pixel {
	char red;
	char green;
	char blue;
};

struct pixel *patch __attribute__ ((alligned(16)));;

volatile char buffer[BUFFER_SIZE] __attribute__ ((aligned(128)));

int main(unsigned long long speid, unsigned long long argp, unsigned long long envp){
	printf("Start SPU %lld\n", argp);
	int pointer_patch, rows, patch_h, patch_w;

	/* Wait for initial parameters and the first patch from SPU */
	while(spu_stat_in_mbox() <= 0);
	rows = spu_read_in_mbox();
	while(spu_stat_in_mbox() <= 0);
	patch_h = spu_read_in_mbox();
	while(spu_stat_in_mbox() <= 0);
	patch_w = spu_read_in_mbox();
	//printf("SPU %lld am primit prin mailbox: rows %d, ph %d, pw%d\n", argp,
	//	rows, patch_h, patch_w);
	patch = malloc(patch_h * patch_w * sizeof(struct pixel));
	while(spu_stat_in_mbox() <= 0);
	pointer_patch = spu_read_in_mbox();
	//printf("SPU %lld primeste pointer %d\n", argp, pointer_patch);


	printf("SPU %lld gets before reading patch\n", argp);
	/* Read the first patch */
	uint32_t tag_id = mfc_tag_reserve();
	if (tag_id == MFC_TAG_INVALID) {
		printf("Cannot allocate tag id\n");
		return -1;
	}
	mfc_get((void *)(patch), (void *)pointer_patch,
		(uint32_t) patch_h * patch_w * sizeof(struct pixel), tag_id, 0, 0);
	waitag(tag_id);
	printf("SPU %lld primeste pixel: %d, %d, %d\n", argp, patch[0].red,
		patch[0].green, patch[0].blue);



	return 0;
}

