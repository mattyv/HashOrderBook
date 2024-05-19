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
        
        bid_ask_node() = default;
        bid_ask_node(const Key& key, const Value& value, Side side)
        : key(key)
        {
            if(side == Side::BID)
                bid_value = value;
            else
                ask_value = value;
        }
        bid_ask_node(const bid_ask_node& other) = default;
        ~bid_ask_node() = default;
    };
    

    template<size_t buckets>
    struct collision_bucket
    {
        bid_ask_node first_node;
        using overflow_bucket_type = std::unique_ptr<std::list<bid_ask_node>>;
        using bucket_type = std::unique_ptr<std::array<bid_ask_node, buckets>>;
        bucket_type nodes;
        overflow_bucket_type overflow_bucket;
        
        collision_bucket()
                : nodes(std::make_unique<std::array<bid_ask_node, buckets>>()),
                  overflow_bucket(std::make_unique<std::list<bid_ask_node>>()) {}
    };
    
    using collision_bucket_type = collision_bucket<collision_buckets>;
    using bucket_type = std::array<collision_bucket_type, fast_book_size>;
    bucket_type _buckets;
    
    Key _starting_mid_price;
    std::optional<Key> _best_bid, _best_offer;
    short _max_overflow_bucket_size;
    size_t _current_mid_index = fast_book_size / 2;
    size_t _size = 0;
private:
    constexpr size_t _positiveMod(long x, long mod) const
    {
        if (mod == 0) {
            throw std::invalid_argument("mod must be non-zero");  // Handle division by zero scenario
        }
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

    
    bid_ask_node* _find_node(const Key& key, typename collision_bucket_type::overflow_bucket_type& overflow_bucket) noexcept
    {
        for(auto& node : *overflow_bucket)
        {
            if(node.key == key)
            {
                return &node;
            }
        }
        return nullptr;
    }
    
    bool _erase_node(const Key& key, typename collision_bucket_type::overflow_bucket_type& overflow_bucket) noexcept
    {
        for(auto it = overflow_bucket->begin(); it != overflow_bucket->end(); ++it)
        {
            if(it->key == key)
            {
                overflow_bucket->erase(it);
                return true;
            }
        }
        return false;
    }
    
    void _update_bbo_and_mid(Side side, const Key& key)
    {
        bool bid_change = false, offer_change = false;
        if(side == Side::BID && key > _best_bid)
        {
            _best_bid = key;
            bid_change = true;
        }
        else if(side == Side::ASK && key < _best_offer)
        {
            _best_offer = key;
            offer_change = true;
        }
        
        if(_best_bid.has_value() && _best_offer.has_value() && (bid_change || offer_change))
        {
            const auto new_mid = (_best_bid.value() + _best_offer.value()) / 2;
            size_t hash, collision_bucket;
            hash_key(new_mid, hash, collision_bucket);
            if(collision_bucket > 0) //if it moves to far its a wrap around. not sure what to do yet. lets come back to this.
                throw std::runtime_error("Massive mid point move! Untested functionality!");
            _current_mid_index = hash;
        }
        else if(bid_change && _best_bid.has_value() )
        {
            size_t hash, collision_bucket;
            hash_key(_best_bid.value(), hash, collision_bucket);
            _current_mid_index = hash;
        }
        else if(offer_change && _best_offer.has_value())
        {
            size_t hash, collision_bucket;
            hash_key(_best_offer.value(), hash, collision_bucket);
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
    
    bool getBestBid(Key& key, Value& value)
    {
        auto k = _best_bid.value();
        decltype(value) valout;
        auto ok  = find(Side::BID, key, valout);
        if(!ok)
            return false;
        
        key = k;
        value = valout;
        return true;
    }
    
    const Value& getBestOffer(Key& key, Value& value)
    {
        auto k = _best_offer.value();
        decltype(value) valout;
        auto ok  = find(Side::ASK, key, valout);
        if(!ok)
            return false;
        
        key = k;
        value = valout;
        return true;
    }
    
    constexpr const Value& getMid() const noexcept
    {
        return _buckets[_current_mid_index].first_node.key;
    }
    
    bool insert(Side side, Key&& key, Value&& value)
    {
        size_t hash, collision_bucket;
        hash_key(key, hash, collision_bucket);
        auto& bucket = _buckets[hash];
        
        bid_ask_node* node = nullptr;
        if(collision_bucket >= collision_buckets)  //if we are using overflow buckets? i.e. collison bucket is larger than the hardcoded allowed
        {
            node = _find_node(key, bucket.overflow_bucket); //it might be in overflow buckets
            if(!node)
            {
               bucket.overflow_bucket->emplace_back(bid_ask_node(std::move(key), std::move(value), side));
                ++_size;
                return true;
            }
            else
                return false;
        }
        else
        {
            auto& nodes = *bucket.nodes;
            node = &nodes[collision_bucket];
        }
        
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
            ++_size;
            return true;
        }
    }
    
    bool find(Side side, const Key& key, Value& value)
    {
        size_t hash, collision_bucket;
        hash_key(key, hash, collision_bucket);
        auto& bucket = _buckets[hash];
        
        bid_ask_node* node = nullptr;
        if(collision_bucket >= collision_buckets)  //if we are using overflow buckets? i.e. collison bucket is larget than the hardcoded allowed
            node = _find_node(key, bucket.overflow_bucket); //it might be in overflow buckets
        else
        {
            auto& nodes = *bucket.nodes;
            node = &nodes[collision_bucket];
        }
        
        if(!node) //if we did find something in the collisin buckets. error
            return false;
        
        if(side == Side::BID && node->bid_value.has_value())
        {
            value = node->bid_value.value();
            return true;
        }
        else if(side == Side::ASK && node->ask_value.has_value())
        {
            value = node->ask_value.value();
            return true;
        }
        return false;
    }
    
    bool erase(Side side, const Key& key)
    {
        size_t hash, collision_bucket;
        hash_key(key, hash, collision_bucket);
        auto& bucket = _buckets[hash];
        
        bid_ask_node* node = nullptr;
        if(collision_bucket >= collision_buckets)  //if we are using overflow buckets? i.e. collison bucket is larget than the hardcoded allowed
        {
            return _erase_node(key, bucket.overflow_bucket); //it might be in overflow buckets
        }
        else
        {
            auto& nodes = *bucket.nodes;
            node = &nodes[collision_bucket];
        }
        
        if(!node) //if we did find something in the collisin buckets. error
            return false;
        
        if(side == Side::BID && node->bid_value.has_value())
        {
            node->bid_value.reset();
            --_size;
            return true;
        }
        else if(side == Side::ASK && node->ask_value.has_value())
        {
            node->ask_value.reset();
            --_size;
            return true;
        }
        return false;
        
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
