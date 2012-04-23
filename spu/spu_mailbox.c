#include <stdio.h>
#include <spu_mfcio.h>

#define BUFFER_SIZE 3072

struct pixel {
	char red;
	char green;
	char blue;
};

volatile char buffer[BUFFER_SIZE] __attribute__ ((aligned(128)));

int main(unsigned long long speid, unsigned long long argp, unsigned long long envp){
	printf("SPU %lld primesc ca date prin parametru la main %lld\n", argp, envp);
	int pointer_patch, rows, patch_h, patch_w;

	/* Wait for initial parameters and the first patch from SPU */
	while(spu_stat_in_mbox() <= 0);
	rows = spu_read_in_mbox();
	while(spu_stat_in_mbox() <= 0);
	patch_h = spu_read_in_mbox();
	while(spu_stat_in_mbox() <= 0);
	patch_w = spu_read_in_mbox();
	while(spu_stat_in_mbox() <= 0);
	pointer_patch = spu_read_in_mbox();
	printf("SPU %lld am primit prin mailbox: rows %d, ph %d, pw%d\n", argp,
		rows, patch_h, patch_w);

}

