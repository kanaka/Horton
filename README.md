## Horton: Optimized Conway's Game of Life Engine

Horton is a highly optimized engine for "playing" 
[John Horton Conway's Game of Life](http://en.wikipedia.org/wiki/Conway's_Game_of_Life).

I originally created Horton because I had an interest in studying how
genetic algorithms could be applied to cellular automata and Conway's
Life in particular. In order to do so I needed a very fast version of
the game. I never did get around to applying the genetic algorithm
ideas to Horton, but I did develop a fairly fast engine.


### Optimizations

#### Compact representation

The grid is stored with 1 bit for each cell. Each byte represents
a 4 x 2 cell area of the grid. A 4 x 2 area of the grid has
6 x 4 ancestors and can be represented by 24 bits of a 32 bit unsigned
integer.


#### Lookup table

Horton generates a lookup table that maps the 24-bit (6 x 4) parent
index to the 8-bit (4 x 2) child result. The lookup table is 16 MB
(2^24). The lookup table can be saved to disk and loaded
(using mmap) later to increase subsequent startup performance.


#### Optimized representation

Not every arangement of living and dead cells is possible in the game
(after the initial state). In addition, different arrangement have
a different probability of occuring. This means that some positions in
the lookup table are accessed more often than others.

With a 16 MB lookup table it turns out that cache efficiency has
a huge impact on performance. So the ordering of bits in the 8-bit
child and 24-bit ancestor structures has been chosen to result in the
most commonly accessed entries in the lookup table being closer
together.



### Performance

<table>
    <tr>
        <th>CPU</th>
        <th>1st generation</th>
        <th>30th generation</th>
    </tr> <tr>
        <td>3.0GHz Intel Core 2 Duo</td>
        <td>1.68s</td>
        <td>0.14s</td>
    </tr> <tr>
        <td>1.60GHz Intel Atom 330</td>
        <td>1.73s</td>
        <td>0.90s</td>
    </tr>
</table>

The table shows the time to calculate the first generation versus the
30th generation of a 10,000 by 10,000 (100 million) cell grid that has
been randomly populated with 30% living cells. Note that the 30th
generation is far more relevant than the 1st because the initial grid
state is very different from what would exist on a grid that has been
running for a while. In other words, the 30th generation time is
closer to the average for a long running game.


### Future improvements

* Multithreading, multiprocessing: the problem is inherently parallel.

* Leverage SIMD type instructions such as SSE.

* Use GPGPU assist (i.e. NVIDIA's CUDA)
