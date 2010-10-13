/* Horton: Optimized Conway's Game of Life Engine
 * Copyright (C) 2003-2004 Joel Martin
 * Licensed under GPL-3 (see LICENSE.GPL-3)
 *
 * Board:
 * 	- 10,000 X 10,000 cells
 * 	- stored as 2,500 X 5,000 array of blocks
 * Cell Block:
 * 	- 4 X 2 cell area of the board stored in a "unsigned char"
 * 	- x=0, y=0 stored in LSB, x=3, y=1 stored in MSB
 * Cell Area:
 * 	- 6 x 4 cell area of the board stored as "unsigned char"
 * 	- represent 1 block and 8 neighboring blocks
 * 	- bit arrangment:
 * 	     00:   4   of block x+1,y-1
 * 	     01:   3   of block x-1,y
 * 	     02:   4   of block x+1,y
 * 	     03:   7   of block x-1,y-1
 *
 * 	     03:   0   of block x+1,y
 * 	     05:   7   of block x-1,y
 * 	     06:   0   of block x+1,y+1
 * 	     07:   3   of block x-1,y+1
 *        08-11:   4-7 of block x  ,y-1
 *        12-19:   0-7 of block x, y
 *        20-23:   0-3 of block x  ,y+1
 * Lookup Table:
 * 	- array of blocks (4X2 cells), indexed by area (24 bits)
 */

#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>


//#define GATHER_STATS 1
//#define DEBUG_LEVEL 1  //Print terse debug info
#define DEBUG_LEVEL 2  //Print verbose debug info


#define ABS_SIZE_X 10000
#define ABS_SIZE_Y 10000  // 8188/2+2 = 4096 

#if (SIZE_X%4) !=0
X size must be multiple of 4
#endif
#if (SIZE_Y%2) !=0
Y size must be multiple of 2
#endif

#define SIZE_X ABS_SIZE_X/4
#define SIZE_Y ABS_SIZE_Y/2
#define LOOKUP_TABLE_SIZE 16777216

/* Timing stuff */
#define TIMER_CLEAR	(tv1.tv_sec=tv1.tv_usec=tv2.tv_sec=tv2.tv_usec=0)
#define TIMER_START	gettimeofday(&tv1, (struct timezone*)0)
#define TIMER_STOP	gettimeofday(&tv2, (struct timezone*)0)
#define TIMER_ELAPSED   (tv2.tv_sec-tv1.tv_sec+(tv2.tv_usec-tv1.tv_usec)*1.E-6)
struct timeval tv1,tv2;


/* +1 to allow for right and +2 bottom edge */
unsigned char board[SIZE_X+1][SIZE_Y+2];

/* Lookup (read) cache */
unsigned char bcol1[SIZE_Y+4]; /* One extra at the top and three at the bottom */
unsigned char bcol2[SIZE_Y+4];
unsigned char bcol3[SIZE_Y+4];
/* Store (write) cache */
unsigned char ncol1[SIZE_Y];
unsigned char ncol2[SIZE_Y];
unsigned char ncol3[SIZE_Y];

unsigned int * lookup_stats;


/* 
 * Count number of neighboring living cells for a given cell
 */
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
    return(adder);
}

/* 
 * Translate the optimized area representation into 
 * the visual representational model 
 */
unsigned int translate_area(unsigned int opt_area)
{
    unsigned int input_area;
    input_area  = (opt_area  & 0x00f000) >> 5;  /* bits 7-10 */
    input_area |= (opt_area  & 0x0f0000) >> 3;  /* bits 11-16 */
    input_area |= (opt_area  & 0x000f00) >> 7;  /* bits 1-4 */
    input_area |= (opt_area  & 0xf00000) >> 1;  /* bits 19-22 */

    input_area |= (opt_area  & 0x000001) << 5; /* bit 5 */
    input_area |= (opt_area  & 0x000002) << 5; /* bit 6 */
    input_area |= (opt_area  & 0x000004) << 15;  /* bit 17 */
    input_area |= (opt_area  & 0x000008) >> 3; /* bit 0 */

    input_area |= (opt_area  & 0x000010) << 7;  /* bit 11 */
    input_area |= (opt_area  & 0x000020) << 7;  /* bit 12 */
    input_area |= (opt_area  & 0x000040) << 17;  /* bit 23 */
    input_area |= (opt_area  & 0x000080) << 11;  /* bit 18 */
    return(input_area);
}

/* 
 * Figure out the result of one life iteration for a life block.
 * The input block is 24 bits (unsigned int)
 * The output block is 8 bits (char)
 */
