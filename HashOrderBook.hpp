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
#include <forward_list>
#include <memory>


//concept for key to require == - /
template<typename KeyType>
concept KeyConcept = requires(KeyType a, KeyType b)
{
    { a == b } -> std::same_as<bool>;
    { a - b } -> std::convertible_to<KeyType>;
    { a / b } -> std::convertible_to<std::size_t>;
};


template<KeyConcept Key,
        class Value,
        Key tick_size, //small key value to show minimum price movement
        size_t fast_book_size, //fast book size is size of bid and ask depth combined
        size_t collision_buckets,
        bool auto_rehash = false> //rehash if the mid price moves out of the fast book size
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
    static constexpr size_t cache_line_size = 128;

private:
    struct bid_ask_node
    {
        std::optional<std::pair<Key, Value>> bid_value;
        std::optional<std::pair<Key, Value>> ask_value;
        
        bid_ask_node() = default;
        
        bid_ask_node(Key&& key, Value&& value, Side side)
        {
            if(side == Side::BID)
            {
                decltype(bid_value) new_node({std::move(key), std::move(value)});
                bid_value = std::move(new_node);
            }
            else
            {
                decltype(ask_value) new_node({std::move(key), std::move(value)});
                ask_value = std::move(new_node);
            }
        }
        bid_ask_node(const bid_ask_node& other) = default;
        ~bid_ask_node() = default;
        bid_ask_node& operator=(const bid_ask_node& other) = default;
        bid_ask_node& operator=(bid_ask_node&& other) noexcept= default;
    };
    
    struct bid_ask_collision_node : public bid_ask_node
    {
        size_t collision_index;
        
        bid_ask_collision_node() = delete;
        bid_ask_collision_node(Key&& key, Value&& value, Side side, size_t collision_index)
                : bid_ask_node(std::move(key), std::move(value), side), collision_index(collision_index) {}
        
        bid_ask_collision_node(const bid_ask_collision_node& other) = default;
        ~bid_ask_collision_node() = default;
    };
    
    
    using list_type = std::forward_list<bid_ask_collision_node>;
    
    template<size_t buckets>
    struct collision_bucket
    {
        bid_ask_node first_node;
        using overflow_bucket_type = std::unique_ptr<list_type>;
        using bucket_type = std::unique_ptr<std::array<bid_ask_node, buckets>>;
        bucket_type nodes;
        overflow_bucket_type overflow_bucket;
        
        
        static constexpr size_t size = sizeof(first_node) + sizeof(nodes) + sizeof(overflow_bucket);
    private:
        static constexpr size_t how_many_nodes_per_line = cache_line_size / size;
        static constexpr size_t remainder = cache_line_size - (size * how_many_nodes_per_line);
    public:
        static constexpr size_t padding_size = (size >= cache_line_size ?  0ul   : remainder / how_many_nodes_per_line);
        
        std::array<char, padding_size> padding; //padd out the structure so no items wrap over a cache when sitting in array.
        //this helps with random access. Without it we may need to fetch 2 cache lines instead of 1 if any of the
        //above members are on either size of the cache line divide.
        
        collision_bucket()
                : nodes(std::make_unique<std::array<bid_ask_node, buckets>>()),
                  overflow_bucket(std::make_unique<list_type>()) {}
        ~collision_bucket() = default;
        collision_bucket(const collision_bucket& other) = default;
    };
    
    using collision_bucket_type = collision_bucket<collision_buckets>;
    using bucket_type = std::array<collision_bucket_type, fast_book_size>;
    alignas(cache_line_size)bucket_type _buckets;
    
    Key _hashing_mid_price;
    size_t _current_mid_index = fast_book_size / 2, _size = 0;
    std::optional<Key> _best_bid, _best_offer;
