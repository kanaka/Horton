#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <sys/time.h>

#define SIZE_X 10000
#define SIZE_Y 10000

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
char board[SIZE_X/4+2][SIZE_Y/2+2];


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
char calc_block(unsigned int input_area) 
{
    int input_cell[6][4];
    
    char result_block;
    int result_cell[4][2];
    int x,y,i;

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
    int x, y;
    for (y=0; y<4; y++) {
	for (x=0; x<6; x++) {
	    if ((area >> (y*6+x)) & 0x1) {
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
    char grid[SIZE_X/4+2][SIZE_Y/2+2], 
    unsigned int startx, 
    unsigned int starty, 
    unsigned int width, 
    unsigned int height )
{
    unsigned int i,j,k,l,block;

    for (i=0; i<height; i++) {
	for (l=0; l<2; l++) {
	    for (j=0; j<width; j++) {
		block=grid[startx+j+1][starty+i+1];
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

populate_board()
{
    int x,y,bit;

    srand(91823);
    for (y=1; y<=SIZE_Y/2; y++) {
	for (x=1; x<=SIZE_X/4; x++) {
	    board[x][y] = 0;
	    for (bit=0; bit<8; bit++) {
		board[x][y] |= (rand() < (RAND_MAX/3) ? 1 : 0) << bit;
	    }
	}
    }
    /* Fill the left and right edge columns */
    for (y=0; y<SIZE_Y/2+1; y++) {
	board[0][y] = 0;
	board[SIZE_X/4+1][y] = 0;
    }
    /* Fill the bottom edge row */
    for (x=0; x<SIZE_X/4+1; x++) {
	board[x][0] = 0;
	board[x][SIZE_Y/2+1] = 0;
    }
}


mate()
{
    unsigned int x,y,x1,y1,x2,y2;
    unsigned int area, adder;
    char result;

    /* Set the block values for outside edges */
    for (x=1; x<=SIZE_X/4; x++) {
	for (y=1; y<=SIZE_Y/2; y++) {
	    area  = ((unsigned int)board[x][y]      & 0x0f) << 7;  /* bits 7-10 */
	    area |= ((unsigned int)board[x][y]      & 0xf0) << 9;  /* bits 13-16 */
	    area |= ((unsigned int)board[x][y-1]    & 0xf0) >> 3;  /* bits 1-4 */
	    area |= ((unsigned int)board[x][y+1]    & 0x0f) << 19; /* bits 19-22 */

	    area |= ((unsigned int)board[x-1][y]    & 0x08) << 3;  /* bit 6 */
	    area |= ((unsigned int)board[x-1][y]    & 0x80) << 5;  /* bit 12 */
	    area |= ((unsigned int)board[x+1][y]    & 0x01) << 11; /* bit 11 */
	    area |= ((unsigned int)board[x+1][y]    & 0x10) << 13; /* bit 17 */

	    area |= ((unsigned int)board[x-1][y-1]  & 0x80) >> 7;  /* bit 0 */
	    area |= ((unsigned int)board[x-1][y+1]  & 0x08) << 15; /* bit 18 */
	    area |= ((unsigned int)board[x+1][y-1]  & 0x10) << 1;  /* bit 5 */
	    area |= ((unsigned int)board[x+1][y+1]  & 0x01) << 23; /* bit 23 */

	    /* Calculate block */
/*
	    result =0;
	    for (y1=1; y1<=2; y1++) {
		for (x1=1; x1<=4; x1++) {
		    * Count neighbors *
		    adder=0;
		    for (y2=y1-1; y2<=y1+1; y2++) {
			for (x2=x1-1; x2<=x1+1; x2++) {
			    if ((area >> (y2*6 + x2)) & 0x1) adder++;
			}
		    }
		    if ((area >> (y1*6 + x1)) & 0x1) adder--; 
		    switch (adder) {
			case 3:
			    result |= 0x1 << ((y1-1)*4 + x1-1);
			    break;
			case 2:
			    if ((area >> (y1*6 + x1)) & 0x1) {
				result |= 0x1 << ((y1-1)*4 + x1-1);
			    } 
			    break;
			default:
			    break;
		    }
		}
	    }
	    board[x][y] = result;
*/
	    board[x][y] = calc_block(area);
	}
	
    }
}


main() {
    unsigned int i,j;
    char * lookup;
    char tmp;

    printf("About to populate board\n");
    TIMER_CLEAR;
    TIMER_START;
    populate_board();
    TIMER_STOP;

    printf("Populate TIME: %lf seconds\n",TIMER_ELAPSED);

    print_board_rect(board, 30, 30, 15, 10);
    printf("\n");
 
    printf("About to process board\n");
    TIMER_CLEAR;
    TIMER_START;
    for (i=0; i<10; i++) {
	mate();
	print_board_rect(board, 30, 30, 2, 2);
	printf("\n");
    }
    TIMER_STOP;
    printf("Finished processing board\n");

    print_board_rect(board, 30, 30, 15, 10);
    printf("\n");

    printf("Generate TIME: %lf seconds\n",TIMER_ELAPSED);
}
