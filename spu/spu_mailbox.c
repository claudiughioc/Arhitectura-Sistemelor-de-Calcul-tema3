// Author:	Alex O.
// Date:	02/15/2010
// Version:	1

#include <stdio.h>
#include <spu_mfcio.h>

int main(unsigned long long speid, unsigned long long argp, unsigned long long envp){
	
	uint32_t mbox_data;

    while (spu_stat_in_mbox()<=0); // busy-waiting... 
    // daca aveam ceva de facut in acest timp, unde scriam codul corespunzator?
    mbox_data = spu_read_in_mbox();

    printf("[SPU %d] received data = %d.\n", (int) argp, (int)mbox_data);
	if (mbox_data == (uint32_t)envp)
        printf("[SPU %d] data checks with envp\n", (int) argp);
	else
        printf("[SPU %d] data check failed\n", (int) argp);
	
	return 0;
}
	
