life: life.c
	cc -o life -O3 life.c

debug: life.c
	cc -o life -g2 life.c

profile: life.c
	cc -o life -g -pg life.c

statistics/stats: statistics/stats.c
	cc -o statistics/stats -g2 statistics/stats.c
