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

#ifndef FRUIT_METAPROGRAMMING_PROOF_TREES_H
#define FRUIT_METAPROGRAMMING_PROOF_TREES_H

#include "basics.h"
#include "list.h"
#include "set.h"

#include <boost/mpl/vector.hpp>
#include <boost/mpl/set.hpp>
#include <boost/mpl/map.hpp>
#include <boost/mpl/transform_view.hpp>
#include <boost/mpl/has_key.hpp>
#include <boost/mpl/erase_key.hpp>
#include <boost/mpl/fold.hpp>
#include <boost/mpl/insert.hpp>
#include <boost/mpl/filter_view.hpp>
#include <boost/mpl/logical.hpp>

namespace fruit {
namespace impl {

using namespace boost::mpl;

// A proof forest is represented as a map, where elements are keyed by a formula Th and map to a mpl::set of formulas Hps of the
// form {Hp1, ... Hp(n)}. Each element represents a proof tree:
// 
// Hp1 ... Hp(n)
// -------------
//      Th
// 
// The keys of the map (theses) must not appear as hypotheses in the same proof forest.
// Formulas are atomic, any type can be used as formula.
using EmptyProofForest = map<>;

// Removes the specified Hp from the proof tree.
template <typename Hp>
struct RemoveHpFromProofTree {
  template <typename Proof>
  struct apply {
    using type = pair<typename Proof::first,
                      Eval<erase_key<typename Proof::second, Hp>>>;
  };
};

// TODO: Fix typo.
#ifndef FRUIT_NO_LOOP_CHECKz

// Constructs a proof forest tree with the given theses and where all theses have the same (given) hypotheses.
// The hypotheses don't have to be distinct. If you know that they are, use ConsProofTree instead.
// Hps must be a List.
struct ConstructProofForest {
  template <typename Hps, typename... Ths>
  struct apply {
    using HpsSet = Apply<ListToSet, Hps>;
    using type = map<pair<Ths, HpsSet>...>;
  };
};

// Checks if the given proof tree has the thesis among its hypotheses.
struct HasSelfLoop {
  template <typename Proof>
  struct apply : has_key<typename Proof::second, typename Proof::first> {
  };
};

struct CombineForestHypothesesWithProofHelper {
  template <typename Proof, typename NewProof>
  struct apply {
    using type = pair<typename Proof::first,
                      Apply<SetUnion,
                            typename NewProof::second,
                            erase_key<typename Proof::second, typename NewProof::first>>>;
  };
};

// Combines `Proof' with the hypotheses in the given proof forest. If the thesis of Proof is among the hypotheses of a proof in
// the tree, it is removed and Proof's hypotheses are added in its place.
// Returns the modified forest.
struct CombineForestHypothesesWithProof {
  template <typename Forest, typename NewProof>
  struct apply;

  template <typename... Proofs, typename NewProof>
  struct apply<List<Proofs...>, NewProof> {
    using type = List<Conditional<has_key<typename Proofs::Hps, typename NewProof::Th>::value,
                                  LazyApply<CombineForestHypothesesWithProofHelper, Proofs, NewProof>,
                                  Lazy<Proofs>>
                      ...>;
  };
};

struct CombineProofHypothesesWithForestHelper {
  template <typename Hps, typename Forest>
  struct apply;

  template <typename Hps, typename... Proofs>
  struct apply<Hps, List<Proofs...>> {
    using type = List<Eval<std::conditional<ApplyC<IsInList, typename Proofs::Th, Hps>::value,
                                            typename Proofs::Hps,
                                            List<>>>...>;
  };
};

// Combines Forest into Proof by replacing each theses of Forest that is an hypothesis in Proof with its hypotheses in Forest.
// Returns the modified proof.
// ForestThs is also passed as parameter, but just as an optimization.
// TODO: ListOfSetsUnion is slow, consider optimizing here to avoid it. E.g. we could try finding each Hp of Proof in Forest, and
//       then do the union of the results.
// TODO: See if ForestThs improves performance, if not remove it.
struct CombineProofHypothesesWithForest {
  template <typename Proof, typename Forest, typename ForestThs>
  struct apply {
    using type = ConsProofTree<
      Apply<SetUnion,
        Apply<ListOfSetsUnion,
          Apply<CombineProofHypothesesWithForestHelper, typename Proof::Hps, Forest>
        >,
        Apply<SetDifference,
          typename Proof::Hps,
          ForestThs
        >
      >,
      typename Proof::Th>;
  };
};

struct ForestTheses {
  template <typename Forest>
  struct apply;
  
