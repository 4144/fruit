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

#ifndef FRUIT_COMPONENT_STORAGE_H
#define FRUIT_COMPONENT_STORAGE_H

#include "../util/type_info.h"
#include "normalized_component_storage.h"
#include "../data_structures/bag.h"
#include "../../fruit_forward_decls.h"

namespace fruit {
  
namespace impl {

/**
 * A component where all types have to be explicitly registered, and all checks are at runtime.
 * Used to implement Component<>, don't use directly.
 * This merely stores the BindingData/CompressedBinding/MultibindingData objects. The real processing will be done in
 * NormalizedComponentStorage and InjectorStorage.
 * 
 * This class handles the creation of types of the forms:
 * - shared_ptr<C>, [const] C*, [const] C&, C (where C is an atomic type)
 * - Injector<T1, ..., Tk> (with T1, ..., Tk of the above forms).
 */
class ComponentStorage {
private:  
  // Duplicate elements (elements with the same typeId) are not meaningful and will be removed later.
  Bag<std::pair<TypeId, BindingData>> bindings;
  
  // All elements in this vector are best-effort. Removing an element from this vector does not affect correctness.
  Bag<CompressedBinding> compressed_bindings;
  
  // Duplicate elements (elements with the same typeId) *are* meaningful, these are multibindings.
  Bag<std::pair<TypeId, MultibindingData>> multibindings;
  
  template <typename... Ts>
  friend class fruit::Injector;
  
  friend class NormalizedComponentStorage;
  friend class InjectorStorage;
  
public:
  void addBinding(std::tuple<TypeId, BindingData> t);
  
  // Takes a tuple (getTypeId<I>(), getTypeId<C>(), bindingData)
  void addCompressedBinding(std::tuple<TypeId, TypeId, BindingData> t);
  
  void addMultibinding(std::tuple<TypeId, MultibindingData> t);
  
  void install(ComponentStorage&& other);
};

} // namespace impl
} // namespace fruit

#include "component_storage.defn.h"

#endif // FRUIT_COMPONENT_STORAGE_H
