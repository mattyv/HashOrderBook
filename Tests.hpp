//
//  Tests.hpp
//  HashOrderBook
//
//  Created by Matthew Varendorff on 18/5/2024.
//

#ifndef Tests_h
#define Tests_h

#include <fstream>
#include <sstream>
#include <tuple>

#ifdef __APPLE__
#include <sys/sysctl.h>
static size_t getCacheLineSize() {
    size_t lineSize = 0;
    size_t sizeOfLineSize = sizeof(lineSize);
    sysctlbyname("hw.cachelinesize", &lineSize, &sizeOfLineSize, NULL, 0);
    return lineSize;
}
std::tuple<size_t, size_t, size_t, size_t> getCacheSizes() {
    size_t l1d = 0, l1i = 0, l2 = 0, l3 = 0;
    size_t sizeOfSize = sizeof(size_t);

    // Ensure each `sysctlbyname` call correctly initializes the size variable before use.
    if (sysctlbyname("hw.l1dcachesize", &l1d, &sizeOfSize, NULL, 0) != 0) {
        std::cerr << "Failed to get L1D cache size" << std::endl;
    }
    if (sysctlbyname("hw.l1icachesize", &l1i, &sizeOfSize, NULL, 0) != 0) {
        std::cerr << "Failed to get L1I cache size" << std::endl;
    }
    if (sysctlbyname("hw.l2cachesize", &l2, &sizeOfSize, NULL, 0) != 0) {
        std::cerr << "Failed to get L2 cache size" << std::endl;
    }
    if (sysctlbyname("hw.l3cachesize", &l3, &sizeOfSize, NULL, 0) != 0) {
        std::cerr << "Failed to get L3 cache size" << std::endl;
    }

    return {l1d, l1i, l2, l3};
}


#elif __linux__
static size_t getCacheLineSize() {
    std::ifstream file("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size");
    if (file) {
        size_t lineSize = 0;
        file >> lineSize;
        return lineSize;
    } else {
        std::cerr << "Failed to open cache info file.\n";
        return 64; // default to 64 bytes
    }
}

std::tuple<size_t, size_t, size_t, size_t> getCacheSizes() {
    size_t l1d = 0, l1i = 0, l2 = 0, l3 = 0;
    std::string line, type;
    for (int index = 0; ; ++index) {
        std::ostringstream path;
        path << "/sys/devices/system/cpu/cpu0/cache/index" << index;
        std::ifstream sizeFile(path.str() + "/size");
        std::ifstream typeFile(path.str() + "/type");

        if (!sizeFile || !typeFile) break;  // No more caches at this index

        std::getline(sizeFile, line);
        std::getline(typeFile, type);
        size_t size = std::stoul(line.substr(0, line.size() - 1));
        if (line.back() == 'K') {
            size *= 1024;  // Convert KB to bytes
        }

        if (type == "Data") {
            l1d = size;
        } else if (type == "Instruction") {
            l1i = size;
        } else if (type == "Unified") {
            if (index == 1) {
                l2 = size;
            } else if (index == 2) {
                l3 = size;
            }
        }
    }
    return {l1d, l1i, l2, l3};
}
#else
#error "Unsupported platform"
#endif


#include "HashOrderBook.hpp"

template<typename T>
static void test(T a, T b, const char* message, int line)
{
    if(a != b)
    {
        std::cout << message << " " << a << " != " << b << " @ line: " << line << std::endl;
        exit(1);
    }
}

static void test(bool condition, const char* message, int line)
{
    if(!condition)
    {
        std::cout << message << " @ line: " << line << std::endl;
        exit(1);
    }
}

static void test_failure(bool condition, const char* message, int line)
{
   test(!condition, message, line);
}



