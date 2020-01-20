/**
 *  @note This file is part of MABE, https://github.com/mercere99/MABE2
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2019-2020.
 *
 *  @file  TraitInfo.h
 *  @brief Information about a single phenotypic trait.
 *
 *  A TraitInfo object contains basic information a about a single trait being tracked in an
 *  organism.  In addition to the name, type, and description of the trait, it also describes:
 *
 *  The ACCESS method to be used for a trait by each module.  A trait can be
 *    [PRIVATE]  - Only this module can access it; no others should read or write it.
 *    [OWNED]    - Only this module can modify the trait, but other modules can read it.
 *    [SHARED]   - This module will read and write this trait, but others are allowed to as well.
 *    [REQUIRED] - This module will read the trait; another module must write to it.
 *
 *  The INIT method describes how a trait should be initialized in a new offspring.
 *  (note that injected organisms always get the DEFAULT value.)
 *    [DEFAULT] - Always initialize this trait to its default value.
 *    [FIRST]   - Initialize trait to the first parent's value (only parent for asexual runs)
 *    [AVERAGE] -  Initiialize trait to the average value of all parents.
 *    [MINIMUM] -  Initiialize trait to the minimum value of all parents.
 *    [MAXIMUM] -  Initiialize trait to the maximum value of all parents.
 *
 *  The ARCHIVE method determines how many older values should be saved with each organism.
 *    [NONE]       - Only the most recent value should be tracked, no archived values.
 *    [AT_BIRTH]   - Store value of this trait was born with in "birth_(name)".
 *    [LAST_REPRO] - Store value of trait at the last reproduction in "last_(name)".
 *    [ALL_REPROS] - Store all value of trait at each reproduction event in "archive_(name)".
 *    [ALL_VALUES] - Store every value chane of trait at any time in "sequence_(name)".
 *
 *  The SUMMARY method determines how a trait should be summarized over a collection of organisms.
 *    [[[ needs refinement... ]]]
 */

#ifndef MABE_TRAIT_INFO_H
#define MABE_TRAIT_INFO_H

#include <string>

#include "meta/TypeID.h"

namespace mabe {

  class ModuleBase;

  class TraitInfo {
  protected:
    std::string name="";         ///< Unique name for this trait.
    std::string desc="";         ///< Description of this trait.
    emp::TypeID type;            ///< Type identifier for this trait.

  public:
    /// Which modules are allowed to read or write this trait?
    enum Access {
      UNKNOWN=0,   ///< Access level unknown; most likely a problem!
      PRIVATE,     ///< Can READ & WRITE this trait; other modules cannot use it at all.
      OWNED,       ///< Can READ & WRITE this trait; other modules can only read.
      SHARED,      ///< Can READ & WRITE this trait; other modules can too.
      REQUIRED,    ///< Can READ this trait, but another module must WRITE to it.
      NUM_ACCESS   ///< How many access methods are there?
    };

    /// How should this trait be initialized in a newly-born organism?
    /// * Injected organisms always use the default value.
    /// * Modules can moitor signals to make other changes at any time.
    enum Init {
      DEFAULT=0, ///< Trait is initialized to a pre-set default value.
      FIRST,     ///< Trait is inhereted (from first parent if more than one)
      AVERAGE,   ///< Trait becomes average of all parents on birth.
      MINIMUM,   ///< Trait becomes lowest of all parents on birth.
      MAXIMUM    ///< Trait becomes highest of all parents on birth.
    };

    /// Which information should we store in the trait as we go?
    /// A "reproduction event" for an organism is when it's born and each time it gives birth.
    enum Archive {
      NONE=0,     ///< Don't store any older information.
      AT_BIRTH,   ///< Store value this trait was born with in "birth_(name)"
      LAST_REPRO, ///< Store value at last reproduction event in "last_(name)"
      ALL_REPRO,  ///< Store values at all reproduction events (including birth) in "archive_(name)"
      ALL_VALUES  ///< Store values from every change in "sequence_(name)"
      // @CAO: ALL_VALUES can be hard to track...
    };

    /// What timings should we use when we summarize data?  (Must maintain summary!)
    enum Timing {
      LATEST,     ///< Always use the most recent value set.
      PARENT,     ///< Use value of parent at time of organism birth.
      REPRO       ///< Use value last time organism replicated (or parent value if no births)
    };

    /// How should these data be summarized in groups such as whole population or phyla types
    /// (such as Genotype, Species, etc.)  Some traits shouldn't be summarized at all (IGNORE)
    /// Otherwise the summary values can be takens as:
    enum Summary {
      IGNORE=0,   ///< Don't include this trait in phyla records.
      AVERAGE,    ///< Average of current value of all organisms (or final value at death).
      SUMMARY,    ///< Basic summary (min, max, count, ave) of current/final values.
      FULL,       ///< Store ALL current/final values for organisms.
    };

  protected:
    Init init = Init::DEFAULT;
    bool reset_parent = false;  ///< Should the parent ALSO be reset on birth?
    Archive archive = Archive::NONE;
    Summary summary = Summary::IGNORE;

    // Track which modules are using this trait and what access they need.
    using mod_ptr_t = emp::Ptr<ModuleBase>;
    struct ModuleInfo {
      std::string mod_name = "";
      mod_ptr_t mod_ptr = nullptr;
      Access access = Access::UNKNOWN;
    };
    emp::vector<ModuleInfo> access_info;

    // Specific access categories
    emp::array<size_t, NUM_ACCESS> access_counts = { 0, 0, 0, 0, 0 };

