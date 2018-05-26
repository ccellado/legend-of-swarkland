#ifndef HASHTABLE_HPP
#define HASHTABLE_HPP

#include "util.hpp"
#include "uint_oversized.hpp"

template<size_t Size64>
static inline uint32_t hash_oversized(const uint_oversized<Size64> & a) {
    // it's just a bunch of xor
    uint64_t result = 0;
    for (size_t i = 0; i < Size64; i++)
        result ^= a.values[i];
    return (uint32_t)(result >> 32) ^ (uint32_t)(result & 0x00000000ffffffffULL);
}

template<size_t Size64>
static inline uint_oversized<Size64> random_oversized() {
    uint_oversized<Size64> result;
    for (size_t i = 0; i < Size64; i++)
        result.values[i] = ((uint64_t)random_uint32()) << 32 | (uint64_t)random_uint32();
    return result;
}

typedef uint_oversized<4> uint256;
uint32_t hash_uint256(const uint256 & a);
static inline uint256 random_uint256() {
    return random_oversized<4>();
}

template<typename K, typename V, uint32_t (*HashFunction)(const K&)>
class Hashtable {
private:
    struct Entry;
public:
    Hashtable() {
        init_capacity(16);
    }
    ~Hashtable() {
        destroy(_entries, _capacity);
    }
    int size() const {
        return _size;
    }

    void put(const K & key, const V & value) {
        _modification_count++;
        internal_put(key, value);

        // if we get too full (80%), double the capacity
        if (_size * 5 >= _capacity * 4) {
            Entry * old_entries = _entries;
            int old_capacity = _capacity;
            init_capacity(_capacity * 2);
            // dump all of the old elements into the new table
            for (int i = 0; i < old_capacity; i += 1) {
                Entry * old_entry = &old_entries[i];
                if (old_entry->used)
                    internal_put(old_entry->key, old_entry->value);
            }
            destroy(old_entries, old_capacity);
        }
    }

    V get(const K & key) const {
        Entry * entry = internal_get(key);
        assert_str(entry != nullptr, "key not found");
        return entry->value;
    }

    V get(const K & key, const V & default_value) const {
        Entry * entry = internal_get(key);
        return entry ? entry->value : default_value;
    }

    void remove(const K & key) {
        _modification_count++;
        int start_index = HashFunction(key) % _capacity;
        for (int roll_over = 0; roll_over <= _max_distance_from_start_index; roll_over += 1) {
            int index = (start_index + roll_over) % _capacity;
            Entry * entry = &_entries[index];

            if (!entry->used)
                return; // not found

            if (entry->key != key)
                continue;

            for (; roll_over < _capacity; roll_over += 1) {
                int next_index = (start_index + roll_over + 1) % _capacity;
                Entry * next_entry = &_entries[next_index];
                if (!next_entry->used || next_entry->distance_from_start_index == 0) {
                    entry->used = false;
                    _size -= 1;
                    return;
                }
                *entry = *next_entry;
                entry->distance_from_start_index -= 1;
                entry = next_entry;
            }
            panic("shifting everything in the table");
        }
        return; // not found
    }

    void clear() {
        for (int i = 0; i < _capacity; i++)
            _entries[i].used = false;
        _size = 0;
        _max_distance_from_start_index = 0;
        _modification_count++;
    }

    class Iterator {
    public:
        bool next(V * output) {
            assert_str(_inital_modification_count == _table->_modification_count, "concurrent modification");
            if (_count >= _table->size())
                return false;
            for (; _index < _table->_capacity; _index++) {
                Entry * entry = &_table->_entries[_index];
                if (entry->used) {
                    _index++;
                    _count++;
                    *output = entry->value;
                    return true;
                }
            }
            panic("no next item");
        }
    private:
        const Hashtable * _table;
        // how many items have we returned
        int _count = 0;
        // iterator through the entry array
        int _index = 0;
        // used to detect concurrent modification
        uint32_t _inital_modification_count;
        Iterator(const Hashtable * table) :
                _table(table), _inital_modification_count(table->_modification_count) {
        }
        friend Hashtable;
    };
    // you must not modify the underlying hashtable while this iterator is still in use
    Iterator value_iterator() const {
        return Iterator(this);
    }

private:
    struct Entry {
        bool used;
        int distance_from_start_index;
        K key;
        V value;
    };

    Entry * _entries;
    int _capacity;
    int _size;
    int _max_distance_from_start_index;
    // this is used to detect bugs where a hashtable is edited while an iterator is running.
    uint32_t _modification_count = 0;

    void init_capacity(int capacity) {
        _capacity = capacity;
        _entries = allocate<Entry>(_capacity);
        _size = 0;
        _max_distance_from_start_index = 0;
        for (int i = 0; i < _capacity; i += 1) {
            _entries[i].used = false;
        }
    }

    void internal_put(K key, V value) {
        int start_index = HashFunction(key) % _capacity;
        for (int roll_over = 0, distance_from_start_index = 0; roll_over < _capacity; roll_over += 1, distance_from_start_index += 1) {
            int index = (start_index + roll_over) % _capacity;
            Entry * entry = &_entries[index];

            if (entry->used && entry->key != key) {
                if (entry->distance_from_start_index < distance_from_start_index) {
                    // robin hood to the rescue
                    Entry tmp = *entry;
                    if (distance_from_start_index > _max_distance_from_start_index)
                        _max_distance_from_start_index = distance_from_start_index;
                    *entry = {
                        true,
                        distance_from_start_index,
                        key,
                        value,
                    };
                    key = tmp.key;
                    value = tmp.value;
                    distance_from_start_index = tmp.distance_from_start_index;
                }
                continue;
            }

            if (!entry->used) {
                // adding an entry. otherwise overwriting old value with
                // same key
                _size += 1;
            }

            if (distance_from_start_index > _max_distance_from_start_index)
                _max_distance_from_start_index = distance_from_start_index;
            *entry = {
                true,
                distance_from_start_index,
                key,
                value,
            };
            return;
        }
        panic("put into a full HashMap");
    }

    Entry * internal_get(const K & key) const {
        int start_index = HashFunction(key) % _capacity;
        for (int roll_over = 0; roll_over <= _max_distance_from_start_index; roll_over += 1) {
            int index = (start_index + roll_over) % _capacity;
            Entry * entry = &_entries[index];

            if (!entry->used)
                return nullptr;

            if (entry->key == key)
                return entry;
        }
        return nullptr;
    }
};

template<typename T>
using IdMap = Hashtable<uint256, T, hash_uint256>;

#endif
