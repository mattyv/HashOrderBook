# HashOrderBook (Concept / Work in progress)
Here this project looks to directly tackle some of the data challenges of the financial markets 'order book' directly. Rather than by using existing associative containers from C++ standard template library, we look to create something bespoke which fulfils the needs of the concept directly to avoid any pitfalls or generalized implementation.

### What are the needs & challenges
 1 - Order retrieval. Other data structures (e.g. std::map) support this but as well see there are penalties.
 2 - Fast look up times. Ideally O(1) lookup. std::unordered_map supports this but lacks ordered retrieval. 
 2 - Fast insertion and deletion. Again ideally O(1) complexity. 
 2 - Cache friendliness. Plenty of structure fulfils this need. In this project we aim to fulfil this need also.
 4 - Memory density. Ideally the data structure shouldn't need to be enormous to fulfil any of the above needs. 
     It should also be able to grow and shrink as data is added and removed



![Diagram](HashOrderBook.png)
