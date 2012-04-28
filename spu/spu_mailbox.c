#include <stdio.h>
#include <libmisc.h>
#include <spu_mfcio.h>
#include <string.h>

#define waitag(t) mfc_write_tag_mask(1<<t); mfc_read_tag_status_all();
#define BUFFER_SIZE 	3072
#define NUMBER_TRIES	1
#define FINISHED		0x08
#define PIXELS_DMA		2000

struct pixel {
	char red;
	char green;
	char blue;
};

struct pixel *patch __attribute__ ((alligned(16)));
struct pixel *zone __attribute__ ((alligned(16)));



/* Places a patch received from the PPU at a certain offset */
void place_patch_in_zone(int offset, int patch_h, int patch_w, int columns)
{
	int i = 0, j, k;
	for (i = 0; i < patch_h; i++) {
		k = i * patch_w;
		j = k * columns + offset * patch_w;
		memcpy((void *)&zone[j], (const void *)&patch[k],
			patch_w * sizeof(struct pixel));
	}
}


int main(unsigned long long speid, unsigned long long argp,
	unsigned long long envp)
{
	int pointer_patch, final_pointer, columns, patch_h, patch_w, i;

	/* Wait for initial parameters and the first patch from SPU */
	while(spu_stat_in_mbox() <= 0);
	final_pointer = spu_read_in_mbox();
	while(spu_stat_in_mbox() <= 0);
	columns = spu_read_in_mbox();
	while(spu_stat_in_mbox() <= 0);
	patch_h = spu_read_in_mbox();
	while(spu_stat_in_mbox() <= 0);
	patch_w = spu_read_in_mbox();

	/* Allocate memory for patch and zone */
	patch = malloc_align(patch_h * patch_w * sizeof(struct pixel), 4);
	zone = malloc_align(columns * patch_h * patch_w * sizeof(struct pixel), 4);
	long zone_size = columns * patch_w * patch_h * sizeof(struct pixel);

	while(spu_stat_in_mbox() <= 0);
	pointer_patch = spu_read_in_mbox();


	/* Read the first patch */
	uint32_t tag_id = mfc_tag_reserve();
	if (tag_id == MFC_TAG_INVALID) {
		printf("\tSPU Cannot allocate tag id\n");
		return -1;
	}
	mfc_get((void *)(patch), (void *)pointer_patch,
		(uint32_t) patch_h * patch_w * sizeof(struct pixel), tag_id, 0, 0);
	waitag(tag_id);
	

	/* Attach the first patch */
	place_patch_in_zone(0, patch_h, patch_w, columns);


	/* Send requests to receive the rest of the patches */
	int repeats = (columns - 1) * NUMBER_TRIES;
	for (i = 0; i < repeats; i++) {
		/* Send the index of the SPU */
		printf("SPU%lld sends request\n", argp);
		spu_write_out_intr_mbox((uint32_t)argp);

		/* Waiting for the pointer to the next patch */
		while(spu_stat_in_mbox() <= 0);
		pointer_patch = spu_read_in_mbox();
		
		/* DMA transfer from main memory to LS*/
		mfc_get((void *)(patch), (void *)pointer_patch,
			(uint32_t) patch_h * patch_w * sizeof(struct pixel), tag_id, 0, 0);
		waitag(tag_id);

		/* Put the patch in the zone */
		place_patch_in_zone(i + 1, patch_h, patch_w, columns);
	}


	/* Send the final zone to the PPU */
	int source = 0, transfer_size = PIXELS_DMA * sizeof(struct pixel);;
	while (zone_size > PIXELS_DMA * sizeof(struct pixel)) {
		
		/* Adjust the transfer size when reachind end of transfer*/
		if (zone_size < 2 * transfer_size)
			transfer_size = zone_size; 

		mfc_put((void *)&zone[source], (void *)final_pointer,
			(uint32_t) transfer_size, tag_id, 0, 0);
		waitag(tag_id);

		final_pointer += PIXELS_DMA * 3;
		source += PIXELS_DMA;
		zone_size -= PIXELS_DMA * sizeof(struct pixel);
	}	


	/* Send FINISHED to SPU */
	spu_write_out_intr_mbox(FINISHED);
	
	return 0;
}