    // Helper functions
    int GetInfoID(const std::string & mod_name) const {
      for (int i = 0; i < (int) access_info.size(); i++) {
        if (access_info[(size_t) i].mod_name == mod_name) return i;
      }
      return -1;
    }

    int GetInfoID(mod_ptr_t mod_ptr) const {
      for (int i = 0; i < (int) access_info.size(); i++) {
        if (access_info[(size_t) i].mod_ptr == mod_ptr) return i;
      }
      return -1;
    }

  public:
    virtual ~TraitInfo() { ; }

    const std::string & GetName() const { return name; }
    const std::string & GetDesc() const { return desc; }
    emp::TypeID GetType() const { return type; }

    /// Determine what kind of access a module has.
    Access GetAccess(mod_ptr_t mod_ptr) const {
      int id = GetInfoID(mod_ptr);
      if (id == -1) return Access::UNKNOWN;
      return access_info[id].access;
    }

    /// Determine if a module has any knd of access to this trait.
    bool HasAccess(mod_ptr_t mod_ptr) const { return GetAccess(mod_ptr) != Access::UNKNOWN; }

    /// How many modules can access this trait?
    size_t GetModuleCount() const { return access_info.size(); }

    /// How many modules can access this trait using a specified access mode?
    size_t GetAccessCount(Access access) const { return access_counts[access]; }

    bool IsPrivate() const { return GetAccessCount(Access::PRIVATE); }
    bool IsOwned() const { return GetAccessCount(Access::OWNED); }
    bool IsShared() const { return GetAccessCount(Access::SHARED); }
    bool IsRequired() const { return GetAccessCount(Access::REQUIRED); }

    size_t GetUnknownCount() const { return GetAccessCount(Access::UNKNOWN); }
    size_t GetPrivateCount() const { return GetAccessCount(Access::PRIVATE); }
    size_t GetOwnedCount() const { return GetAccessCount(Access::OWNED); }
    size_t GetSharedCount() const { return GetAccessCount(Access::SHARED); }
    size_t GetRequiredCount() const { return GetAccessCount(Access::REQUIRED); }

    emp::vector<std::string> GetModuleNames() const {
      emp::vector<std::string> mod_names;
      for (auto info : access_info) {
        mod_names.push_back(info.mod_name);
      }
      return mod_names;
    }

    emp::vector<std::string> GetModuleNames(Access test_access) const {
      emp::vector<std::string> mod_names;
      for (auto info : access_info) {
        if (info.access == test_access) mod_names.push_back(info.mod_name);
      }
      return mod_names;
    }

    emp::vector<std::string> GetUnknownNames() const { return GetModuleNames(Access::UNKNOWN); }
    emp::vector<std::string> GetPrivateNames() const { return GetModuleNames(Access::PRIVATE); }
    emp::vector<std::string> GetOwnedNames() const { return GetModuleNames(Access::OWNED); }
    emp::vector<std::string> GetSharedNames() const { return GetModuleNames(Access::SHARED); }
    emp::vector<std::string> GetRequiredNames() const { return GetModuleNames(Access::REQUIRED); }

    /// Was a default value set for this trait (can only be done in overload that knows type)
    virtual bool HasDefault() { return false; }
    bool GetResetParent() const { return reset_parent; }

    Init GetInit() const { return init; }
    Archive GetArchive() const { return archive; }
    TypeRecord GetTypeRecord() const { return type_record; }

    TraitInfo & SetName(const std::string & in_name) { name = in_name; return *this; }
    TraitInfo & SetDesc(const std::string & in_desc) { desc = in_desc; return *this; }
    TraitInfo & AddUser(mod_ptr_t in_mod) { users.push_back(in_mod); return *this; }

    /// Set the access level of this trait to a specified level.
    TraitInfo & SetAccess(Access in_access) { access = in_access; return *this; }

    /// Set the current value of this trait to be automatically inthereted by offspring.
    TraitInfo & SetInheritParent() { init = Init::FIRST; return *this; }

    /// Set the average across parents for this trait to be automatically inthereted by offspring.
    TraitInfo & SetInheritAverage() { init = Init::AVERAGE; return *this; }

    /// Set the minimum across parents for this trait to be automatically inthereted by offspring.
    TraitInfo & SetInheritMinimum() { init = Init::MINIMUM; return *this; }

    /// Set the maximum across parents for this trait to be automatically inthereted by offspring.
    TraitInfo & SetInheritMaximum() { init = Init::MAXIMUM; return *this; }

    /// Set the parent to ALSO reset to the same value as the offspring on divide.
    TraitInfo & SetParentReset() { reset_parent = true; return *this; }

    /// Set the previous value of this trait to be stored on birth or reset.
    TraitInfo & SetArchiveLast() { archive = Archive::LAST_RESET; return *this; }

    /// Set ALL previous values of this trait to be store after each reset.
    TraitInfo & SetArchiveAll() { archive = Archive::ALL_RESET; return *this; }
  };

  template <typename T>
  class TypedTraitInfo : public TraitInfo {
  private:
    T default_value;
    bool has_default;

  public:
    TypedTraitInfo(const std::string & in_name="") : has_default(false)
    {
      name = in_name;
      type = emp::GetTypeID<T>();
    }

    TypedTraitInfo(const std::string & in_name, const T & in_default)
      : default_value(in_default), has_default(true)
    {
      name = in_name;
      type = emp::GetTypeID<T>();
    }

    bool HasDefault() { return has_default; }

    const T & GetDefault() const { return default_value; }

    TypedTraitInfo<T> & SetDefault(const T & in_default) {
      default_value = in_default;
      has_default = true;
      return *this;
    }
  };

}

#endif
