//
//  Tests.hpp
//  HashOrderBook
//
//  Created by Matthew Varendorff on 18/5/2024.
//

#ifndef Tests_h
#define Tests_h

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
    std::cout << "Running tests..." << std::endl;
    
    // tick sizse of 1, fast book of 10, 3 collision buckets, max 3 overflow buckets
    using BookType = HashOrderBook<size_t, size_t, 1, 10, 3, 3>;
    BookType order_book(110l);
    size_t hash, collision_bucket;
    order_book.hash_key(110l, hash, collision_bucket);
    test(hash, 5ul, "hash_key failed", __LINE__);
    test(collision_bucket, 0ul, "hash_key failed", __LINE__);
    
    //should yield last index on zero'th collision bucket
    test(order_book.hash_key(114l, hash, collision_bucket),"hash_key failed", __LINE__);
    test(hash, 9ul, "hash_key failed", __LINE__);
    test(collision_bucket, 0ul, "hash_key failed", __LINE__);
    
    //should yield first index on first collision bucket
    test(order_book.hash_key(115l, hash, collision_bucket),"hash_key failed", __LINE__);
    test(hash, 0ul, "hash_key failed", __LINE__);
    test(collision_bucket, 1ul, "hash_key failed", __LINE__);
    
    //should yield last index on first collision bucket
    test(order_book.hash_key(124l, hash, collision_bucket),"hash_key failed", __LINE__);
    test(hash, 9ul, "hash_key failed", __LINE__);
    test(collision_bucket, 1ul, "hash_key failed", __LINE__);
    
    //should yield first index on second collision bucket
    test(order_book.hash_key(125l, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 0ul, "hash_key failed", __LINE__);
    test(collision_bucket, 2ul, "hash_key failed", __LINE__);
    
    //should yield last index on second collision bucket
    test(order_book.hash_key(134l, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 9ul, "hash_key failed", __LINE__);
    test(collision_bucket, 2ul, "hash_key failed", __LINE__);
    
    //should yield first index on third collision bucket and show that its now full and should use overflow
    test_failure(order_book.hash_key(135l, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 0ul, "hash_key failed", __LINE__);
    test(collision_bucket, 3ul, "hash_key failed", __LINE__);
    
    //test the keys lower than mid
    test(order_book.hash_key(105l, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 0ul, "hash_key failed", __LINE__);
    test(collision_bucket, 0ul, "hash_key failed", __LINE__);
    
    test(order_book.hash_key(104l, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 9ul, "hash_key failed", __LINE__);
    test(collision_bucket, 1ul, "hash_key failed", __LINE__);
    
    test(order_book.hash_key(9l, hash, collision_bucket), "hash_key failed", __LINE__);
    test(hash, 0ul, "hash_key failed", __LINE__);
    test(collision_bucket, 1ul, "hash_key failed", __LINE__);
    
    test(order_book.hash_key(94l, hash, collision_bucket), "hash_key failed", __LINE__);
    
    
    std::cout << "All tests passed" << std::endl;
}

#endif /* Tests_h */
