#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libspe2.h>
#include <libmisc.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#define SPU_THREADS 	8
#define NUMBER_TRIES	1
#define FINISHED		0x08
#define OK				0x0a
#define PIXELS_DMA		2000
// macro for rounding input value to the next higher multiple of either 
// 16 or 128 (to fulfill MFC's DMA requirements)
#define spu_mfc_ceil128(value)  ((value + 127) & ~127)
#define spu_mfc_ceil16(value)   ((value +  15) &  ~15)

volatile char str[256]  __attribute__ ((aligned(16)));
extern spe_program_handle_t spu_mailbox;

typedef struct {
	int cellno;
	int data;
	spe_context_ptr_t spe;
} thread_arg_t;

struct pixel {
	char red;
	char green;
	char blue;
};

void *ppu_pthread_function(void *thread_arg) {
	thread_arg_t *arg = (thread_arg_t *) thread_arg;
	unsigned int entry = SPE_DEFAULT_ENTRY;
	if (spe_context_run(arg->spe, &entry, 0, (void *) arg->cellno, 
				(void *) arg->data, NULL) < 0) {
		perror ("Failed runnnning context");
		exit (1);
	}

	pthread_exit(NULL);
}

void read_from_file(FILE *fin, struct pixel **a, long *width, long *height,
		int *max_color)
{
	char line[256];
	char *numbers, *tok;
	long line_no = 0, i = 0;

	while(fgets(line, sizeof(line), fin) != NULL) {
		/* Exclude comments */
		if (line[0] == '#')
			continue;
		/* Check if the file is ppm */
		if (!line_no && strncmp(line, "P3", 2)) {
			perror("The input file is not ppm");
			return;
		}
		line_no++;
		if (line_no == 2) {
			numbers = strtok(line, " ");
			*width = atol(numbers);
			while (1) {
				tok = strtok(NULL, " ");
				if (tok == NULL)
					break;
				*height = atol(tok);
			};
			*a = malloc_align(*width * *height * sizeof(struct pixel), 4);
			if (!*a) {
				perror("Error on alocating matrix");
				return;
			}
		}
		if (line_no == 3) {
			*max_color = atoi(line);
			printf("Width = %ld, height = %ld, ", *width, *height);
			printf("Max color = %d\n", *max_color);
		}
		/* Read the RGB values */
		if (line_no > 3) {
			if (i % 3 == 0)
				(*a)[i / 3].red = atoi(line);
			if (i % 3 == 1)
				(*a)[i / 3].green = atoi(line);
			if (i % 3 == 2)
				(*a)[i / 3].blue = atoi(line);
			i++;
		}
	}
}

/* Return a random path from the pool of patches */
struct pixel *get_random_patch(struct pixel**pool, int pool_size)
{
	int pos = rand() % pool_size;
	return pool[pos];
}

struct pixel **build_pool_patches(int count, struct pixel *img, struct pixel **pool,
		int patch_w, int patch_h, int img_w, int img_h)
{
	printf("Building pool patches, h %d, w%d\n", patch_h, patch_w);
	int i, j, k;
	long rand_x, rand_y, pos_img, pos_patch;
	srand(time(NULL));
	pool = malloc_align(count * sizeof(struct pixel *), 4);
	if (!pool)
		return NULL;
	for (i = 0; i < count; i++) {
		pool[i] = malloc_align(patch_w * patch_h * sizeof(struct pixel), 4);
		if (!pool[i]) {
			free(pool);
			return NULL;
		}
		//printf("Successfuly allocated patch %d\n", i);
		rand_x = rand() % (img_h - patch_h);
		rand_y = rand() % (img_w - patch_w);		
		//printf("Patch starts at %ld, %ld\n", rand_x, rand_y);
		for (j = 0; j < patch_h; j++) {
			for (k = 0; k < patch_w; k++) {
				pos_img = (rand_x + j) * img_w + rand_y + k;
				pos_patch = j * patch_w + k;
				//printf("Copy pixel from img[%d] to patch[%d]\n",
				//	pos_img, pos_patch);
				pool[i][pos_patch] = img[pos_img];
			}
		}
	}	
	return pool;
}