char calc_block(unsigned int opt_area) 
{
    unsigned int input_area;
    int input_cell[6][4];
    
    char result_block;
    int result_cell[4][2];
    int x,y,i;

    input_area=translate_area(opt_area);

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

    input_area  = translate_area(opt_area);

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

/*
 * Take a numeric block representation and print it:
 *  - 'O' for a living cell
 *  - '.' for an empty cell
 */
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

/*
 * Print a rectangular region of the board
 *  - 'O' for a living cell
 *  - '.' for an empty cell
 */
print_board_rect(
    unsigned char grid[SIZE_X+1][SIZE_Y+2], 
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
 * Create the lookup table array.
 *  - First try and mmap a pre-generated file.
 *  - If the file doesn't exist calculate the table in memory
 *    dump it to the file and re-mmap it.
 */
char * create_lookup_table(char * filename)
{
    unsigned int i;
    unsigned int j;
    unsigned int k;
    char * table;
    int fd;
    long int rsz;

    fd = open(filename, O_RDONLY, 0600);
    if (fd<1) {
	perror("File open failed on lookup table");
	close(fd);

	table = (char *)malloc(sizeof(char) * LOOKUP_TABLE_SIZE);
	if (! table ) {
	    return(table);
	}
	printf("Populating lookup:\n");
	for (i=0; i<LOOKUP_TABLE_SIZE; i++) {
	    if (i%(LOOKUP_TABLE_SIZE/10)==0) printf (".\n", i);
	    table[i] =calc_block(i);
	}

	/* Writing out lookup table */
	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	write(fd, table, LOOKUP_TABLE_SIZE);
	close(fd);
	free(table);

	printf("Lookup table written -- trying mmap again\n");
	fd = open(filename, O_RDONLY, 0600);
	if (fd<1) {
	    perror("File open failed again on lookup table, giving up");
	    return 0;
	}
    }

    rsz = lseek(fd, 0, SEEK_END);
    if(rsz%4096) {
	rsz+=4096-(rsz%4096);
    }
    //table = mmap(NULL, rsz, PROT_READ, MAP_FILE|MAP_VARIABLE|MAP_SHARED,fd,0);
    table = mmap(NULL, rsz, PROT_READ, MAP_FILE|MAP_SHARED,fd,0);
    if (table==MAP_FAILED) {
	perror("Couldn't mmap");
	return(NULL);
    }
    close(fd);

    return(table);
}


/*
 * Randomly seed the board with 30% living cells (70% empty)
 */
populate_board()
{
    int x,y,bit;

    srand(91823);
    for (y=0; y<SIZE_Y; y++) {
	for (x=0; x<SIZE_X; x++) {
	    board[x][y] = 0;
	    for (bit=0; bit<8; bit++) {
		board[x][y] |= (rand() < (RAND_MAX/3) ? 1 : 0) << bit;
	    }
	}
    }
    /* Fill the right edge column */
    for (y=0; y<SIZE_Y+1; y++) {
	board[SIZE_X][y] = 0;
    }
    /* Fill the bottom edge row */
    for (x=0; x<SIZE_X+1; x++) {
	board[x][SIZE_Y] = 0;
    }
}


/*
 * "loop-unroll" CALC_COL() macro for mate() function 
 */
#ifdef GATHER_STATS
#define CALC_COL(NN1, NN2, NN3) 				\
	bcopy(board[x+1], &bcol##NN3[1], SIZE_Y); 		\
	colptr1 = (unsigned int *)bcol##NN1; 			\
	colptr2 = (unsigned int *)bcol##NN2; 			\
	colptr3 = (unsigned int *)bcol##NN3; 			\
	for (y=0; y<SIZE_Y; y++) { 				\
	    /* Calculate area value */ 				\
	    area2  = (*colptr1 & 0x088880) | 			\
		     (*colptr3 & 0x011110); 			\
	    area = 						\
		((area2 & 0x019800) >> 10) | 			\
		((area2 & 0x000190) >> 4) | 			\
		((area2 & 0x080000) >> 12) | 			\
		((*colptr2 & 0x0ffff0) << 4); 			\
	    ncol##NN1[y] = lookup[area]; 			\
	    lookup_stats[area]++; 				\
	    colptr1 = (unsigned int *) ((char *) colptr1 + 1); 	\
	    colptr2 = (unsigned int *) ((char *) colptr2 + 1); 	\
	    colptr3 = (unsigned int *) ((char *) colptr3 + 1); 	\
        }
#else
#define CALC_COL(NN1, NN2, NN3) 				\
	bcopy(board[x+1], &bcol##NN3[1], SIZE_Y); 		\
	colptr1 = (unsigned int *)bcol##NN1; 			\
	colptr2 = (unsigned int *)bcol##NN2; 			\
	colptr3 = (unsigned int *)bcol##NN3; 			\
	for (y=0; y<SIZE_Y; y++) { 				\
	    /* Calculate area value */ 				\
	    area2  = (*colptr1 & 0x088880) | 			\
		     (*colptr3 & 0x011110); 			\
	    ncol##NN1[y] = lookup[ 				\
		((area2 & 0x019800) >> 10) | 			\
		((area2 & 0x000190) >> 4) | 			\
		((area2 & 0x080000) >> 12) | 			\
		((*colptr2 & 0x0ffff0) << 4)]; 			\
	    colptr1 = (unsigned int *) ((char *) colptr1 + 1); 	\
	    colptr2 = (unsigned int *) ((char *) colptr2 + 1); 	\
	    colptr3 = (unsigned int *) ((char *) colptr3 + 1); 	\
        }
#endif

/* 
 * Calculate a new generation for an entire board.
 */ 
mate(char * lookup)
{
    unsigned int x,y;
    unsigned int area, area2;
    //__unaligned register unsigned int *colptr1,*colptr2, *colptr3;
    register unsigned int *colptr1,*colptr2, *colptr3;

    /* Set the block values for outside edges */
    bzero(bcol1, SIZE_Y+4);
    bcopy(board[0], &bcol2[1], SIZE_Y);
    bcol2[0] =0;
    bcol2[SIZE_Y+1] =0;
    bcol2[SIZE_Y+2] =0;
    bcol2[SIZE_Y+3] =0;
    bcol3[0] =0;
    bcol3[SIZE_Y+1] =0;
    bcol3[SIZE_Y+2] =0;
    bcol3[SIZE_Y+3] =0;
    for (x=0; x<SIZE_X; x++) {

	CALC_COL(1,2,3);

	x++;
	if (x>=(SIZE_X)) {
	    bcopy(ncol1, board[x-1], SIZE_Y);
	    break;
	}

	CALC_COL(2,3,1);

	x++;
	if (x>=(SIZE_X)) {
	    bcopy(ncol1, board[x-2], SIZE_Y);
	    bcopy(ncol2, board[x-1], SIZE_Y);
	    break;
	}

	CALC_COL(3,1,2);

	bcopy(ncol1, board[x-2], SIZE_Y);
	bcopy(ncol2, board[x-1], SIZE_Y);
	bcopy(ncol3, board[x], SIZE_Y);
    }
}

#undef CALC_COL


/*
 * main:
 *  - mmap the lookup table or generate it
 *  - Randomly populate the board
 *  - Iterate through board generations
 *  - Print timing information
 *
 *  - Gather stats and print debug info as requested
 */
int main(int argc, char *argv[]) {
    unsigned int i,j;
    char * lookup;
    float total_time=0.0;
    float min_time=10000.0;
    float max_time=0.0;

    lookup = create_lookup_table("lookup.tbl");
    if (! lookup ) {
	printf ("Couldn't load lookup table. Exiting.\n");
	return -1;
    }

#ifdef GATHER_STATS
    lookup_stats = (unsigned int *)malloc(sizeof(unsigned int) * LOOKUP_TABLE_SIZE);
    for (i=0; i<LOOKUP_TABLE_SIZE; i++) {
	lookup_stats[i] =0;
    }
#endif

    printf("Populating board:\n");
    TIMER_CLEAR;
    TIMER_START;
    populate_board();
    TIMER_STOP;
    printf("Populating TIME: %lf seconds\n",TIMER_ELAPSED);

#ifdef DEBUG_LEVEL    
    print_board_rect(board, SIZE_X-15, SIZE_Y-10, 15, 10);
    printf("\n");
#endif
    printf("Processing board:\n");
    for (i=0; i<30; i++) {
	TIMER_CLEAR;
	TIMER_START;
	mate(lookup);
	TIMER_STOP;
	total_time += TIMER_ELAPSED;
	if (TIMER_ELAPSED > max_time)
	    max_time = TIMER_ELAPSED;
	if (TIMER_ELAPSED < min_time)
	    min_time = TIMER_ELAPSED;
#if DEBUG_LEVEL==2
	print_board_rect(board, SIZE_X-30, SIZE_Y-30, 30, 30);
#endif
	printf("Generation %d took %lf seconds\n",i+1, TIMER_ELAPSED);

    }
    printf("Finished processing board\n");
#ifdef DEBUG_LEVEL
    print_board_rect(board, SIZE_X-30, SIZE_Y-30, 30, 30);
    printf("\n");
#endif

    printf("All generations time:    %10lf seconds (%d generations)\n",
	   total_time,i);
    printf("Max generation time:     %10lf seconds\n",
	   max_time);
    printf("Min generation time:     %10lf seconds\n",
	   min_time);
    printf("Average generation time: %10lf seconds\n",
	   total_time/i);

#ifdef GATHER_STATS
    {
	int fd;
	for (i=0; i<20; i++) {
	    printf("lookup_stats[%d] = %ld\n", i, lookup_stats[i]);
	}
	fd = open("lookup_stats", O_CREAT | O_WRONLY, 0600);
	if (fd<1) {
	    perror("File open failed on lookup table");
	} else {
	    write(fd, lookup_stats, LOOKUP_TABLE_SIZE*4);
	}
	close(fd);
	free(lookup_stats);
    }
#endif

    munmap(lookup, LOOKUP_TABLE_SIZE);
}
