#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/time.h>

#define SIZE_X 10000
#define SIZE_Y 10000
#define LOOKUP_TABLE_SIZE 16777216

#if (SIZE_X%4) !=0
X size must be multiple of 4
#endif
#if (SIZE_Y%2) !=0
Y size must be multiple of 2
#endif


/* Timing stuff */
#define TIMER_CLEAR	(tv1.tv_sec=tv1.tv_usec=tv2.tv_sec=tv2.tv_usec=0)
#define TIMER_START	gettimeofday(&tv1, (struct timezone*)0)
#define TIMER_STOP	gettimeofday(&tv2, (struct timezone*)0)
#define TIMER_ELAPSED   (tv2.tv_sec-tv1.tv_sec+(tv2.tv_usec-tv1.tv_usec)*1.E-6)
struct timeval tv1,tv2;


/* +1 to allow for right and bottom edge conditions */
unsigned char board[SIZE_X/4+1][SIZE_Y/2+1];

/* Lookup (read) cache */
unsigned char bcol1[SIZE_Y/2+4]; /* One extra at the top and three at the bottom */
unsigned char bcol2[SIZE_Y/2+4];
unsigned char bcol3[SIZE_Y/2+4];
/* Store (write) cache */
unsigned char ncol1[SIZE_Y/2];
unsigned char ncol2[SIZE_Y/2];
unsigned char ncol3[SIZE_Y/2];



/* Count number of neighboring living cells for a given cell */
int count_neighbors(int bit_array[6][4], int x, int y) {
    int adder,i,j;
  
    adder=0;

    for (i=x-1; i<=x+1; i++) {
	for (j=y-1; j<=y+1; j++) {
	    if ((i!=x) || (j!=y)) {
		adder=adder+bit_array[i][j];
	    }
	}
    }
//    printf("count: x=%d, y=%d, adder=%d\n", x, y, adder);
    return(adder);
}

/* Figure out the result of one life iteration for a life block.
 * The input block is 24 bits (unsigned int)
 * The output block is 8 bits (char)
 * an char (8 bits) */
char calc_block(unsigned int opt_area) 
{
    unsigned int input_area;
    int input_cell[6][4];
    
    char result_block;
    int result_cell[4][2];
    int x,y,i;

    /* Translate the optimized storage model into the 
     * representational model */
    input_area  = (opt_area  & 0x0000f0) << 3;  /* bits 7-10 */
    input_area |= (opt_area  & 0x000f00) << 5;  /* bits 11-16 */
    input_area |= (opt_area  & 0x00000f) << 1;  /* bits 1-4 */
    input_area |= (opt_area  & 0x00f000) << 7;  /* bits 19-22 */

    input_area |= (opt_area  & 0x010000) >> 11; /* bit 5 */
    input_area |= (opt_area  & 0x020000) >> 11; /* bit 6 */
    input_area |= (opt_area  & 0x040000) >> 1;  /* bit 17 */
    input_area |= (opt_area  & 0x080000) >> 19; /* bit 0 */

    input_area |= (opt_area  & 0x100000) >> 9;  /* bit 11 */
    input_area |= (opt_area  & 0x200000) >> 9;  /* bit 12 */
    input_area |= (opt_area  & 0x400000) << 1;  /* bit 23 */
    input_area |= (opt_area  & 0x800000) >> 5;  /* bit 18 */

    /* Populate input_cell array from input_area*/
    for (x=0; x<6; x++) {
	for (y=0; y<4; y++) {
	    if ((input_area >> (y*6+x)) & 0x1) {
		input_cell[x][y] =1;
	    } else {
		input_cell[x][y] =0;
	    }
	}
    }

    for (x=1; x<5; x++) {
	for (y=1; y<3; y++) {
	    switch (count_neighbors(input_cell, x, y)) {
		case 3:
		    result_cell[x-1][y-1] =1;
		    break;
		case 2:
		    if (input_cell[x][y]) {
			result_cell[x-1][y-1] =1;
		    } else {
			result_cell[x-1][y-1] =0;
		    }
		    break;
		default:
		    result_cell[x-1][y-1]=0;
		    break;
	    }
	}
    }

    /* Create result_block from result_cell array */
    result_block = 0;
    for (x=0; x<4; x++) {
	for (y=0; y<2; y++) {
	    if (result_cell[x][y] == 1) {
		result_block |= (0x1 << y*4+x);
	    }
	}
    }

    return(result_block);
}

