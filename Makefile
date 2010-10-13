horton: horton.c
	cc -o horton -O3 horton.c

debug: horton.c
	cc -o horton -g2 horton.c

profile: horton.c
	cc -o horton -g -pg horton.c

statistics/stats: statistics/stats.c
	cc -o statistics/stats -g2 statistics/stats.c
