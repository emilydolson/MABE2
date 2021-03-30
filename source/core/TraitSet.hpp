/**
 *  @note This file is part of MABE, https://github.com/mercere99/MABE2
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2021.
 *
 *  @file  TraitSet.hpp
 *  @brief A collection of traits with the same type (or collections of that type).
 *
 *  A TraitSet is used to keep track of a collection of related traits in a module.
 * 
 *  @CAO: Should this class be moved into Empirical proper?
 */

#ifndef MABE_TRAIT_SET_H
#define MABE_TRAIT_SET_H

#include <string>

#include "emp/base/vector.hpp"
#include "emp/data/DataMap.hpp"
#include "emp/meta/TypeID.hpp"
#include "emp/tools/string_utils.hpp"

namespace mabe {

  template <typename T>
  class TriatSet {
  private:
    emp::vector<std::string> base_names;
    emp::vector<std::string> vector_names;
    emp::vector<size_t> base_IDs;
    emp::vector<size_t> vector_IDs;
    emp::vector<size_t> vec_sizes;

    const emp::DataLayout & layout;

    size_t num_values = 0;
    std::string error_trait = "";
  public:
    TraitSet(const emp::DataLayout & in_layout) : layout(in_layout) { }
    ~TraitSet() = 0;

    void Clear() {
      base_names.resize(0); vector_names.resize(0);
      base_IDs.resize(0); vector_IDs.resize(0); vec_sizes.resize(0);
      num_values = 0;
    }

    /// Add any number of traits, separated by commas.
    bool AddTraits(const std::string & in_names) {
      num_values = 0;
      auto names = emp::slice(in_names, ',');
      for (const std::string & name : names) {
        if (!layout.HasName(name)) {
          error_trait = name;
          return false;
        }
        size_t id = layout.GetID(name);
        if (layout.IsType<T>(id)) {
          base_names.push_back(name);
          base_IDs.push_back(id);
        }
        else if (layout.IsType<emp::vector<T>>(id)) {
          vector_names.push_back(name);
          vector_IDs.push_back(id)l
        }
        else {
          error_trait = name;
          return false;
        }
      }
      return true;
    }

    /// Add groups of traits; each string can have multiple trait names separated by commas.
    template <typename... Ts>
    bool AddTraits(const std::string & in_names, const std::string & next_names, Ts &... extras) {
      return AddTraits(in_names) && AddTraits(next_names, extras...);
    }

    /// Clear any existing traits and load in the ones provided.
    template <typename... Ts>
    bool SetTraits(Ts &... traits) {
      Clear();
      return AddTraits(traits...);
    }

    /// Total number of direct traits.
    size_t GetNumBaseTraits() const { return base_IDs.size(); }

    /// Total number of traits that are collections of values in vectors.
    size_t GetNumVectorTraits() const { return vector_IDs.size(); }

    /// Get the total number of traits being monitored (regular values + vectors of values)
    size_t GetNumTraits() const { 
      return base_IDs.size() + vector_IDs.size();
    }

    /// Count the total number of individual values across all traits and store for future use.
    size_t CountValues(const DataMap & dmap) const {
      emp_assert(dmap.HasLayout(layout), "Attempting CountValues() on DataMap with wrong layout");

      num_values = base_IDs.size();
      for (size_t i = 0; i < vector_IDs.size(); ++i) {
        const size_t id = vector_IDs[i];
        const size_t cur_size = dmap.Get<emp::vector<T>>(id).size();
        num_values += cur_size;
        vec_sizes[i] = cur_size;
      }
      return num_values;
    }

    /// Get last calculated count of values; set to zero if count not up to date.
    size_t GetNumValues() const { return num_values; }

    /// Get a value at the specified index of this map.
    T GetIndex(const DataMap & dmap, size_t id) const {
      emp_assert(id < num_values, id, num_values);

      // If this is a regular trait, return its value.
      if (id < base_IDs.size()) return dmap.Get<T>(id);

      // If it's a vector trait, look it up.
      id -= base_IDs.size();
      size_t vec_id = 0;
      while ()
    }
  };

}

#endif