  template <typename... Proof>
  struct apply<List<Proof...>> {
    using type = List<typename Proof::Th...>;
  };
};

// Adds Proof to Forest. If doing so would create a loop (a thesis depending on itself), returns None instead.
struct AddProofTreeToForest {
  template <typename Proof, typename Forest, typename ForestThs>
  struct apply {
    FruitStaticAssert(ApplyC<IsSameSet, Apply<ForestTheses, Forest>, ForestThs>::value, "");
    using NewProof = Apply<CombineProofHypothesesWithForest, Proof, Forest, ForestThs>;
    // Note that NewProof might have its own thesis as hypothesis.
    // At this point, no hypotheses of NewProof appear as theses of Forest. A single replacement step is sufficient.
    using type = Eval<std::conditional<ApplyC<HasSelfLoop, NewProof>::value,
                                       None,
                                       Apply<AddToList,
                                             NewProof,
                                             Apply<CombineForestHypothesesWithProof, Forest, NewProof>>
                                       >>;
  };
};

struct AddProofTreesToForest {
  // Case with empty Proofs....
  template <typename Forest, typename ForestThs, typename... Proofs>
  struct apply {
    using type = Forest;
  };

  template <typename Forest, typename ForestThs, typename Proof, typename... Proofs>
  struct apply<Forest, ForestThs, Proof, Proofs...> {
    using type = Apply<AddProofTreesToForest,
                       Apply<AddProofTreeToForest, Proof, Forest, ForestThs>,
                       Apply<AddToList, typename Proof::Th, ForestThs>,
                       Proofs...>;
  };
};

struct AddProofTreeListToForest {
  template <typename Proofs, typename Forest, typename ForestThs>
  struct apply {
    using type = ApplyWithList<AddProofTreesToForest, Proofs, Forest, ForestThs>;
  };
};

// Returns the proof with the given thesis in Proofs..., or None if there isn't one.
struct FindProofInProofs {
  // Case with empty Proofs...
  template <typename Th, typename... Proofs>
  struct apply {
    using type = None;
  };
  
  template <typename Th, typename Hps, typename... Proofs>
  struct apply<Th, ConsProofTree<Hps, Th>, Proofs...> {
    using type = ConsProofTree<Hps, Th>;
  };
  
  template <typename Th, typename Th1, typename Hps1, typename... Proofs>
  struct apply<Th, ConsProofTree<Hps1, Th1>, Proofs...> : public apply<Th, Proofs...> {
  };
};

// Returns the proof with the given thesis in Forest, or None if there isn't one.
struct FindProofInForest {
  template <typename Th, typename Forest>
  struct apply {
    using type = ApplyWithList<FindProofInProofs, Forest, Th>;
  };
};

struct IsProofEntailedByForestHelper {
  template <typename Proof, typename Proof1>
  struct apply : ApplyC<IsEmptyList, Apply<SetDifference, typename Proof1::Hps, typename Proof::Hps>> {
  };
};

// Checks whether Proof is entailed by Forest, i.e. whether there is a corresponding Proof1 in Forest with the same thesis
// and with the same hypotheses as Proof (or a subset).
struct IsProofEntailedByForest {
  template <typename Proof, typename Forest>
  struct apply {
    using Proof1 = Apply<FindProofInForest, typename Proof::Th, Forest>;
    using Result = Eval<std::conditional<std::is_same<Proof1, None>::value,
                                         std::false_type,
                                         ApplyC<IsProofEntailedByForestHelper, Proof, Proof1>>>;
    constexpr static bool value = Result::value;
  };
};

struct IsForestEntailedByForest {
  template <typename EntailedForest, typename Forest>
  struct apply;

  template <typename... EntailedProofs, typename Forest>
  struct apply<List<EntailedProofs...>, Forest> 
  : public StaticAnd<ApplyC<IsProofEntailedByForest, EntailedProofs, Forest>::value...> {
  };
};

// Given two proof trees, check if they are equal.
struct IsProofTreeEqualTo {
  template <typename Proof1, typename Proof2>
  struct apply {
    constexpr static bool value = std::is_same<typename Proof1::Th, typename Proof2::Th>::value
                               && ApplyC<IsSameSet, typename Proof1::Hps, typename Proof2::Hps>::value;
  };
};

// Given two proofs forests, check if they are equal.
// This is not very efficient, consider re-implementing if it will be used often.
struct IsForestEqualTo {
  template <typename Forest1, typename Forest2>
  struct apply {
    constexpr static bool value = ApplyC<IsForestEntailedByForest, Forest1, Forest2>::value
                               && ApplyC<IsForestEntailedByForest, Forest2, Forest1>::value;
    // TODO: Remove
    static_assert(value, "");
  };
};

#else // FRUIT_NO_LOOP_CHECK

struct ConstructProofTree {
  template <typename P, typename Rs>
  struct apply {
    using type = None;
  };
};

struct ConstructProofForest {
  template <typename Rs, typename... P>
  struct apply {
    using type = List<>;
  };
};

struct AddProofTreeToForest {
  template <typename Proof, typename Forest, typename ForestThs>
  struct apply {
    using type = List<>;
  };
};

struct AddProofTreeListToForest {
  template <typename Proofs, typename Forest, typename ForestThs>
  struct apply {
    using type = List<>;
  };
};

#endif // FRUIT_NO_LOOP_CHECK

} // namespace impl
} // namespace fruit


#endif // FRUIT_METAPROGRAMMING_PROOF_TREES_H
