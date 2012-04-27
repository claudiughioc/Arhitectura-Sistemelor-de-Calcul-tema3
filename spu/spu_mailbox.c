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

volatile char buffer[BUFFER_SIZE] __attribute__ ((aligned(128)));

void place_patch_in_zone(int offset,int patch_h, int patch_w,
			int rows, unsigned long long argp)
{
	int i = 0, j, k;
	//printf("SPU %lld Height %d, width %d\n", argp, patch_h, patch_w);
	for (i = 0; i < patch_h; i++) {
		k = i * patch_w;
		j = k * rows + offset * patch_w;
		//printf("SPU %lld Iau de la %d din patch si dau la %d in zone\n",
		//	 argp, k, j);
		memcpy((void *)&zone[j], (const void *)&patch[k],
			patch_w * sizeof(struct pixel));
		//printf("SPU %lld zone are la %d pixel %d, %d, %d\n", argp, j,
		//	zone[j].red, zone[j].green, zone[j].blue);
	}

}

int main(unsigned long long speid, unsigned long long argp, unsigned long long envp){
	//printf("\tStart SPU %lld\n", argp);
	unsigned int pointer_patch, final_pointer, rows, patch_h, patch_w, i, j, k;

	/* Wait for initial parameters and the first patch from SPU */
	while(spu_stat_in_mbox() <= 0);
	final_pointer = spu_read_in_mbox();
	while(spu_stat_in_mbox() <= 0);
	rows = spu_read_in_mbox();
	while(spu_stat_in_mbox() <= 0);
	patch_h = spu_read_in_mbox();
	while(spu_stat_in_mbox() <= 0);
	patch_w = spu_read_in_mbox();
	//printf("SPU %lld am primit prin mailbox: rows %d, ph %d, pw%d\n", argp,
	//	rows, patch_h, patch_w);
	/* Allocate memory for patch and zone */
	patch = malloc_align(patch_h * patch_w * sizeof(struct pixel), 4);
	zone = malloc_align(rows * patch_h * patch_w * sizeof(struct pixel), 4);
	long zone_size = rows * patch_w * patch_h * sizeof(struct pixel);
	printf("SPU %lld sizeof(zone) = %ld\n", argp, zone_size);
	while(spu_stat_in_mbox() <= 0);
	pointer_patch = spu_read_in_mbox();
	//printf("SPU %lld primeste pointer %d\n", argp, pointer_patch);


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
	place_patch_in_zone(0, patch_h, patch_w, rows, argp);



	/* Send requests to receive the rest of the patches */
	int repeats = (rows - 1) * NUMBER_TRIES, position = patch_h	* patch_w;
	for (i = 0; i < repeats; i++) {
		//printf("\tSPU %lld trimite request nr %d\n", argp, i);
		spu_write_out_intr_mbox((uint32_t)argp);

		/* Waiting for the pointer to the next patch */
		while(spu_stat_in_mbox() <= 0);
		pointer_patch = spu_read_in_mbox();
		
		/* DMA transfer */
		mfc_get((void *)(patch), (void *)pointer_patch,
						(uint32_t) patch_h * patch_w * sizeof(struct pixel), tag_id, 0, 0);
		waitag(tag_id);
		//printf("\tSPU %lld primeste patch nr %d, pixel %d %d %d\n", argp, i,
		//	patch[0].red, patch[0].green, patch[0].blue);

		/* Put the patch in the zone */
		place_patch_in_zone(i + 1, patch_h, patch_w, rows, argp);
	}

	/* Print the zone to an output file */
	char file_name[6] = "f_out";
	file_name[5] = (char) argp;
	FILE *fout = fopen(file_name, "w");
	fprintf(fout, "P3\n");
	fprintf(fout, "%d %d\n%d\n", patch_w * rows, patch_h, 255);
	for (i = 0; i < rows * patch_h * patch_w; i++) {
			fprintf(fout, "%d\n%d\n%d\n", zone[i].red,
					zone[i].green, zone[i].blue);
	}
	close(fout);

	/* Send the final zone to the PPU */
	int source = 0;
	while (zone_size > source * sizeof(struct pixel)) {
		printf("SPU %lld will send from %x to pointer %x\n", 
			argp,&zone[source], final_pointer);
		mfc_put((void *)&zone[source], (void *)final_pointer,
			(uint32_t) PIXELS_DMA * sizeof(struct pixel), tag_id, 0, 0);
		waitag(tag_id);
		printf("SPU %lld passed the tag\n", argp);
		final_pointer += PIXELS_DMA;
		source += PIXELS_DMA;
		break;
	}	
	printf ("SPU %lld trimite FINISH\n", argp);
	/* Send FINISHED to SPU */
	spu_write_out_intr_mbox(FINISHED);
	
	return 0;
}
