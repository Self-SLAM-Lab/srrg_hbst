#include <iostream>
#include "srrg_hbst_types/binary_tree.hpp"

//ds current setup
#define DESCRIPTOR_SIZE_BITS 256
typedef srrg_hbst::BinaryMatchable<DESCRIPTOR_SIZE_BITS> Matchable;
typedef srrg_hbst::BinaryNode<Matchable> Node;
typedef srrg_hbst::BinaryTree<Node> Tree;

//ds dummy descriptor generation
Tree::MatchableVector getDummyMatchables(const uint64_t& number_of_matchables_, const uint64_t& identifier_tree_) {

  //ds preallocate vector
  Tree::MatchableVector matchables(number_of_matchables_);

  //ds set values
  for(uint64_t identifier = 0; identifier < number_of_matchables_; ++identifier) {

    //ds generate a "random" descriptor by flipping some bits
    Matchable::Descriptor descriptor;
    for (uint32_t u = 0; u < DESCRIPTOR_SIZE_BITS; ++u) {
      if (rand()%2) {
        descriptor.flip(u);
      }
    }

    //ds set matchable
    matchables[identifier] = new Matchable(identifier, descriptor, identifier_tree_);
  }

  //ds done
  return matchables;
}

int32_t main() {

  //ds obtain a vector of reference matchables
  std::vector<Tree::MatchableVector> matchables_per_image(10);
  for (uint32_t u = 0; u < 10; ++u) {
    matchables_per_image[u] = getDummyMatchables(10000, u);
  }

  //ds allocate a BTree object on these descriptors (no shared pointer passed as the tree will have its own constant copy of the train descriptors)
  std::cerr << "building initial tree" << std::endl;
  Tree hbst_tree(0, matchables_per_image.front());
  std::cerr << "built initial tree" << std::endl;

  //ds add other elements to the tree
  std::cerr << "growing tree" << std::endl;
  for (uint32_t u = 1; u < 10; ++u) {
    std::cerr << "training on matches of image: " << u << std::endl;
    hbst_tree.add(matchables_per_image[u]);
  }
  std::cerr << "grown tree" << std::endl;

  //ds query descriptor pool
  const Tree::MatchableVector matchables_query = getDummyMatchables(5000, 1);

  //ds query the tree with maximum distance 25
  std::cerr << "querying tree" << std::endl;
  Tree::MatchVector matches;
  hbst_tree.match(matchables_query, matches, 25);
  std::cerr << "queried tree" << std::endl;

  //ds fight memory leaks!
  for (const Matchable* matchable: matchables_query) {
    delete matchable;
  }
  return 0;
}