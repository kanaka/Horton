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
unsigned char bcol1[SIZE_Y/2+2];
unsigned char bcol2[SIZE_Y/2+2];
unsigned char bcol3[SIZE_Y/2+2];
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
    input_area  = (opt_area  & 0x00000f) << 7;
    input_area |= (opt_area  & 0x0000f0) << 9;
    input_area |= (opt_area  & 0x000f00) >> 7;
    input_area |= (opt_area  & 0x00f000) << 7;

    input_area |= (opt_area  & 0x010000) >> 10;
    input_area |= (opt_area  & 0x100000) >> 8;
    input_area |= (opt_area  & 0x020000) >> 6;
    input_area |= (opt_area  & 0x200000) >> 4;

    input_area |= (opt_area  & 0x040000) >> 18;
    input_area |= (opt_area  & 0x080000) >> 14;
    input_area |= (opt_area  & 0x400000) >> 4;
    input_area |= (opt_area  & 0x800000);

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

print_area(unsigned int area)
{
    unsigned int input_area;
    int x,y,i;

    /* Translate the optimized storage model into the 
     * representational model */
    input_area  = (area  & 0x00000f) << 7;
    input_area |= (area  & 0x0000f0) << 9;
    input_area |= (area  & 0x000f00) >> 7;
    input_area |= (area  & 0x00f000) << 7;

    input_area |= (area  & 0x010000) >> 10;
    input_area |= (area  & 0x100000) >> 8;
    input_area |= (area  & 0x020000) >> 6;
    input_area |= (area  & 0x200000) >> 4;

    input_area |= (area  & 0x040000) >> 18;
    input_area |= (area  & 0x080000) >> 14;
    input_area |= (area  & 0x400000) >> 4;
    input_area |= (area  & 0x800000);

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

dummy(unsigned int x)
{
    int i=x;
    i=i+x;
    puts("\n");
    return;
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
    unsigned int x,y,y1,y2;
    unsigned int area;

    /* Set the block values for outside edges */
    bzero(bcol1, SIZE_Y/2+2);
    bcopy(board[0], &bcol2[1], SIZE_Y/2);
    bcol2[0] =0;
    bcol2[SIZE_Y/2+1] =0;
    bcol3[0] =0;
    bcol3[SIZE_Y/2+1] =0;
    for (x=0; x<SIZE_X/4; x++) {

	/* First inner loop */
	bcopy(board[x+1], &bcol3[1], SIZE_Y/2);
	for (y=0,y1=1,y2=2; y<SIZE_Y/2; y++, y1++, y2++) {
	    /* Calculate area value */
	    area  =  (unsigned int)bcol2[y1];                /* bit 7-10, 13-16 */
	    area |= ((unsigned int)bcol2[y]   & 0xf0) << 4;  /* bit 1-4 */
	    area |= ((unsigned int)bcol2[y2]  & 0x0f) << 12; /* bit 19-22 */

	    area |= ((unsigned int)bcol1[y1]  & 0x88) << 13; /* bit 6,12 */
	    area |= ((unsigned int)bcol3[y1]  & 0x11) << 17; /* bit 11, 17 */

	    area |= ((unsigned int)bcol1[y]   & 0x80) << 11; /* bit 0 */
	    area |= ((unsigned int)bcol1[y2]  & 0x08) << 19; /* bit 18 */
	    area |= ((unsigned int)bcol3[y]   & 0x10) << 15; /* bit 5 */
	    area |= ((unsigned int)bcol3[y2]  & 0x01) << 23; /* bit 23 */

	    ncol1[y] = lookup[area]; 
        }
	
	x++;
	if (x>=(SIZE_X/4)) break;

	/* Second inner loop */
	bcopy(board[x+1], &bcol1[1], SIZE_Y/2);
	for (y=0,y1=1,y2=2; y<SIZE_Y/2; y++, y1++, y2++) {
	    /* Calculate area value */
	    area  =  (unsigned int)bcol3[y1];                /* bit 7-10, 13-16 */
	    area |= ((unsigned int)bcol3[y]   & 0xf0) << 4;  /* bit 1-4 */
	    area |= ((unsigned int)bcol3[y2]  & 0x0f) << 12; /* bit 19-22 */

	    area |= ((unsigned int)bcol2[y1]  & 0x88) << 13; /* bit 6,12 */
	    area |= ((unsigned int)bcol1[y1]  & 0x11) << 17; /* bit 11, 17 */

	    area |= ((unsigned int)bcol2[y]   & 0x80) << 11; /* bit 0 */
	    area |= ((unsigned int)bcol2[y2]  & 0x08) << 19; /* bit 18 */
	    area |= ((unsigned int)bcol1[y]   & 0x10) << 15; /* bit 5 */
	    area |= ((unsigned int)bcol1[y2]  & 0x01) << 23; /* bit 23 */

	    ncol2[y] = lookup[area]; 
	}
	
	x++;
	if (x>=(SIZE_X/4)) break;

	/* Second inner loop */
	bcopy(board[x+1], &bcol2[1], SIZE_Y/2);
	for (y=0,y1=1,y2=2; y<SIZE_Y/2; y++, y1++, y2++) {
	    /* Calculate area value */
	    area  =  (unsigned int)bcol1[y1];                /* bit 7-10, 13-16 */
	    area |= ((unsigned int)bcol1[y]   & 0xf0) << 4;  /* bit 1-4 */
	    area |= ((unsigned int)bcol1[y2]  & 0x0f) << 12; /* bit 19-22 */

	    area |= ((unsigned int)bcol3[y1]  & 0x88) << 13; /* bit 6,12 */
	    area |= ((unsigned int)bcol2[y1]  & 0x11) << 17; /* bit 11, 17 */

	    area |= ((unsigned int)bcol3[y]   & 0x80) << 11; /* bit 0 */
	    area |= ((unsigned int)bcol3[y2]  & 0x08) << 19; /* bit 18 */
	    area |= ((unsigned int)bcol2[y]   & 0x10) << 15; /* bit 5 */
	    area |= ((unsigned int)bcol2[y2]  & 0x01) << 23; /* bit 23 */

	    ncol3[y] = lookup[area]; 
	}

	bcopy(ncol1, board[x-2], SIZE_Y/2);
	bcopy(ncol2, board[x-1], SIZE_Y/2);
	bcopy(ncol3, board[x], SIZE_Y/2);
    }
}


main() {
    unsigned int i,j;
    char * lookup;
    char tmp;

    lookup = create_lookup_table("lookup.tbl", 1);
    if (! lookup ) {
	printf ("Couldn't crate lookup table. Exiting.\n");
	return;
    }

    /*
    print_area(11790337);
    print_block(lookup[11790337]);
    */

    printf("Populating board:\n");
    TIMER_CLEAR;
    TIMER_START;
    populate_board();
    TIMER_STOP;

    printf("Populating TIME: %lf seconds\n",TIMER_ELAPSED);

    print_board_rect(board, 30, 30, 15, 10);
    printf("\n");

    printf("Processing board:\n");
    TIMER_CLEAR;
    TIMER_START;
    for (i=0; i<20; i++) {
	mate(lookup);
	//print_board_rect(board, 30, 30, 15, 10);
	//printf("\n");

    }
    TIMER_STOP;
    printf("Finished processing board\n");

    print_board_rect(board, 30, 30, 15, 10);
    printf("\n");

    printf("Processing TIME: %lf seconds\n",TIMER_ELAPSED);
    printf("Processed %d generations, each averaged: %lf seconds\n",
	   i, TIMER_ELAPSED/i);

    destroy_lookup_table(lookup, 1);
}
