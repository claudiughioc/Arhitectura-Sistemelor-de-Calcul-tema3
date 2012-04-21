#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <libspe2.h>
#include <pthread.h>

#define SPU_THREADS 8
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
	int red;
	int green;
	int blue;
};

void *ppu_pthread_function(void *thread_arg) {
	thread_arg_t *arg = (thread_arg_t *) thread_arg;
	unsigned int entry = SPE_DEFAULT_ENTRY;

	if (spe_context_run(arg->spe, &entry, 0, (void *) arg->cellno, 
				(void *) arg->data, NULL) < 0) {
		perror ("Failed running context");
		exit (1);
	}

	pthread_exit(NULL);
}

void read_from_file(FILE *fin, struct pixel *a, int *width, int *height,
	int *max_color)
{
	char line[256];
	char *numbers, *tok;
	long line_no = 0, i;

	while(fgets(line, sizeof(line), fin) != NULL) {
		/* Exclude comments */
		if (line[0] == '#')
			continue;
		/* Check if the file is ppm */
		if (!line_no && strncmp(line, "P3", 2)) {
			perror("The input file is not ppm");
			return 1;
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
			}
			a = malloc(*width * *height * sizeof(struct pixel));
			if (!a) {
				perror("Error on alocating matrix");
				return 1;
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
				a[i / 3].red = atoi(line);
			if (i % 3 == 1)
				a[i / 3].green = atoi(line);
			if (i % 3 == 2)
				a[i / 3].blue = atoi(line);
			i++;
		}
	}
}



int main(int argc, char **argv)
{
	spe_context_ptr_t ctxs[SPU_THREADS];
	pthread_t threads[SPU_THREADS];
	thread_arg_t arg[SPU_THREADS];
	int zoom, rows, columns, overlap_vert, overlap_oriz;
	long height, width, i, j, k, petic_x, petic_y;
	int max_color;
	struct pixel *a, *result;

	/* Store the arguments */
	if (argc < 8)
		perror("Insert the name of the input file");
	char *input_file = argv[1];
	char *output_file = argv[2];
	zoom = atoi(argv[3]);
	rows = atoi(argv[4]);
	columns = atoi(argv[5]);
	overlap_vert = atoi(argv[6]);
	overlap_oriz = atoi(argv[7]);
	printf("Input file name %s\n", input_file);

	/* Read the matrix */
	FILE *fin = fopen(input_file, "r");
	if (!fin)
		perror("Error while opening input file for reading");
	read_from_file(fin, a, &width, &height, &max_color);
	printf("Successfuly read from file\n");





	/* Create several SPE-threads to execute 'SPU'. */
	for(i=0; i<SPU_THREADS; i++) {
		/* Create context */
		if ((ctxs[i] = spe_context_create (0, NULL)) == NULL) {
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
	}

	// Write mailbox entry: it's definetly one message per SPU so no need for blocking
	for (i=0; i<SPU_THREADS; i++) {
		spe_in_mbox_write(ctxs[i], (void*)&i, 1, SPE_MBOX_ANY_NONBLOCKING);
		printf("[PPU] data sent to SPU# = %d\n",arg[i].data);
	}













	/* Wait for SPU-thread to complete execution. */
	for (i=0; i<SPU_THREADS; i++) {
		if (pthread_join (threads[i], NULL)) {
			perror("Failed pthread_join");
			exit (1);
		}

		/* Destroy context */
		if (spe_context_destroy (ctxs[i]) != 0) {
			perror("Failed destroying context");
			exit (1);
		}
	}

	printf("\nThe program has successfully executed.\n");
	return (0);
}