static void RunTests()
{
    // tick sizse of 1, fast book of 10, 3 collision buckets, max 3 overflow buckets
    const size_t fast_book_size = 10, tick_size = 1, collision_buckets = 3, mid_price = 110;
    using BookType = HashOrderBook<size_t, size_t, tick_size, fast_book_size, collision_buckets>;
    BookType order_book(mid_price);
    std::cout << "What size are things?..." << std::endl;
    std::cout << "size of bid_ask_node: " << sizeof(BookType::bid_ask_node) << std::endl;
    std::cout << "size of collision_bucket: " << sizeof(BookType::collision_bucket<3>) << std::endl;
    std::cout << "size of overflow_bucket_type: " << sizeof(BookType::overflow_bucket_type) << std::endl;
    std::cout << "size of bucket_type: " << sizeof(BookType::bucket_type) << std::endl;
    std::cout << "Size of static order_book: " << sizeof(order_book) << " bytes. Or "
                << sizeof(order_book) / getCacheLineSize() << " cache lines." << std::endl;
    std::cout << "Total order_book size: " << order_book.getByteSize() << " bytes. Or "
                << order_book.getByteSize() / getCacheLineSize() << " cache lines " << std::endl;
    //auto cacheSizes = getCacheSizes();
    //std::cout << "Cache sizes: L1 - " << std::get<0>(cacheSizes) << ", L2 - " << std::get<2>(cacheSizes) << ", L3 - " << std::get<3>(cacheSizes) << std::endl;
    
    std::cout << std::endl << "Running tests..." << std::endl;
    
    test(BookType::tick_size_val, tick_size, "tick_size_val failed", __LINE__);
    test(BookType::fast_book_size_val, fast_book_size, "fast_book_size_val failed", __LINE__);
    test(BookType::collision_buckets_val, collision_buckets, "collision_buckets_val failed", __LINE__);
    
    size_t hash, collision_bucket;
    order_book.hash_key(mid_price, hash, collision_bucket);
    test(hash, 5ul, "hash_key failed", __LINE__);
    test(collision_bucket, 0ul, "hash_key failed", __LINE__);
    
    //should yield last index on zero'th collision bucket
    size_t price = mid_price + ((fast_book_size / 2) * tick_size) -1;
    test(order_book.hash_key(price, hash, collision_bucket),"hash_key failed", __LINE__);
    test(hash, 9ul, "hash_key failed", __LINE__);
    test(collision_bucket, 0ul, "hash_key failed", __LINE__);
    
    //should yield first index on first collision bucket;
    test(order_book.hash_key(price + 1, hash, collision_bucket),"hash_key failed", __LINE__);
    test(hash, 0ul, "hash_key failed", __LINE__);
    test(collision_bucket, 1ul, "hash_key failed", __LINE__);
    
    //should yield last index on first collision bucket
    price += fast_book_size * tick_size;
    test(order_book.hash_key(price, hash, collision_bucket),"hash_key failed", __LINE__);
    test(hash, 9ul, "hash_key failed", __LINE__);
    test(collision_bucket, 1ul, "hash_key failed", __LINE__);
    
    //should yield first index on second collision bucket
    test(order_book.hash_key(price +1, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 0ul, "hash_key failed", __LINE__);
    test(collision_bucket, 2ul, "hash_key failed", __LINE__);
    
    //should yield last index on second collision bucket
    price += fast_book_size * tick_size;
    test(order_book.hash_key(price, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 9ul, "hash_key failed", __LINE__);
    test(collision_bucket, 2ul, "hash_key failed", __LINE__);
    
    //should yield first index on third collision bucket and show that its now full and should use overflow
    test_failure(order_book.hash_key(price +1, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 0ul, "hash_key failed", __LINE__);
    test(collision_bucket, 3ul, "hash_key failed", __LINE__);
    
    //test the keys lower than mid
    price = mid_price - ((fast_book_size / 2) * tick_size);
    test(order_book.hash_key(price, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 0ul, "hash_key failed", __LINE__);
    test(collision_bucket, 0ul, "hash_key failed", __LINE__);
    
    test(order_book.hash_key(price -1, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 9ul, "hash_key failed", __LINE__);
    test(collision_bucket, 1ul, "hash_key failed", __LINE__);
    
    price -= fast_book_size * tick_size;
    test(order_book.hash_key(price, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 0ul, "hash_key failed", __LINE__);
    test(collision_bucket, 1ul, "hash_key failed", __LINE__);
    
    test(order_book.hash_key(price +1, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 1ul, "hash_key failed", __LINE__);
    test(collision_bucket, 1ul, "hash_key failed", __LINE__);
    
    test(order_book.hash_key(price -1, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 9ul, "hash_key failed", __LINE__);
    test(collision_bucket, 2ul, "hash_key failed", __LINE__);
    
    test(order_book.hash_key(price -2, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 8ul, "hash_key failed", __LINE__);
    test(collision_bucket, 2ul, "hash_key failed", __LINE__);
    
    price -= fast_book_size * tick_size;
    test(order_book.hash_key(price, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 0ul, "hash_key failed", __LINE__);
    test(collision_bucket, 2ul, "hash_key failed", __LINE__);
    
    test_failure(order_book.hash_key(price -1, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 9ul, "hash_key failed", __LINE__);
    test(collision_bucket, 3ul, "hash_key failed", __LINE__);
    
    test_failure(order_book.hash_key(price -1, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 9ul, "hash_key failed", __LINE__);
    test(collision_bucket, 3ul, "hash_key failed", __LINE__);
    
    std::cout << "All tests passed" << std::endl;
}

#endif /* Tests_h */