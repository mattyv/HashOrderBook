//
//  Benchmark.hpp
//  HashOrderBook
//
//  Created by Matthew Varendorff on 19/5/2024.
//

#ifndef Benchmark_h
#define Benchmark_h

#include "HashOrderBook.hpp"
#include <map>
#include <random>
#include <cmath>
#include <queue>
#include <unordered_map>


static void RunBenchmarks()
{
    std::cout << "Running benchmarks..." << std::endl;
    
    
    using Key = size_t;
    using Value = size_t;
    
    
    // tick sizse of 1, fast book of 10, 3 collision buckets, max 3 overflow buckets
    const size_t fast_book_size = 10, tick_size = 1, collision_buckets = 3, mid_price = 110;
    using BookType = HashOrderBook<Key, Key, tick_size, fast_book_size, collision_buckets>;
    BookType book(mid_price);
    
    std::random_device rd;  // Obtain a random number from hardware
    std::mt19937 gen(rd()); // Seed the generator

    // Define distributions
    std::normal_distribution<> centeredDist(110, 1.5); // Centered at 110 with small standard deviation
    std::uniform_int_distribution<> fullRangeDist(0, 200);
    
    std::vector<Key> keys;
    constexpr int NUM_KEYS = 200; // Total number of keys to generate

    for (int i = 0; i < NUM_KEYS; i++) {
        double probability = (double)rand() / RAND_MAX; // Generate probability for distribution choice

        int key;
        if (probability < 0.9) {
            // Generate key from normal distribution, round and clamp
            key = std::round(centeredDist(gen));
            if (key < 105) key = 105;
            if (key > 115) key = 115;
        } else {
            // Generate key from uniform distribution
            key = fullRangeDist(gen);
        }

        // Add the key to the vector
        keys.push_back(key);
    }
    
    
    //print keys
    /*for(auto key: keys)
    {
        std::cout << key << std::endl;
    }*/
    
    // Insert keys into the book
    std::map<Key, Value> book_map;
    auto startMap = std::chrono::high_resolution_clock::now();
    for(auto key: keys)
    {
        auto it  = book_map.lower_bound(key);
        book_map.emplace_hint(it, key, key);
    }
    auto endMap = std::chrono::high_resolution_clock::now();
    
    std::cout << "Map insert time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(endMap - startMap).count() / keys.size() << "ns" << std::endl;
    
    
    auto book_start = std::chrono::high_resolution_clock::now();
    for(auto key: keys)
    {
        book.insert(BookType::Side::BID, std::move(key), std::move(key));
    }
    auto book_end = std::chrono::high_resolution_clock::now();
    
    std::cout << "Book insert time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_end - book_start).count() / keys.size() << "ns" << std::endl;
    
    // benchmark find
    auto startMapFind = std::chrono::high_resolution_clock::now();
    std::map<Key, Value>::iterator it;
    for(auto key: keys)
    {
        it = book_map.find(key);
    }
    auto endMapFind = std::chrono::high_resolution_clock::now();
    if(it == book_map.end())//just to make sure it doesn't optimise the find() away
    {
        std::cerr << "Benchmark failed" << std::endl;
    }
    
    std::cout << "Map find time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(endMapFind - startMapFind).count() / keys.size() << "ns" << std::endl;
    
    auto book_find_start = std::chrono::high_resolution_clock::now();
    size_t value = 0;
    bool ok = true;
    for(auto key: keys)
    {
        ok = book.find(BookType::Side::BID, key, value);
    }
    auto book_find_end = std::chrono::high_resolution_clock::now();
    if(!ok) //just to make sure it doesn't optimise the find() away
    {
        std::cerr << "Benchmark failed" << std::endl;
    }
    
    std::cout << "Book find time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_find_end - book_find_start).count() / keys.size()  << "ns" << std::endl;
    
    auto map_erase_start = std::chrono::high_resolution_clock::now();
    for(auto key: keys)
    {
        book_map.erase(key);
    }
    auto map_erase_end = std::chrono::high_resolution_clock::now();
    
    std::cout << "Map erase time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(map_erase_end - map_erase_start).count() / keys.size() << "ns" << std::endl;
    
    auto book_erase_start = std::chrono::high_resolution_clock::now();
    for(auto key: keys)
    {
        book.erase(BookType::Side::BID, key);
    }
    auto book_erase_end = std::chrono::high_resolution_clock::now();
    
    std::cout << "Book erase time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_erase_end - book_erase_start).count()/ keys.size()  << "ns" << std::endl;
    
    //generate random keys in range 105 - 114
    std::vector<Key> random_keys;
    
    std::uniform_int_distribution<> randomDist(105, 114);
    for (int i = 0; i < NUM_KEYS; i++) {
        random_keys.push_back(randomDist(gen));
    }
    
    std::cout << "keys into fast book only..." << std::endl;
    
    // benchmark insert keys into map
    auto startMapRandom = std::chrono::high_resolution_clock::now();
    for(Key key = 105; key < 115; ++key)
    {
        book_map.emplace( key, key);
    }
    auto endMapRandom = std::chrono::high_resolution_clock::now();
    
    std::cout << "Map insert time for top of book: " << std::chrono::duration_cast<std::chrono::nanoseconds>(endMapRandom - startMapRandom).count() / 10<< "ns" << std::endl;
    
    auto book_start_random = std::chrono::high_resolution_clock::now();
    for(Key key = 105; key < 115; ++key)
    {
        book.insert(BookType::Side::BID, std::move(key), std::move(key));
    }
    auto book_end_random = std::chrono::high_resolution_clock::now();
    
    std::cout << "Book insert time for top of book: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_end_random - book_start_random).count() / 10<< "ns" << std::endl;
    
    // benchmark find random keys
    auto startMapFindRandom = std::chrono::high_resolution_clock::now();
    for(auto key: random_keys)
    {
        it = book_map.find(key);
    }
    auto endMapFindRandom = std::chrono::high_resolution_clock::now();
    if(it == book_map.end())
    {
        std::cerr << "Benchmark failed" << std::endl;
    }
    
    std::cout << "Map find random time for top of book: " << std::chrono::duration_cast<std::chrono::nanoseconds>(endMapFindRandom - startMapFindRandom).count() / keys.size() << "ns" << std::endl;
    
    auto book_find_start_random = std::chrono::high_resolution_clock::now();
    for(auto key: random_keys)
    {
        ok = book.find(BookType::Side::BID, key, value);
    }
    auto book_find_end_random = std::chrono::high_resolution_clock::now();
    if(!ok)
    {
        std::cerr << "Benchmark failed" << std::endl;
    }
    
    std::cout << "Book find time for top of book: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_find_end_random - book_find_start_random).count() / keys.size()  << "ns" << std::endl;
    
    auto map_erase_start_random = std::chrono::high_resolution_clock::now();
    for(Key key = 105; key < 115; ++key)
    {
        book_map.erase(key);
    }
    auto map_erase_end_random = std::chrono::high_resolution_clock::now();
    
    std::cout << "Map erase time for top of book: " << std::chrono::duration_cast<std::chrono::nanoseconds>(map_erase_end_random - map_erase_start_random).count() /10 << "ns" << std::endl;
    
    auto book_erase_start_random = std::chrono::high_resolution_clock::now();
    for(Key key = 105; key < 115; ++key)
    {
        book.erase(BookType::Side::BID, key);
    }

    auto book_erase_end_random = std::chrono::high_resolution_clock::now();
    
    std::cout << "Book erase time for top of book: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_erase_end_random - book_erase_start_random).count() /10<< "ns" << std::endl;
    
    //generate random keys between 95 - 104
    random_keys.clear();
    std::uniform_int_distribution<> randomDist2(95, 104);
    for (int i = 0; i < NUM_KEYS; i++) {
        random_keys.push_back(randomDist2(gen));
    }
    
    std::cout << std::endl << "keys into collision buckets only..." << std::endl;
    
    // benchmark insert random keys into map
    auto startMapRandom2 = std::chrono::high_resolution_clock::now();
    for(Key key = 95; key < 105; ++key)
    {
        book_map.emplace(key, key);
    }
    auto endMapRandom2 = std::chrono::high_resolution_clock::now();
    
    std::cout << "Map time for bottom of book: " << std::chrono::duration_cast<std::chrono::nanoseconds>(endMapRandom2 - startMapRandom2).count() / 10 << "ns" << std::endl;
    
    auto book_start_random2 = std::chrono::high_resolution_clock::now();
    for(Key key = 95; key < 105; ++key)
    {
        book.insert(BookType::Side::BID, std::move(key), std::move(key));
    }
    auto book_end_random2 = std::chrono::high_resolution_clock::now();
    
    std::cout << "Book insert time for bottom of book: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_end_random2 - book_start_random2).count() /10<< "ns" << std::endl;
    
    // benchmark find random keys
    auto startMapFindRandom2 = std::chrono::high_resolution_clock::now();
    for(auto key: random_keys)
    {
        it = book_map.find(key);
    }
    auto endMapFindRandom2 = std::chrono::high_resolution_clock::now();
    if(it == book_map.end())
    {
        std::cerr << "Benchmark failed" << std::endl;
    }
    
    std::cout << "Map find random time for bottom of book: " << std::chrono::duration_cast<std::chrono::nanoseconds>(endMapFindRandom2 - startMapFindRandom2).count() / keys.size() << "ns" << std::endl;
    
    auto book_find_start_random2 = std::chrono::high_resolution_clock::now();
    for(auto key: random_keys)
    {
        ok = book.find(BookType::Side::BID, key, value);
    }
    auto book_find_end_random2 = std::chrono::high_resolution_clock::now();
    if(!ok)
    {
        std::cerr << "Benchmark failed" << std::endl;
    }
    
    std::cout << "Book find random time for bottom of book: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_find_end_random2 - book_find_start_random2).count() / keys.size()  << "ns" << std::endl;
    
    auto map_erase_start_random2 = std::chrono::high_resolution_clock::now();
    for(Key key = 95; key < 105; ++key)
    {
        book_map.erase(key);
    }
    auto map_erase_end_random2 = std::chrono::high_resolution_clock::now();
    
    std::cout << "Map erase time for bottom of book: " << std::chrono::duration_cast<std::chrono::nanoseconds>(map_erase_end_random2 - map_erase_start_random2).count() /10 << "ns" << std::endl;
    
    auto book_erase_start_random2 = std::chrono::high_resolution_clock::now();
    for(Key key = 95; key < 105; ++key)
    {
        book.erase(BookType::Side::BID, key);
    }
    auto book_erase_end_random2 = std::chrono::high_resolution_clock::now();
    
    std::cout << "Book erase time for bottom of book: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_erase_end_random2 - book_erase_start_random2).count() /10<< "ns" << std::endl;
    
    //generate random keys between 115 - 124
    random_keys.clear();
    std::uniform_int_distribution<> randomDist3(115, 124);
    for (int i = 0; i < NUM_KEYS; i++) {
        random_keys.push_back(randomDist3(gen));
    }
    
    std::cout << std::endl << "keys into overflow buckets on the high side only..." << std::endl;
    
    // benchmark insert random keys into map
    auto startMapRandom3 = std::chrono::high_resolution_clock::now();
    for(Key key = 115; key < 125; ++key)
    {
        book_map.emplace(key, key);
    }
    auto endMapRandom3 = std::chrono::high_resolution_clock::now();
    
    std::cout << "Map insert time for overflow buckets: " << std::chrono::duration_cast<std::chrono::nanoseconds>(endMapRandom3 - startMapRandom3).count()/ 10 << "ns" << std::endl;
    
    auto book_start_random3 = std::chrono::high_resolution_clock::now();
    for(Key key = 115; key < 125; ++key)
    {
        book.insert(BookType::Side::BID, std::move(key), std::move(key));
    }
    auto book_end_random3 = std::chrono::high_resolution_clock::now();
    
    std::cout << "Book insert time for overflow buckets: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_end_random3 - book_start_random3).count() / 10  << "ns" << std::endl;
    
    // benchmark find random keys
    auto startMapFindRandom3 = std::chrono::high_resolution_clock::now();
    for(auto key: random_keys)
    {
        it = book_map.find(key);
    }
    auto endMapFindRandom3 = std::chrono::high_resolution_clock::now();
    if(it == book_map.end())
    {
        std::cerr << "Benchmark failed" << std::endl;
    }
    
    std::cout << "Map find random time for overflow buckets: " << std::chrono::duration_cast<std::chrono::nanoseconds>(endMapFindRandom3 - startMapFindRandom3).count()/ keys.size()  << "ns" << std::endl;
    
    auto book_find_start_random3 = std::chrono::high_resolution_clock::now();
    
    for(auto key: random_keys)
    {
        ok = book.find(BookType::Side::BID, key, value);
    }
    
    auto book_find_end_random3 = std::chrono::high_resolution_clock::now();
    if(!ok)
    {
        std::cerr << "Benchmark failed" << std::endl;
    }
    
    std::cout << "Book find random time for overflow buckets: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_find_end_random3 - book_find_start_random3).count() / keys.size()  << "ns" << std::endl;
    
    auto map_erase_start_random3 = std::chrono::high_resolution_clock::now();
    for(Key key = 115; key < 125; ++key)
    {
        book_map.erase(key);
    }
    auto map_erase_end_random3 = std::chrono::high_resolution_clock::now();
    
    std::cout << "Map erase time for overflow buckets: " << std::chrono::duration_cast<std::chrono::nanoseconds>(map_erase_end_random3 - map_erase_start_random3).count() /10<< "ns" << std::endl;
    
    auto book_erase_start_random3 = std::chrono::high_resolution_clock::now();
    
    for(Key key = 115; key < 125; ++key)
    {
        book.erase(BookType::Side::BID, key);
    }
    
    auto book_erase_end_random3 = std::chrono::high_resolution_clock::now();
    
    std::cout << "Book erase time for overflow buckets: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_erase_end_random3 - book_erase_start_random3).count() /10<< "ns" << std::endl;
}

#endif /* Benchmark_h */