void send_random_patch_to_spu(int spu_no, struct pixel **patches, int pool_size,
	spe_context_ptr_t *ctxs)
{
	struct pixel *curr_patch = get_random_patch(patches, pool_size);
	printf("PPU is sending patch to SPU %d, pixel%d %d %d\n", spu_no,
		curr_patch[0].red, curr_patch[0].green, curr_patch[0].blue);
	int pointer = curr_patch;
	spe_in_mbox_write(ctxs[spu_no], (void *)&pointer, 1,
		SPE_MBOX_ANY_NONBLOCKING);
}



int main(int argc, char **argv)
{
	spe_context_ptr_t ctxs[SPU_THREADS];
	pthread_t threads[SPU_THREADS];
	thread_arg_t arg[SPU_THREADS];
	spe_event_unit_t pevents[SPU_THREADS],
			 event_received;
	spe_event_handler_ptr_t event_handler;
	event_handler = spe_event_handler_create();

	int zoom, rows, columns, overlap_vert, overlap_oriz, pos_patch;
	long height, width, i, j, k, petic_x, petic_y;
	int max_color, patch_w, patch_h, pool_size, nevents;
	struct pixel *a = NULL, *result = NULL; 
	struct pixel **patches __attribute__ ((aligned(16))) = NULL;
	

	/* Store the arguments */
	if (argc < 8)
		perror("Insert the name of the input file");
	char *input_file = argv[1];
	char *output_file = argv[2];
	zoom = atoi(argv[3]);
	columns = atoi(argv[4]);
	rows = atoi(argv[5]);
	overlap_vert = atoi(argv[6]);
	overlap_oriz = atoi(argv[7]);


	/* Read the matrix */
	FILE *fin = fopen(input_file, "r");
	if (!fin)
		perror("Error while opening input file for reading");
	read_from_file(fin, &a, &width, &height, &max_color);

	/* Create a pool of several random patches */
	patch_w = width * zoom / columns;
	patch_h = height * zoom / rows;
	pool_size = rows * columns;
	patches = build_pool_patches(pool_size, a, patches, patch_w, patch_h,
			width, height);
	printf("PPU patches e la pointer %x\n", patches);
	if (patches == NULL) {
		printf("Patches e null\n");
		return -1;
	}


	/* Create several SPE-threads to execute 'SPU'. */
	for (i = 0; i < SPU_THREADS; i++) {
		/* Create context */
		if ((ctxs[i] = spe_context_create (SPE_EVENTS_ENABLE, NULL)) == NULL) {
			perror ("Failed creating context");
			exit (1);
		}

		/* Load program into context */
		if (spe_program_load (ctxs[i], &spu_mailbox)) {
			perror ("Failed loading program");
			exit (1);
		}

		/* Send some more paramters to the SPU. */
		arg[i].cellno = i;
		arg[i].spe = ctxs[i];
		arg[i].data = 55 + i;

		/* Create thread for each SPE context */
		if (pthread_create (&threads[i], NULL, &ppu_pthread_function, &arg[i])) {
			perror ("Failed creating thread");
			exit (1);
		}
		/* The type of the event(s) we are working with */
		pevents[i].events = SPE_EVENT_OUT_INTR_MBOX;
		pevents[i].spe = ctxs[i];
		pevents[i].data.u32 = i; // just some data to pass

		spe_event_handler_register(event_handler, &pevents[i]);
	}

	/* Sending initial parameters and the first patch to each SPU */	
	struct pixel *first_patch, *final;
	long spu_zone_pointer;

	/* Create a separate memory location for each SPU */
	struct pixel **pointers = (struct pixel **) malloc_align(SPU_THREADS *
		sizeof(struct pixel *), 4);
	if (!pointers)
		return -1;

	for (i = 0; i < SPU_THREADS; i++) {
		pointers[i] = (struct pixel *)malloc_align(zoom * width * patch_h *
			sizeof(struct pixel), 4);
		if (!pointers[i]) {
			perror("Error on alocatin SPU pointer\n");
			free(pointers);
			return -1;
		}
		/* Send via mailbox: the zone pointer, nr of columns, size of patch */	
		spu_zone_pointer = pointers[i];
		spe_in_mbox_write(ctxs[i], (void *) &spu_zone_pointer, 1, 
			SPE_MBOX_ANY_NONBLOCKING);
		spe_in_mbox_write(ctxs[i], (void *) &columns, 1, 
			SPE_MBOX_ANY_NONBLOCKING);
		spe_in_mbox_write(ctxs[i], (void *) &patch_h, 1,
			SPE_MBOX_ANY_NONBLOCKING);
		spe_in_mbox_write(ctxs[i], (void *) &patch_w, 1,
			SPE_MBOX_ANY_NONBLOCKING);

		/* Send the first patch's pointer */
		first_patch = get_random_patch(patches, pool_size);
		int pointer = (int)first_patch;
		spe_in_mbox_write(ctxs[i], (void *)&pointer, 1,
			SPE_MBOX_ANY_NONBLOCKING);	
	}



	/* Send random patches to fill a column in SPU */
	int repeats = (columns - 1) * NUMBER_TRIES * SPU_THREADS, curr_spu_no = -1;
	for (i = 0; i < repeats; i++) {
		printf("\n\n\nPPU repeat %ld, waiting for event...\n", i);
		nevents = spe_event_wait(event_handler, &event_received, 1, -1);

		/* Processing */
		if (curr_spu_no != -1)
			send_random_patch_to_spu(curr_spu_no, patches, pool_size,
				ctxs);

		if (nevents <= 0)
			continue;
		while(spe_out_intr_mbox_status(event_received.spe) < 1);
		spe_out_intr_mbox_read(event_received.spe, &curr_spu_no, 1,
			SPE_MBOX_ANY_NONBLOCKING);
		printf("PPU am primit request de la SPU %d\n", curr_spu_no);
	}

	
	/* Processing the last requesti */
	if (curr_spu_no != -1)
			send_random_patch_to_spu(curr_spu_no, patches, pool_size,
							ctxs);
	/* Receive final data from SPU */
	for (i = 0; i < SPU_THREADS; i++) {
		int data = OK;	
		spe_in_mbox_write(ctxs[i], (void *) &data, 1, 
			SPE_MBOX_ANY_NONBLOCKING);
	}

		

	/* Received FINISHED signal from SPUs */
	int signal;
	printf("-----------Waiting for finish signal from SPU\n");
	for (i = 0; i < SPU_THREADS; i++) {
	
		nevents = spe_event_wait(event_handler, &event_received, 1, -1);
		if (nevents <= 0)
			continue;
		while(spe_out_intr_mbox_status(event_received.spe) < 1);
		spe_out_intr_mbox_read(event_received.spe, &signal, 1,
			SPE_MBOX_ANY_NONBLOCKING);
		if (signal != FINISHED)
			printf("Message from a SPU which is not FINISH\n");
	}

	/* Print the final image to an output file */
	printf("PPU writing the final image to output file\n");
	FILE *fout = fopen(output_file, "w");
	fprintf(fout, "P3\n");
	printf("PPU patch_h %d, patch_w, %d\n", patch_h, patch_w);
	fprintf(fout, "%d %d\n%d\n", width * zoom, height * zoom, 255);
	for (j = 0; j < SPU_THREADS; j++) {
		final = pointers[j];
		for (i = 0; i < zoom * width * patch_h; i++) {
			fprintf(fout, "%d\n%d\n%d\n", final[i].red,
				final[i].green, final[i].blue);
		}
	}
	close(fout);

	/* Wait for SPU-thread to complete execution. */
	for (i = 0; i < SPU_THREADS; i++) {
		if (pthread_join(threads[i], NULL)) {
			perror("Failed pthread_join");
			exit(1);
		}

		/* Destroy context */
		if (spe_context_destroy(ctxs[i]) != 0) {
			perror("Failed destroying context");
			exit(1);
		}
	}

	printf("\nThe program has successfully executed.\n");
	return 0;
}