private:
    constexpr size_t _positiveMod(long x, long mod) const
    {
        if (mod == 0)[[unlikely]] {
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
        if (size == 0)[[unlikely]] {
            throw std::invalid_argument("size must be non-zero");  // Handle division by zero scenario
        }

        // For positive indices or zero, perform the normal division
        if (index >= 0)
            return static_cast<size_t>(index / size);

        // For negative indices, adjust the bucket calculation
        return static_cast<size_t>(std::abs(index + 1) / size) + 1;
    }

    
    bid_ask_node* _find_node(Side side, const Key& key, typename collision_bucket_type::overflow_bucket_type& overflow_bucket) noexcept
    {
        for(auto& node : *overflow_bucket)
        {
            if(side == Side::BID && node.bid_value.has_value())
            {
                if(node.bid_value.value().first == key)
                    return &node;
            }
            else if(side == Side::ASK && node.ask_value.has_value())
            {
                if(node.ask_value.value().first == key)
                    return &node;
            }
        }
        return nullptr;
    }
    
    bool _erase_node(Side side, const Key& key, typename collision_bucket_type::overflow_bucket_type& overflow_bucket) noexcept
    {
        auto before = overflow_bucket->before_begin();
        for(auto it = overflow_bucket->begin(); it != overflow_bucket->end(); ++it)
        {
            bool found = false;
            if(side == Side::BID && it->bid_value.has_value() && it->bid_value.value().first == key)
            {
                it->bid_value.reset();
                found = true;
                --_size;
            }
            else if(side == Side::ASK && it->ask_value.has_value() && it->ask_value.value().first == key)
            {
                it->ask_value.reset();
                found = true;
                --_size;
            }
                
            if(!it->bid_value.has_value() && !it->ask_value.has_value())
                overflow_bucket->erase_after(before);
            
            if( found)
                return true;
            
            ++before;
        
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
            hash_key(side, new_mid, hash, collision_bucket);
            if(collision_bucket > 0) //if it moves to far its a wrap around. not sure what to do yet. lets come back to this.
                throw std::runtime_error("Massive mid point move! Untested functionality!");
            _current_mid_index = hash;
        }
        else if(bid_change && _best_bid.has_value() )
        {
            size_t hash, collision_bucket;
            hash_key(side, _best_bid.value(), hash, collision_bucket);
            _current_mid_index = hash;
        }
        else if(offer_change && _best_offer.has_value())
        {
            size_t hash, collision_bucket;
            hash_key(side, _best_offer.value(), hash, collision_bucket);
            _current_mid_index = hash;
        }
    }
    
    //calculates the hash based on an offset from the mid and the size of the array
    constexpr bool _hash_key(Side side, const Key& key, size_t& hash, size_t& collision_bucket, const size_t& hashing_mid_price) const
    {
        const long mid = fast_book_size / 2;
        const long offset_in_ticks = (key - hashing_mid_price) / tick_size; //can be -ve
        const long index = mid + offset_in_ticks; //can be -ve
        hash = _positiveMod(index, fast_book_size); //must always be +ve
        if((side == Side::BID && index > static_cast<long>(fast_book_size))
           ||
           (side == Side::ASK && index < 0))
        {
            collision_bucket = collision_buckets + 1; //if the price is wrapping too high or too low
            return false; //we use the collision buckets to store these as the nodes are reserved for lower bid or higher asks
        }
        collision_bucket = _calc_collision_bucket(index, fast_book_size); //also must be +ve
        return collision_bucket < collision_buckets;
    }
    
public:
    using value_type = bid_ask_node;
    
    HashOrderBook(const Key& hashing_mid_price)
    : _hashing_mid_price(hashing_mid_price)
    , _ask_End(this)
    , _cask_End(this)
    , _bid_End(this)
    , _cbid_End(this)
    {
        for(auto& bucket : _buckets)
        {
            bucket.nodes = std::make_unique<std::array<bid_ask_node, collision_buckets>>();
            bucket.overflow_bucket = std::make_unique<list_type>();
        }
    }
    ~HashOrderBook() = default;
    HashOrderBook(const HashOrderBook&) = delete;
    
    void rehash(const Key& hashing_mid_price)
    {
        bucket_type new_buckets;
        for(auto& bucket : new_buckets) //fill blank buckets
        {
            bucket.nodes = std::make_unique<std::array<bid_ask_node, collision_buckets>>();
            bucket.overflow_bucket = std::make_unique<list_type>();
        }
        _size = 0; //todo: size will update on _insert below. a little odd but ok for now.
        for(auto& bucket : _buckets) //extract each value from curret buckets and insert into new_buckets
        {
            //first node
            if(bucket.first_node.bid_value.has_value())
            {
                auto& key = bucket.first_node.bid_value.value().first;
                //std::cout << "Bid key: " << key << std::endl;
                auto& value = bucket.first_node.bid_value.value().second;
                if(!_insert(Side::BID, std::move(key), std::move(value), new_buckets, hashing_mid_price))
                    throw std::runtime_error("Failed to insert into new buckets");
            }
            if(bucket.first_node.ask_value.has_value())
            {
                auto& key = bucket.first_node.ask_value.value().first;
                //std::cout << "Ask key: " << key << std::endl;
                auto& value = bucket.first_node.ask_value.value().second;
                if(!_insert(Side::ASK, std::move(key), std::move(value), new_buckets, hashing_mid_price))
                    throw std::runtime_error("Failed to insert into new buckets");
            }
            
            //iterate over nodes
            for(auto& node: *bucket.nodes)
            {
                if(node.bid_value.has_value())
                {
                    auto& key = node.bid_value.value().first;
                    //std::cout << "Bid key: " << key << std::endl;
                    auto& value = node.bid_value.value().second;
                    if(!_insert(Side::BID, std::move(key), std::move(value), new_buckets, hashing_mid_price))
                        throw std::runtime_error("Failed to insert into new buckets");
                }
                if(node.ask_value.has_value())
                {
                    auto& key = node.ask_value.value().first;
                    //std::cout << "Ask key: " << key << std::endl;
                    auto& value = node.ask_value.value().second;
                    if(!_insert(Side::ASK, std::move(key), std::move(value), new_buckets, hashing_mid_price))
                        throw std::runtime_error("Failed to insert into new buckets");
                }
            }
            
            //overflow buckets
            for(auto& node: *bucket.overflow_bucket)
            {
                if(node.bid_value.has_value())
                {
                    auto& key = node.bid_value.value().first;
                    //std::cout << "Bid key: " << key << std::endl;
                    auto& value = node.bid_value.value().second;
                    if(!_insert(Side::BID, std::move(key), std::move(value), new_buckets, hashing_mid_price))
                        throw std::runtime_error("Failed to insert into new buckets");
                }
                if(node.ask_value.has_value())
                {
                    auto& key = node.ask_value.value().first;
                    //std::cout << "Ask key: " << key << std::endl;
                    auto& value = node.ask_value.value().second;
                    if(!_insert(Side::ASK, std::move(key), std::move(value), new_buckets, hashing_mid_price))
                        throw std::runtime_error("Failed to insert into new buckets");
                }
            }
        }
        
        //no copy assignment or move on std::array. move each individually
        for(size_t i = 0; i < _buckets.size(); ++i)
        {
            _buckets[i].first_node = std::move(new_buckets[i].first_node);
            _buckets[i].nodes = std::move(new_buckets[i].nodes);
            _buckets[i].overflow_bucket = std::move(new_buckets[i].overflow_bucket);
        }
        _hashing_mid_price = hashing_mid_price;
    }
    
    //calculates the hash based on an offset from the mid and the size of the array
    constexpr bool hash_key(Side side, const Key& key, size_t& hash, size_t& collision_bucket) const
    {
        return _hash_key(side, key, hash, collision_bucket, _hashing_mid_price);
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
    
private:
    bool _insert(Side side, Key&& key, Value&& value, bucket_type& buckets, const Key& hashing_mid_price)
    {
        size_t hash, collision_bucket; //collision bucket of 0 means we are looking in the "first node". Should give us better cache performance
        _hash_key(side, key, hash, collision_bucket, hashing_mid_price);
        auto& bucket = buckets[hash];
        
        bid_ask_node* node = nullptr;
          
        if(collision_bucket == 0) //we're looking in "first_node"
        {
            bool occupied = false;
            if(side == Side::BID)
                occupied = bucket.first_node.bid_value.has_value();
            else
                occupied = bucket.first_node.ask_value.has_value();
            
            if(occupied)
                return false;
            
            node = &bucket.first_node;
        }
        else if(collision_bucket -1 < collision_buckets)//were looknig in nodes.
        {
            auto& nodes = *bucket.nodes;
            const size_t collision_bucket_index = collision_bucket - 1;
            node = &nodes[collision_bucket_index];
        }
        else//if we are using overflow buckets? i.e. collison bucket is larger than the hardcoded allowed
        {
            node = _find_node(side, key, bucket.overflow_bucket); //it might be in overflow buckets
            if(!node)
            {
                bucket.overflow_bucket->emplace_front(bid_ask_collision_node(std::move(key), std::move(value), side, collision_bucket));
                ++_size;
                return true;
            }
            else if(side == Side::BID)
            {
                const bool has_value = node->bid_value.has_value();
                if(has_value)
                    return false;
                node->bid_value.value().first = std::move(key);
                node->bid_value.value().second = std::move(value);
            }
            else if(side == Side::ASK)
            {
                const bool has_value = node->ask_value.has_value();
                if(has_value)
                    return false;
                node->ask_value.value().first = std::move(key);
                node->ask_value.value().second = std::move(value);
            }
            else
                return false;
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
            decltype(node->bid_value) new_value({std::move(key), std::move(value)});
            *value_ptr = std::move(new_value);
            _update_bbo_and_mid(side, key);
            ++_size;
            return true;
        }
    }
    
public:
    bool insert(Side side, Key&& key, Value&& value)
    {
        return _insert(side, std::move(key), std::move(value), _buckets, _hashing_mid_price);
    }
    
    bool find(Side side, const Key& key, Value& value)
    {
        size_t hash, collision_bucket;
        hash_key(side, key, hash, collision_bucket);
        auto& bucket = _buckets[hash];
        
        bid_ask_node* node = nullptr;
        
        if(collision_bucket == 0) //we're looking in "first_node"
        {
            bool occupied = false;
            if(side == Side::BID)
                occupied = bucket.first_node.bid_value.has_value();
            else
                occupied = bucket.first_node.ask_value.has_value();
            
            if(!occupied)
                return false;
            
            node = &bucket.first_node;
        }
        else if(collision_bucket -1 < collision_buckets)//were looknig in nodes.
        {
            auto& nodes = *bucket.nodes;
            const size_t collision_bucket_index = collision_bucket - 1;
            node = &nodes[collision_bucket_index];
        }
        else//if we are using overflow buckets? i.e. collison bucket is larget than the hardcoded allowed
            node = _find_node(side, key, bucket.overflow_bucket); //it might be in overflow buckets
        
        if(!node) //if we did find something in the collisin buckets. error
            return false;
        
        if(side == Side::BID && node->bid_value.has_value())
        {
            if(key != node->bid_value.value().first)
                throw std::runtime_error("key mismatch");
            value = node->bid_value.value().second;
            return true;
        }
        else if(side == Side::ASK && node->ask_value.has_value())
        {
            if(key != node->ask_value.value().first)
                throw std::runtime_error("key mismatch");
            value = node->ask_value.value().second;
            return true;
        }
        else
            return false;
    }
    
    bool erase(Side side, const Key& key)
    {
        size_t hash, collision_bucket;
        hash_key(side, key, hash, collision_bucket);
        auto& bucket = _buckets[hash];
        
        bid_ask_node* node = nullptr; //unfortunately faster than using std::optinal<std::reference_wrapper<bid_ask_node>> and checking if it has value.
        
        if(collision_bucket == 0) //we're looking in "first_node"
        {
            bool occupied = false;
            if(side == Side::BID)
                occupied = bucket.first_node.bid_value.has_value();
            else
                occupied = bucket.first_node.ask_value.has_value();
            
            if(!occupied)
                return false;
            
            node = &bucket.first_node;
        }
        
        else if(collision_bucket -1 < collision_buckets)//were looknig in nodes.
        {
            auto& nodes = *bucket.nodes;
            const size_t collision_bucket_index = collision_bucket - 1;
            node = &nodes[collision_bucket_index];
        }
        else//if we are using overflow buckets? i.e. collison bucket is larget than the hardcoded allowed
        {
            return _erase_node(side, key, bucket.overflow_bucket); //it might be in overflow buckets
        }
        
        if(!node) //if we did find something in the collisin buckets. error
            return false;
        
        if(side == Side::BID && node->bid_value.has_value())
        {
            if(key != node->bid_value.value().first)
                throw std::runtime_error("key mismatch");
            
            node->bid_value.reset();
            --_size;
            return true;
        }
        else if(side == Side::ASK && node->ask_value.has_value())
        {
            if(key != node->ask_value.value().first)
                throw std::runtime_error("key mismatch");
            
            node->ask_value.reset();
            --_size;
            return true;
        }
        return false;
        
    }
    
    constexpr size_t size() const noexcept
    {
        return _size;
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
    
    void clear()
    {
        for(auto& bucket : _buckets)
        {
            bucket.first_node.bid_value.reset();
            bucket.first_node.ask_value.reset();
            if(bucket.nodes)
            {
                for(auto& node : *bucket.nodes)
                {
                    node.bid_value.reset();
                    node.ask_value.reset();
                }
            }
            if(bucket.overflow_bucket)
            {
                bucket.overflow_bucket->clear();
            }
        }
        _size = 0;
        _best_bid.reset();
        _best_offer.reset();
    }
    
    void clear(const Key& new_mid_price)
    {
        clear();
        _hashing_mid_price = new_mid_price;
    }
    
    friend void RunTests();
    
private:
    

    //enum class IteratorDirection { FORWARD, REVERSE}; sticking to foward iterators for now
    enum class IteratorConstness { CONST, NON_CONST};
    template<Side side, /*IteratorDirection direction, */IteratorConstness constness = IteratorConstness::NON_CONST>
    class Xiterator
    {
    private:
        using book_pointer = std::conditional_t<constness == IteratorConstness::CONST, const HashOrderBook*, HashOrderBook*>;
        using value_type_pointer = std::conditional_t<constness == IteratorConstness::CONST, const value_type*, value_type*>;
        using value_type_reference = std::conditional_t<constness == IteratorConstness::CONST, const value_type&, value_type&>;
        using value = std::conditional_t<constness == IteratorConstness::CONST, const Xiterator, Xiterator>;
        using pointer = std::conditional_t<constness == IteratorConstness::CONST, const Xiterator*, Xiterator*>;
        using reference = std::conditional_t<constness == IteratorConstness::CONST, const Xiterator&, Xiterator&>;
        
        size_t _index = 0, _collision_bucket = 0;
        book_pointer _book = nullptr;
        Side _side = side;
        //IteratorDirection _direction = direction;
        bool _isEnd = true;
        
        Xiterator(size_t index, size_t collision_bucket, book_pointer book, bool isEnd = false)
        : _index(index),
          _collision_bucket(collision_bucket),
          _book(book),
          _isEnd(isEnd)
        {
            while(!_has_price() && _has_next())
                this->operator++();
            if(!_has_price() && !_has_next())
                _isEnd = true;
        }
        
        Xiterator(book_pointer book)
        : _book(book),
          _isEnd(true)
        {
        }
        
    public:
        Xiterator()  = default;
        // Default copy constructor - used for same type
        Xiterator(const Xiterator& other) noexcept = default;
        
        Xiterator(Xiterator&& other) noexcept
        {
            _index = std::move(other._index);
            _collision_bucket = std::move(other._collision_bucket);
            _book = std::move(other._book);
            //_direction = std::move(other._direction);
            _side = std::move(other._side);
            _isEnd = std::move(other._isEnd);
        }

        // Default copy assignment operator - used for same type
        Xiterator& operator=(const Xiterator& other) noexcept = default;

        // Prevent cross-direction copying and assignment using a deleted function template
        template<Side otherSide>
        Xiterator(const Xiterator<otherSide>&) = delete;
       
        template<Side otherSide>
        Xiterator& operator=(const Xiterator<otherSide>&) = delete;
        
        //converter functions to convert form forward to reverse and vice versa
        //template<IteratorDirection OtherDirection>
        /*auto get_other_direction() const  noexcept {
            if constexpr (direction == IteratorDirection::FORWARD) {
                return Xiterator<Side, IteratorDirection::REVERSE, constness>(_index, _collision_bucket, _book);
            } else {
                return Xiterator<Side, IteratorDirection::FORWARD, constness>(_index, _collision_bucket, _book);
            }
        }*/
        
        auto get_other_side() const noexcept{
            if constexpr (side == Side::BID) {
                return Xiterator<Side::ASK, /*direction,*/ constness>(_index, _collision_bucket, _book);
            } else {
                return Xiterator<Side::BID, /*direction,*/ constness>(_index, _collision_bucket, _book);
            }
        }
                    
    private:
        
        size_t _get_max_collision_bucket(const collision_bucket<collision_buckets>& bucket) const
        {
            //given the current index what is the max collision bucket in the overflow buckets
            //const auto& bucket = _book->_buckets[_index].overflow_bucket;
            
            auto it = std::max_element(bucket.overflow_bucket->begin(), bucket.overflow_bucket->end(), [](const auto& a, const auto& b)
            {
                return a.collision_index < b.collision_index;
            });
            if(it != bucket.overflow_bucket->end())
               return it->collision_index;
            return 0;
        }
        
        bool _has_next_overflow_bucket() const //this might be too slow :( but is it worse than possibly adding more overhead on the insert /erase to track say, worst price or max price?
        {
            //for each overflow bucket starting from the current index if the max collison bucket is larger than the current collion index there must be more.
            //if we are at the end of the overflow buckets then there are no more
            if constexpr (side == Side::ASK)
            {
                for(auto it = _book->buckets.begin() + _index; it != _book->buckets.end(); ++it)
                {
                    if(_get_max_collision_bucket(*it) >= _collision_bucket)
                        return true;
                }
                return false;
            }
            else
            {
                for(auto it = _book->_buckets.rbegin() + (_index + fast_book_size); it != _book->_buckets.rend(); ++it)
                {
                    if(_get_max_collision_bucket(*it) >= _collision_bucket)
                        return true;
                }
                return false;
            }
        }
        
        bool _has_next() const
        {
            if(!_book) [[unlikely]]
                return false;
            
            //if we're in fast map or collison bucket territory then there is a possiblity of a next time
            auto next_index = _index;
            auto next_collision_bucket = _collision_bucket;
            _next_index(next_index, next_collision_bucket);
            
            if(next_collision_bucket <= collision_buckets)
                return true;
            else //otherwise we have to find the max collision bucket in each overflow bucket
                return _has_next_overflow_bucket();
        }
        
        
        constexpr void _next_index(size_t& index, size_t& collision_bucket) const//bool increment)
        {
            if(!_book) [[unlikely]]
                return;
            if constexpr (side == Side::ASK)
            {
                const long next_index = static_cast<long>(index) + 1;
                index = _book->_positiveMod( next_index, fast_book_size);
                collision_bucket = _book->_calc_collision_bucket(next_index, fast_book_size);
            }
            else
            {
                const long next_index = static_cast<long>(index) - 1;
                index = _book->_positiveMod(next_index, fast_book_size);
                collision_bucket = _book->_calc_collision_bucket(next_index, fast_book_size);
            }
        }
        
        bool _has_price() const
        {
            if(_collision_bucket == 0)
            {
                const auto& fn = _book->_buckets[_index].first_node;
                return fn.bid_value.has_value() || fn.ask_value.has_value();
            }
            else if(_collision_bucket <= collision_buckets)
            {
                const auto collision_index = _collision_bucket - 1;
                const auto & nodes = *_book->_buckets[_index].nodes;
                const auto& cb = nodes[collision_index];
                return cb.bid_value.has_value() || cb.ask_value.has_value();
            }
            else //is overflow bucket
            {
                const auto& ob = *_book->_buckets[_index].overflow_bucket;
                
                for(const auto& node : ob)
                {
                    if(node.collision_index == _collision_bucket)
                        return node.bid_value.has_value() || node.ask_value.has_value();
                }
                return false;
            }
        }
        
        value_type* _get_value_type() const
        {
            if(_collision_bucket == 0)
            {
                return &_book->_buckets[_index].first_node;
            }
            else if(_collision_bucket <= collision_buckets)
            {
                const auto collision_index = _collision_bucket - 1;
                auto& nodes = *_book->_buckets[_index].nodes;
                return &nodes[collision_index];
            }
            else //is overflow bucket
            {
                auto& ob = *_book->_buckets[_index].overflow_bucket;
                
                for(auto& node : ob)
                {
                    if(node.collision_index == _collision_bucket)
                        return &node;
                }
                return nullptr;
            }
        }
    public:
        reference operator++()
        {
            if(_book == nullptr) [[unlikely]]
            {
                return *this;
            }
            
            while( _has_next())
            {
                _next_index(_index, _collision_bucket);
                if(_has_price())
                    return *this;
            }
           
            _isEnd = true;
            return *this;
        }
        /*reference operator--()
        {
            if(_book == nullptr) [[unlikely]]
            {
                return *this;
            }
            
            for(; _migh_have_next(); _next_index(true))
            {
                if(_has_price())
                    return *this;
            }
           
            _isEnd = true;
            return *this;
        }*/
        //pre-increment
        value operator++(int)
        {
            Xiterator tmp = *this;
            ++(*this);
            return tmp;
        }
        //pre-decrement
        /*value operator--(int)
        {
            Xiterator tmp = *this;
            --(*this);
            return tmp;
        }*/
        constexpr value_type_pointer operator->() const
        {
            return _get_value_type();
        }
        constexpr value_type_reference operator*() const
        {
            return *_get_value_type();
        }
        constexpr bool operator==(const Xiterator& rhs) const
        {
            if(_book != rhs._book)
                return false;
            if(_isEnd == rhs._isEnd)
                return true;
            return _index == rhs._index && _collision_bucket == rhs._collision_bucket;
        }
        constexpr bool operator!=(const Xiterator& rhs) const
        {
            if(_book != rhs._book)
                return true;
            if(_isEnd != rhs._isEnd)
                return true;
            return _index != rhs._index || _collision_bucket != rhs._collision_bucket;
        }
        friend class HashOrderBook;
    };
public:
    using ask_itertator = Xiterator<Side::ASK>;
    using const_ask_itertator = Xiterator<Side::ASK, IteratorConstness::NON_CONST>;
    using bid_itertator = Xiterator<Side::BID>;
    using const_bid_itertator = Xiterator<Side::BID, IteratorConstness::NON_CONST>;
private:
    const ask_itertator _ask_End;
    const const_ask_itertator _cask_End;
    const bid_itertator _bid_End;
    const const_bid_itertator _cbid_End;
public:
    
    const ask_itertator& ask_end()
    {
        return _ask_End;
    }
    
    const const_ask_itertator& ask_end() const
    {
        return _cask_End;
    }
    
    const bid_itertator& bid_end()
    {
        return _bid_End;
    }
    
    const const_bid_itertator& bid_end() const
    {
        return _cbid_End;
    }
    
    ask_itertator ask_begin()
    {
        if(_best_offer.has_value())
        {
            size_t hash, collision_bucket;
            hash_key(Side::ASK, _best_offer.value(), hash, collision_bucket);
            return ask_itertator(hash, collision_bucket, this);
        }
        else
            return ask_end();
    }
    
    const_ask_itertator ask_begin() const
    {
        if(_best_offer.has_value())
        {
            size_t hash, collision_bucket;
            hash_key(Side::ASK, _best_offer.value(), hash, collision_bucket);
            return const_ask_itertator(hash, collision_bucket, this);
        }
        else
            return ask_end();
    }
    
    bid_itertator bid_begin()
    {
        if(_best_bid.has_value())
        {
            size_t hash, collision_bucket;
            hash_key(Side::BID, _best_bid.value(), hash, collision_bucket);
            return bid_itertator( hash, collision_bucket, this);
        }
        else
            return bid_end();
    }
    
    const_bid_itertator bid_begin() const
    {
        if(_best_bid.has_value())
        {
            size_t hash, collision_bucket;
            hash_key(Side::BID, _best_bid.value(), hash, collision_bucket);
            return const_bid_itertator(hash, collision_bucket, this);
        }
        else
            return bid_end();
    }
};

#endif /* HashOrderBook_h */
