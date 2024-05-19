//
//  HashOrderBook.hpp
//  HashOrderBook
//
//  Created by Matthew Varendorff on 18/5/2024.
//

#ifndef HashOrderBook_h
#define HashOrderBook_h

#include <array>
#include <optional>
#include <list>
#include <memory>

//concept for key to require == - /
template<typename KeyType>
concept KeyConcept = requires(KeyType a, KeyType b)
{
    {a == b} -> std::same_as<bool>;
    {a - b} -> std::same_as<KeyType>;
    {a / b} -> std::same_as<size_t>;
};

template<KeyConcept Key,
        class Value,
        Key tick_size, //small key value to show minimum price movement
        size_t fast_book_size, //fast book size is size of bid and ask depth combined
        size_t collision_buckets>
class HashOrderBook
{
public:
    enum class Side
    {
        BID,
        ASK
    };
    
    static constexpr Key tick_size_val = tick_size;
    static constexpr size_t fast_book_size_val = fast_book_size;
    static constexpr size_t collision_buckets_val = collision_buckets;
private:
    struct bid_ask_node
    {
        Key key;
        std::optional<Value> bid_value;
        std::optional<Value> ask_value;
    };
    

    template<size_t buckets>
    struct collision_bucket
    {
        bid_ask_node first_node;
        using overflow_bucket_type = std::unique_ptr<std::list<bid_ask_node>>;
        using bucket_type = std::unique_ptr<std::array<bid_ask_node, buckets>>;
        bucket_type nodes;
        overflow_bucket_type overflow_bucket;
    };
    
    using overflow_bucket_type = collision_bucket<collision_buckets>;
    using bucket_type = std::array<overflow_bucket_type, fast_book_size>;
    bucket_type _buckets;
    
    Key _starting_mid_price;
    std::optional<Key> _best_bid, _best_offer;
    short _max_overflow_bucket_size;
    size_t _current_mid_index = fast_book_size / 2;
    
private:
    constexpr size_t _positiveMod(long x, long mod) const noexcept
    {
        long result = x % mod;
        if (result < 0) {
            result += mod;
        }
        return result;
    }
    
    constexpr size_t _calc_collision_bucket(long index, long size) const
    {
        if (size == 0) {
            throw std::invalid_argument("size must be non-zero");  // Handle division by zero scenario
        }

        // For positive indices or zero, perform the normal division
        if (index >= 0)
            return static_cast<size_t>(index / size);

        // For negative indices, adjust the bucket calculation
        return static_cast<size_t>(std::abs(index + 1) / size) + 1;
    }

    
    bid_ask_node* _find_node(const Key& key, typename overflow_bucket_type::overflow_bucket_type& overflow_bucket) const noexcept
    {
        for(auto& node : overflow_bucket)
        {
            if(node.key == key)
            {
                return &node;
            }
        }
        return nullptr;
    }
    
    void _update_bbo_and_mid(Side side, const Key& key)
    {
        if(side == Side::BID && key > _best_bid)
            _best_bid = key;
        else if(side == Side::ASK && key < _best_offer)
            _best_offer = key;
        
        if(_best_bid.has_value() && _best_offer.has_value())
        {
            const auto new_mid = (_best_bid.value() + _best_offer.value()) / 2;
            size_t hash, collision_bucket;
            hash_key(new_mid, hash, collision_bucket);
            if(collision_bucket > 0) //if it moves to far its a wrap around. not sure what to do yet. lets come back to this.
                throw std::runtime_error("Massive mid point move! Untested functionality!");
            _current_mid_index = hash;
        }
    }
    
public:
    HashOrderBook(const Key& starting_mid_price)
    : _starting_mid_price(starting_mid_price)
    {
        for(auto& bucket : _buckets)
        {
            bucket.nodes = std::make_unique<std::array<bid_ask_node, collision_buckets>>();
            bucket.overflow_bucket = std::make_unique<std::list<bid_ask_node>>();
        }
    }
    ~HashOrderBook() = default;
    HashOrderBook(const HashOrderBook&) = delete;
    
    
    //calculates the hash based on an offset from the mid and the size of the array
    constexpr bool hash_key(const Key& key, size_t& hash, size_t& collision_bucket) const
    {
        const long mid = fast_book_size / 2;
        const long offset_in_ticks = (key - _starting_mid_price) / tick_size; //can be -ve
        const long index = mid + offset_in_ticks; //can be -ve
        hash = _positiveMod(index, fast_book_size); //must always be +ve
        collision_bucket = _calc_collision_bucket(index, fast_book_size); //also must be +ve
        return collision_bucket < collision_buckets;
    }
    
    bool insert(Side side, Key&& key, Value&& value)
    {
        size_t hash, collision_bucket;
        hash_key(key, hash, collision_bucket);
        auto& bucket = _buckets[hash];
        
        bid_ask_node* node = nullptr;
        if(collision_bucket > collision_buckets)  //if we are using overflow buckets? i.e. collison bucket is larget than the hardcoded allowed
            node = _find_node(key, bucket.overflow_bucket); //it might be in overflow buckets
        else
            node = &bucket.nodes[collision_bucket];
        
        if(!node) //if we did find something in the collisin buckets. error
            return false;
        
        decltype(node->bid_value)* value_ptr = nullptr;
        if(side == Side::BID)
            value_ptr = &node->bid_value;
        else
            value_ptr = &node->ask_value;
        
        if(value_ptr->has_value()) //we already have a value! so is error
            return false;
        else
        {
            *value_ptr = std::move(value);
            _update_bbo_and_mid(side, key);
            return true;
        }
    }
    
    size_t getByteSize() const
    {
        size_t size = 0;
        for(auto& bucket : _buckets)
        {
            size += sizeof(bucket.first_node);
            size += sizeof(bucket.nodes);
            size += sizeof(bucket.overflow_bucket);
            for(auto& node : *bucket.nodes)
            {
                size += sizeof(node);
            }
            for(auto& node : *bucket.overflow_bucket)
            {
                size += sizeof(node);
            }
        }
        return size;
    }
    
    friend void RunTests();
};

#endif /* HashOrderBook_h */