print_area(unsigned int opt_area)
{
    unsigned int input_area;
    int x,y,i;

    /* Translate the optimized storage model into the 
     * representational model */
    input_area  = (opt_area  & 0x0000f0) << 3;  /* bits 7-10 */
    input_area |= (opt_area  & 0x000f00) << 5;  /* bits 11-16 */
    input_area |= (opt_area  & 0x00000f) << 1;  /* bits 1-4 */
    input_area |= (opt_area  & 0x00f000) << 7;  /* bits 19-22 */

    input_area |= (opt_area  & 0x010000) >> 11; /* bit 5 */
    input_area |= (opt_area  & 0x020000) >> 11; /* bit 6 */
    input_area |= (opt_area  & 0x040000) >> 1;  /* bit 17 */
    input_area |= (opt_area  & 0x080000) >> 19; /* bit 0 */

    input_area |= (opt_area  & 0x100000) >> 9;  /* bit 11 */
    input_area |= (opt_area  & 0x200000) >> 9;  /* bit 12 */
    input_area |= (opt_area  & 0x400000) << 1;  /* bit 23 */
    input_area |= (opt_area  & 0x800000) >> 5;  /* bit 18 */

    /* Populate input_cell array from input_area*/
    for (y=0; y<4; y++) {
	for (x=0; x<6; x++) {
	    if ((input_area >> (y*6+x)) & 0x1) {
		printf("O");
	    } else {
		printf(".");
	    }
	}
	printf("\n");
    }
}

print_block(char block)
{
    int x, y;
    for (y=0; y<2; y++) {
	for (x=0; x<4; x++) {
	    if ((block >> (y*4+x)) & 0x1) {
		printf("O");
	    } else {
		printf(".");
	    }
	}
	printf("\n");
    }
}

print_board_rect(
    unsigned char grid[SIZE_X/4+1][SIZE_Y/2+1], 
    unsigned int startx, 
    unsigned int starty, 
    unsigned int width, 
    unsigned int height )
{
    unsigned int i,j,k,l,block;

    for (i=0; i<height; i++) {
	for (l=0; l<2; l++) {
	    for (j=0; j<width; j++) {
		block=grid[startx+j][starty+i];
		for (k=0; k<4; k++) {
		    if ((block >> (l*4+k)) & 0x1) {
			printf("O");
		    } else {
			printf(".");
		    }
		}
	    }
	    printf("\n");
	}
    }
}

/* 
 * Create the lookup table. How and where to store it depends on mode
 *
 * mode:
 * 	0 - create the lookup table from scratch in memory
 * 	1 - mmap the file from disk to memory
 * 	2 - create the lookup table from scratch in memory and copy disk
 *
 * filename: 
 * 	only applicable in modes 1 and 2
 */
char * create_lookup_table(char * filename, int mode) 
{
    unsigned int i;
    unsigned int j;
    unsigned int k;
    char * table;
    int fd;
    long int rsz;

    if ((mode == 0) || (mode == 2)) {
	table = (char *)malloc(sizeof(char) * LOOKUP_TABLE_SIZE);
	if (! table ) {
	    return(table);
	}
	printf("Populating lookup:\n");
	for (i=0; i<LOOKUP_TABLE_SIZE; i++) {
	    if (i%(LOOKUP_TABLE_SIZE/10)==0) printf (".\n", i);
	    table[i] =calc_block(i);
	}
    }

    if (mode == 1) {
	fd = open(filename, O_RDONLY, 0600);
	if (fd<1) {
	    perror("File open failed on lookup table: ");
	    return (NULL);
	}
	rsz = lseek(fd, 0, SEEK_END);
	if(rsz%4096) {
	    rsz+=4096-(rsz%4096);
	}
	table = mmap(NULL, rsz, PROT_READ, MAP_FILE|MAP_VARIABLE|MAP_SHARED,fd,0);
	if (errno) {
	    perror("Couldn't mmap: ");
	    return(NULL);
	}
	close(fd);
    }

    if (mode == 2) {
	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	write(fd, table, LOOKUP_TABLE_SIZE);
	close(fd);
    }


    return(table);
}


destroy_lookup_table(char * table, int mode) 
{
    if ((table) && ((mode==0) ||(mode==2))) {
	free(table);
    }
    if ((table) && (mode==1)) {
	munmap(table, 16777216);
    }
    return;
}

populate_board()
{
    int x,y,bit;

    srand(91823);
    for (y=0; y<SIZE_Y/2; y++) {
	for (x=0; x<SIZE_X/4; x++) {
	    board[x][y] = 0;
	    for (bit=0; bit<8; bit++) {
		board[x][y] |= (rand() < (RAND_MAX/3) ? 1 : 0) << bit;
	    }
	}
    }
    /* Fill the right edge column */
    for (y=0; y<SIZE_Y/2+1; y++) {
	board[SIZE_X/4][y] = 0;
    }
    /* Fill the bottom edge row */
    for (x=0; x<SIZE_X/4+1; x++) {
	board[x][SIZE_Y/2] = 0;
    }
}


