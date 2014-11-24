/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SEMISTATIC_MAP_TEMPLATES_H
#define SEMISTATIC_MAP_TEMPLATES_H

#ifndef IN_FRUIT_CPP_FILE
#error "Fruit .template.h file included in non-cpp file."
#endif

#include <algorithm>
#include <cassert>
#include <chrono>
#include <random>
#include <utility>

#include "semistatic_map.h"

namespace fruit {
namespace impl {

template <typename Key, typename Value>
void SemistaticMap<Key, Value>::insert(std::size_t h, 
                                       typename std::vector<value_type>::const_iterator elemsBegin,
                                       typename std::vector<value_type>::const_iterator elemsEnd) {
  
  value_type* oldBucketBegin = lookup_table[h].begin;
  value_type* oldBucketEnd = lookup_table[h].end;
  
  lookup_table[h].begin = values.data() + values.size();
  
  // Step 1: re-insert all keys with the same hash at the end (if any).
  for (value_type* p = oldBucketBegin; p != oldBucketEnd; ++p) {
    values.push_back(*p);
  }
  
  // Step 2: also insert the new keys and values
  for (typename std::vector<value_type>::const_iterator itr = elemsBegin; itr != elemsEnd; ++itr) {
    values.push_back(*itr);
  }
  
  lookup_table[h].end = values.data() + values.size();
  
  // The old sequence is no longer pointed to by any index in the lookup table, but recompacting the vectors would be too slow.
}

template <typename Key, typename Value>
SemistaticMap<Key, Value>::SemistaticMap(const SemistaticMap<Key, Value>& map, std::vector<value_type>&& newElements)
  : hash_function(map.hash_function), lookup_table(map.lookup_table) {
    
  // Sort by hash.
  std::sort(newElements.begin(), newElements.end(), [this](const value_type& x, const value_type& y) {
    return hash(x.first) < hash(y.first);
  });
  
  std::size_t additionalValues = newElements.size();
  // Add the space needed to store copies of the old buckets.
  for (typename std::vector<value_type>::iterator itr = newElements.begin(), itr_end = newElements.end();
       itr != itr_end;
       /* no increment */) {
    Unsigned h = hash(itr->first);
    auto p = map.lookup_table[h];
    additionalValues += (p.end - p.begin);
    for (; itr != itr_end && hash(itr->first) == h; ++itr) {
    }
  }
  
  values.reserve(additionalValues);
  
  // Now actually perform the insertions.

  for (typename std::vector<value_type>::iterator itr = newElements.begin(), itr_end = newElements.end();
       itr != itr_end;
       /* no increment */) {
    Unsigned h = hash(itr->first);
    auto p = map.lookup_table[h];
    additionalValues += (p.end - p.begin);
    typename std::vector<value_type>::iterator first = itr;
    for (; itr != itr_end && hash(itr->first) == h; ++itr) {
    }
    typename std::vector<value_type>::iterator last = itr;
    insert(h, first, last);
  }
}

template <typename Key, typename Value>
template <typename Iter>
SemistaticMap<Key, Value>::SemistaticMap(Iter valuesBegin, std::size_t num_values) {
  NumBits num_bits = pickNumBits(num_values);
  std::size_t num_buckets = (1 << num_bits);
  
  std::vector<Unsigned> count;
  count.reserve(num_buckets);
  
  hash_function.shift = (sizeof(Unsigned)*CHAR_BIT - num_bits);
  
  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  std::default_random_engine random_generator(seed);
  std::uniform_int_distribution<Unsigned> random_distribution;
  
  while (1) {
    hash_function.a = random_distribution(random_generator);
    count.assign(num_buckets, 0);
    
    Iter itr = valuesBegin;
    for (std::size_t i = 0; i < num_values; ++i, ++itr) {
      Unsigned& thisCount = count[hash((*itr).first)];
      ++thisCount;
      if (thisCount == beta) {
        goto pick_another;
      }
    }
    break;
    
    pick_another:;
  }
  
  values.resize(num_values);
  
  std::partial_sum(count.begin(), count.end(), count.begin());
  lookup_table.reserve(count.size());
  for (Unsigned n : count) {
    lookup_table.push_back(CandidateValuesRange{values.data() + n, values.data() + n});
  }
  
  // At this point lookup_table[h] is the number of keys in [first, last) that have a hash <=h.
  // Note that even though we ensure this after construction, it is not maintained by insert() so it's not an invariant.
  
  Iter itr = valuesBegin;
  for (std::size_t i = 0; i < num_values; ++i, ++itr) {
    value_type*& firstValuePtr = lookup_table[hash((*itr).first)].begin;
    --firstValuePtr;
    assert(values.data() <= firstValuePtr);
    assert(firstValuePtr < values.data() + values.size());
    *firstValuePtr = *itr;
  }
}

template <typename Key, typename Value>
const Value& SemistaticMap<Key, Value>::at(Key key) const {
  Unsigned h = hash(key);
  for (const value_type* p = lookup_table[h].begin; /* p!=lookup_table[h].end but no need to check */; ++p) {
    assert(p != lookup_table[h].end);
    if (p->first == key) {
      return p->second;
    }
  }
}

template <typename Key, typename Value>
const Value* SemistaticMap<Key, Value>::find(Key key) const {
  Unsigned h = hash(key);
  for (const value_type *p = lookup_table[h].begin, *p_end = lookup_table[h].end; p != p_end; ++p) {
    if (p->first == key) {
      return &(p->second);
    }
  }
  return nullptr;
}

} // namespace impl
} // namespace fruit

#endif // SEMISTATIC_MAP_TEMPLATES_H