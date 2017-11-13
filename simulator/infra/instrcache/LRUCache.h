/**
 * A cache for decoded instructions in functional simulator
 * @author Denis Los
*/

#ifndef LRUCACHE_H
#define LRUCACHE_H

#include <cassert>

#include <list>
#include <unordered_map>
#include <utility>

template <typename Key, typename Value, size_t CAPACITY>
class LRUCache
{
    public:
        LRUCache() { data.reserve( CAPACITY); }

        static auto get_capacity() { return CAPACITY; }
        
        auto begin() const { return data.cbegin(); }
        auto end() const { return data.cend(); }
        auto find( const Key& key) const { return data.find( key); }
        
        std::size_t size() const { return number_of_elements; }
        bool empty() const { return number_of_elements == 0u; }

        void allocate( const Key& key, const Value& value) {
            if ( number_of_elements == CAPACITY)
            {
                // Delete least recently used element
                const auto& lru_elem = lru_list.back();
                lru_hash.erase( lru_elem);
                data.erase( lru_elem);
                lru_list.pop_back();
            }
            else {
                 number_of_elements++;
            }
            // Add a new element
            data.emplace( key, value);
            auto ptr = lru_list.insert( lru_list.begin(), key);
            lru_hash.emplace( key, ptr);
        }

        void update( const Key& key, const Value& value)
        {
            auto data_it = data.find( key);
            auto lru_it  = lru_hash.find( key);
            assert ( ( data_it == data.end()) == ( lru_it == lru_hash.end()));
            if ( data_it == data.end())
            {
                allocate( key, value);
            }
            else
            {
                assert( data_it->second.is_same( value));
                lru_list.splice( lru_list.begin(), lru_list, lru_it->second);
            }
        }

        void erase( const Key& key)
        {
            auto data_it = data.find( key);
            auto lru_it  = lru_hash.find( key);
            assert ( ( data_it == data.end()) == ( lru_it == lru_hash.end()));
            if ( data_it != data.end())
            {
                assert( number_of_elements > 0);
                data.erase( data_it);
                lru_list.erase( lru_it->second);
                lru_hash.erase( lru_it);

                number_of_elements--;
            }
        }

    private:
        std::unordered_map<Key, Value> data{};

        std::list<Key> lru_list{};
        std::unordered_map<Key, typename std::list<Key>::iterator> lru_hash{};

        std::size_t number_of_elements = 0u;
};


#endif // LRUCACHE_H
