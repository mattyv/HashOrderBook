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

static void RunBenchmarks()
{
    std::cout << "Running benchmarks..." << std::endl;
    
    
    using Key = size_t;
    using Value = size_t;
    
    
    // tick sizse of 1, fast book of 10, 3 collision buckets, max 3 overflow buckets
    const size_t fast_book_size = 10, tick_size = 1, collision_buckets = 3, mid_price = 110;
    using BookType = HashOrderBook<size_t, size_t, tick_size, fast_book_size, collision_buckets>;
    BookType book(mid_price);
    
    std::random_device rd;  // Obtain a random number from hardware
    std::mt19937 gen(rd()); // Seed the generator

    // Define distributions
    std::normal_distribution<> centeredDist(110, 1.5); // Centered at 110 with small standard deviation
    std::uniform_int_distribution<> fullRangeDist(0, 200);
    
    std::vector<size_t> keys;
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
    
    for(auto key: keys)
    {
        std::cout << key << std::endl;
    }
    
    // Insert keys into the book
    std::map<size_t, size_t> book_map;
    auto startMap = std::chrono::high_resolution_clock::now();
    for(auto key: keys)
    {
        auto it  = book_map.lower_bound(key);
        book_map.emplace_hint(it, key, key);
    }
    auto endMap = std::chrono::high_resolution_clock::now();
    
    std::cout << "Map insert time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(endMap - startMap).count()<< "ns" << std::endl;
    
    
    auto book_start = std::chrono::high_resolution_clock::now();
    for(auto key: keys)
    {
        book.insert(BookType::Side::BID, std::move(key), std::move(key));
    }
    auto book_end = std::chrono::high_resolution_clock::now();
    
    std::cout << "Book insert time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_end - book_start).count() << "ns" << std::endl;
    
    // benchmark find
    auto startMapFind = std::chrono::high_resolution_clock::now();
    std::map<size_t, size_t>::iterator it;
    for(auto key: keys)
    {
        it = book_map.find(key);
    }
    auto endMapFind = std::chrono::high_resolution_clock::now();
    if(it == book_map.end())
    {
        std::cerr << "Benchmark failed" << std::endl;
    }
    
    std::cout << "Map find time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(endMapFind - startMapFind).count() << "ns" << std::endl;
    
    auto book_find_start = std::chrono::high_resolution_clock::now();
    size_t value = 0;
    bool ok = true;
    for(auto key: keys)
    {
        ok = book.find(BookType::Side::BID, key, value);
    }
    auto book_find_end = std::chrono::high_resolution_clock::now();
    if(!ok)
    {
        std::cerr << "Benchmark failed" << std::endl;
    }
    
    std::cout << "Book find time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_find_end - book_find_start).count()  << "ns" << std::endl;
    
    auto map_erase_start = std::chrono::high_resolution_clock::now();
    for(auto key: keys)
    {
        book_map.erase(key);
    }
    auto map_erase_end = std::chrono::high_resolution_clock::now();
    
    std::cout << "Map erase time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(map_erase_end - map_erase_start).count() << "ns" << std::endl;
    
    auto book_erase_start = std::chrono::high_resolution_clock::now();
    for(auto key: keys)
    {
        book.erase(BookType::Side::BID, key);
    }
    auto book_erase_end = std::chrono::high_resolution_clock::now();
    
    std::cout << "Book erase time: " << std::chrono::duration_cast<std::chrono::nanoseconds>(book_erase_end - book_erase_start).count() << "ns" << std::endl;
    
}

#endif /* Benchmark_h */