mate(char * lookup)
{
    unsigned int x,y;
    unsigned int area, area2;
    __unaligned register unsigned int *colptr1,*colptr2, *colptr3;

    /* Set the block values for outside edges */
    bzero(bcol1, SIZE_Y/2+4);
    bcopy(board[0], &bcol2[1], SIZE_Y/2);
    bcol2[0] =0;
    bcol2[SIZE_Y/2+1] =0;
    bcol2[SIZE_Y/2+2] =0;
    bcol2[SIZE_Y/2+3] =0;
    bcol3[0] =0;
    bcol3[SIZE_Y/2+1] =0;
    bcol3[SIZE_Y/2+2] =0;
    bcol3[SIZE_Y/2+3] =0;
    for (x=0; x<SIZE_X/4; x++) {

#define CALC_COL(NN1, NN2, NN3) \
	bcopy(board[x+1], &bcol##NN3[1], SIZE_Y/2); \
	colptr1 = (unsigned int *)bcol##NN1; \
	colptr2 = (unsigned int *)bcol##NN2; \
	colptr3 = (unsigned int *)bcol##NN3; \
	for (y=0; y<SIZE_Y/2; y++) { \
	    /* Calculate area value */ \
	    area2  = (*colptr1 & 0x088880) | (*colptr3 & 0x011110); \
	    ncol##NN1[y] = lookup[ \
	    ((area2 & 0x019800) << 6) | \
	    ((area2 & 0x000190) << 12) | \
	    ((area2 & 0x080000) << 4) | \
	    ((*colptr2 & 0x0ffff0) >> 4)]; \
	    colptr1 = (unsigned int *) ((char *) colptr1 + 1); \
	    colptr2 = (unsigned int *) ((char *) colptr2 + 1); \
	    colptr3 = (unsigned int *) ((char *) colptr3 + 1); \
        }

	CALC_COL(1,2,3);

	x++;
	if (x>=(SIZE_X/4)) {
	    bcopy(ncol1, board[x-1], SIZE_Y/2);
	    break;
	}

	CALC_COL(2,3,1);

	x++;
	if (x>=(SIZE_X/4)) {
	    bcopy(ncol1, board[x-2], SIZE_Y/2);
	    bcopy(ncol2, board[x-1], SIZE_Y/2);
	    break;
	}

	CALC_COL(3,1,2);

	bcopy(ncol1, board[x-2], SIZE_Y/2);
	bcopy(ncol2, board[x-1], SIZE_Y/2);
	bcopy(ncol3, board[x], SIZE_Y/2);
    }
}


main() {
    unsigned int i,j;
    char * lookup;
    float total_time=0.0;
    float min_time=10000.0;
    float max_time=0.0;

    lookup = create_lookup_table("lookup.tbl", 1);
    if (! lookup ) {
	printf ("Couldn't crate lookup table. Exiting.\n");
	return;
    }

/*
    print_area(1656032);
    print_block(lookup[1656032]);
    printf("\n");
*/

    printf("Populating board:\n");
    TIMER_CLEAR;
    TIMER_START;
    populate_board();
    TIMER_STOP;
    printf("Populating TIME: %lf seconds\n",TIMER_ELAPSED);

    
    print_board_rect(board, 2485, 4990, 15, 10);
    printf("\n");
    printf("Processing board:\n");
    for (i=0; i<20; i++) {
	TIMER_CLEAR;
	TIMER_START;
	mate(lookup);
	TIMER_STOP;
	total_time += TIMER_ELAPSED;
	if (TIMER_ELAPSED > max_time)
	    max_time = TIMER_ELAPSED;
	if (TIMER_ELAPSED < min_time)
	    min_time = TIMER_ELAPSED;
//	print_board_rect(board, 2485, 4990, 16, 11);
	printf("Generation %d took %lf seconds\n",i+1, TIMER_ELAPSED);

    }
    printf("Finished processing board\n");
    print_board_rect(board, 2485, 4990, 15, 10);
    printf("\n");

    printf("All generations time:    %10lf seconds (%d generations)\n",
	   total_time,i);
    printf("Max generation time:     %10lf seconds\n",
	   max_time);
    printf("Min generation time:     %10lf seconds\n",
	   min_time);
    printf("Average generation time: %10lf seconds\n",
	   total_time/i);

    destroy_lookup_table(lookup, 1);
}
