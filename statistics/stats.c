#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/time.h>


#define LOOKUP_TABLE_SIZE 16777216

int compar(const void * pa, const void * pb) 
{
    unsigned long a, b;

    a = *((unsigned long *) pa) & 0xffffffff;
    b = *((unsigned long *) pb) & 0xffffffff;

    if (a < b) {
	return (-1);
    }
    if (a == b) {
	return (0);
    }
    return (1);
}

int main ()
{
    int fd,i;
    unsigned int rsz;

    unsigned int * lookup_stats;
    unsigned long * table;


    /* MMAP the statistics file */
    fd = open("lookup_stats_100", O_RDONLY, 0600);
    if (fd<1) {
	perror("File open failed on lookup table");
	return (-1);
    }

    rsz = lseek(fd, 0, SEEK_END);
    if(rsz%4096) {
        rsz+=4096-(rsz%4096);
    }
    lookup_stats= mmap(NULL, rsz, PROT_READ, MAP_FILE|MAP_VARIABLE|MAP_SHARED,fd,0);
    if (lookup_stats==MAP_FAILED) {
        perror("Couldn't mmap");
        return(NULL);
    }
    close(fd);


    /* Create the half the sorting table from the input stats */
    table = (unsigned long *)malloc(sizeof(unsigned long) * LOOKUP_TABLE_SIZE/2);
    if (! table ) {
	perror("Couldn't malloc table");
	return(-1);
    }

    /* First half */
    for (i=0; i<LOOKUP_TABLE_SIZE/2; i++) {
	table[i]  = lookup_stats[i];
	table[i] |= (unsigned long)i << 32;
    }
    printf("Sorting:\n");
    qsort(table, LOOKUP_TABLE_SIZE/2, 8, compar);

    /* Okay, now do some reporting */
    for (i=0; i<100; i++) {
	printf("table[%.8d] = %.8lx %.8lx\n", LOOKUP_TABLE_SIZE/2-i-1, 
	       (table[LOOKUP_TABLE_SIZE/2-i-1] & 0xffffffff00000000)>>32,
	       (table[LOOKUP_TABLE_SIZE/2-i-1] & 0x00000000ffffffff));
    }       

    /* Second half */
    for (i=0; i<LOOKUP_TABLE_SIZE/2; i++) {
	table[i]  = lookup_stats[i+LOOKUP_TABLE_SIZE/2];
	table[i] |= (unsigned long)(i+LOOKUP_TABLE_SIZE/2) << 32;
    }
    printf("Sorting:\n");
    qsort(table, LOOKUP_TABLE_SIZE/2, 8, compar);

    /* Okay, now do some reporting */
    for (i=0; i<100; i++) {
	printf("table[%.8d] = %.8lx %.8lx\n", LOOKUP_TABLE_SIZE-i-1, 
	       (table[LOOKUP_TABLE_SIZE/2-i-1] & 0xffffffff00000000)>>32,
	       (table[LOOKUP_TABLE_SIZE/2-i-1] & 0x00000000ffffffff));
    }       


    free( table );
}
