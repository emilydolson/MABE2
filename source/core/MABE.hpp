/**
 *  @note This file is part of MABE, https://github.com/mercere99/MABE2
 *  @copyright Copyright (C) Michigan State University, MIT Software license; see doc/LICENSE.md
 *  @date 2019-2021.
 *
 *  @file  MABE.hpp
 *  @brief Master controller object for a MABE run.
 *
 *  This is the master MABE controller object that hooks together all of the modules and provides
 *  the interface for them to interact.
 *
 *  MABE allows modules to interact via a set of SIGNALS that they can listen for by overriding
 *  specific base class functions.  See Module.h for a full list of available signals.
 */

#ifndef MABE_MABE_HPP
#define MABE_MABE_HPP

#include <limits>
#include <string>
#include <sstream>

#include "emp/base/array.hpp"
#include "emp/base/Ptr.hpp"
#include "emp/base/vector.hpp"
#include "emp/config/command_line.hpp"
#include "emp/control/Signal.hpp"
#include "emp/data/DataMap.hpp"
#include "emp/data/DataMapParser.hpp"
#include "emp/datastructs/vector_utils.hpp"
#include "emp/math/Random.hpp"
#include "emp/tools/string_utils.hpp"

#include "../Emplode/Emplode.hpp"

#include "Collection.hpp"
#include "data_collect.hpp"
#include "ErrorManager.hpp"
#include "MABEBase.hpp"
#include "ModuleBase.hpp"
#include "Population.hpp"
#include "SigListener.hpp"
#include "TraitManager.hpp"

namespace mabe {

  ///  @brief The main MABE controller class
  ///
  ///  The MABE controller class manages interactions among modules, ensures that needed
  ///  components are present at startup, and triggers signals as needed.  Derived from
  ///  MABEBase, which handles all population manipulation and signal management.

  class MABE : public MABEBase {
  private:
    const std::string VERSION = "0.0.1";

    // --- Variables to handle configuration, initialization, and error reporting ---

    bool verbose = false;        ///< Should we output extra information during setup?
    bool show_help = false;      ///< Should we show "help" before exiting?
    std::string help_topic="";   ///< What topic should we give help about?
    bool exit_now = false;       ///< Do we need to immediately clean up and exit the run?
    ErrorManager error_man;      ///< Object to manage warnings and errors.

    /// Populations used; generated in the configuration file.
    emp::vector< emp::Ptr<Population> > pops;

    /// Organism pointer to use for all empty cells.
    emp::Ptr<Organism> empty_org = nullptr;

    /// Trait information to be stored on each organism.  Tracks the name, type, and current
    /// value of all traits that modules associate with organisms.
    emp::DataMap org_data_map;

    TraitManager<ModuleBase> trait_man; ///< Manage consistent read/write access to traits
    emp::DataMapParser dm_parser;       ///< Parser to process functions on a data map
    emp::Random random;                 ///< Master random number generator
    size_t update = 0;                  ///< How many times has Update() been called?


    // --- Config information for command-line arguments ---
    struct ArgInfo {
      std::string name;    ///< E.g.: "help" which would be called with "--help"
      std::string flag;    ///< E.g.: "h" which would be called with -h
      std::string args;    ///< Type of arguments needed: E.g.: "[filename...]"
      std::string desc;    ///< E.g.: "Print available command-line options."

      /// Function to call when triggered.
      using fun_t = std::function<void(const emp::vector<std::string> &)>;
      fun_t action;

      ArgInfo(const std::string & _n, const std::string & _f, const std::string & _a,
              const std::string & _d, fun_t _action)
        : name(_n), flag(_f), args(_a), desc(_d), action(_action) { }
    };

    emp::vector<ArgInfo> arg_set;              ///< Info about valid command-line arguments.
    emp::vector<std::string> args;             ///< Command-line arguments passed in.
    emp::vector<std::string> config_filenames; ///< Names of configuration files to load.
    emp::vector<std::string> config_settings;  ///< Additional config commands to run.
    std::string gen_filename;                  ///< Name of output file to generate.
    emplode::Emplode config;                   ///< Configuration information for this run.


    // ----------- Helper Functions -----------    
    void ShowHelp();      ///< Print information on how to run the software.
    void ShowModules();   ///< List all available modules in the current compilation.
    void ProcessArgs();   ///< Process all arguments passed in on the command line.

    // -- Helper functions to be called inside of Setup() --
    void Setup_Modules(); ///< Run SetupModule() method on each module we've loaded.
    void Setup_Traits();  ///< Load organism traits and test for module conflicts.    
    void UpdateSignals(); ///< Link signals only to modules that respond to them.

    /// Find any instances of ${X} and eval the X.
    std::string Preprocess(const std::string & in_string);

    /// Setup a function as deprecated so we can phase it out.
    void Deprecate(const std::string & old_name, const std::string & new_name);

  public:
    MABE(int argc, char* argv[]);  ///< MABE command-line constructor.
    MABE(const MABE &) = delete;
    MABE(MABE &&) = delete;
    ~MABE() {
      before_exit_sig.Trigger();                      // Notify modules of end...

      for (auto mod_ptr : modules) mod_ptr.Delete();  // Delete all modules.
      for (auto pop_ptr : pops) {                     // Delete all populations.
        ClearPop(*pop_ptr);
        pop_ptr.Delete();
      }
      // Delete the empty organism AFTER clearing the populations, so it's not still used.
      if (empty_org) empty_org.Delete();
    }

    // --- Basic accessors ---
    emp::Random & GetRandom() { return random; }
    size_t GetUpdate() const noexcept { return update; }
    bool GetVerbose() const { return verbose; }
    mabe::ErrorManager & GetErrorManager() { return error_man; }

    /// Output args if (and only if) we are in verbose mode.
    template <typename... Ts> void Verbose(Ts &&... args) {
      if (verbose) std::cout << emp::to_string(std::forward<Ts>(args)...) << std::endl;
    }

    // --- Tools to setup runs ---
    bool Setup();

    /// Build a placeholder organism for "empty" positions in a Population
    template <typename EMPTY_MANAGER_T> void SetupEmpty();

    /// Update MABE a single time step.
    void Update(size_t num_updates=1);

    // --- Population Management ---

    size_t GetNumPopulations() const { return pops.size(); }
    int GetPopID(std::string_view pop_name) const {
      return emp::FindEval(pops, [pop_name](const auto & p){ return p->GetName() == pop_name; });
    }
    Population & GetPopulation(size_t id) { return *pops[id]; }
    const Population & GetPopulation(size_t id) const { return *pops[id]; }
    Population & AddPopulation(const std::string & name, size_t pop_size=0);

    /// Move an organism from one position to another; kill anything that previously occupied
    /// the target position.
    void MoveOrg(OrgPosition from_pos, OrgPosition to_pos) {
      ClearOrgAt(to_pos);
      SwapOrgs(from_pos, to_pos);
    }

    /// Inject one or more copies of an organism and return the positions they were placed in.
    Collection Inject(Population & pop, const Organism & org, size_t copy_count=1);

    /// Inject this specific instance of an organism and turn over the pointer to be managed
    /// by MABE.  Return the position the organism was placed in.
    OrgPosition InjectInstance(Population & pop, emp::Ptr<Organism> org_ptr);
    
    /// Add one or more organisms of a specified type (provide the type name; the MABE controller
    /// will create instances of it.)  Returns the positions the organisms were placed.
    Collection Inject(Population & pop, const std::string & type_name, size_t copy_count=1);

    /// Add an organism of a specified type and population (provide names of both and they
    /// will be properly setup.)
    Collection InjectByName(const std::string & pop_name,
                             const std::string & type_name, 
                             size_t copy_count=1);

    /// Inject a copy of the provided organism at a specified position.
    void InjectAt(const Organism & org, OrgPosition pos) {
      emp_assert(pos.IsValid());
      emp::Ptr<Organism> inject_org = org.CloneOrganism();
      on_inject_ready_sig.Trigger(*inject_org, GetPopulation(pos.PopID()));
      AddOrgAt( inject_org, pos);
    }

    /// Give birth to one or more offspring; return position of last placed.
    /// Triggers 'before repro' signal on parent (once) and 'offspring ready' on each offspring.
    /// Regular signal triggers occur in AddOrgAt.
    Collection DoBirth(const Organism & org,
                       OrgPosition ppos,
                       Population & target_pop,
                       size_t birth_count=1,
                       bool do_mutations=true);

    Collection DoBirth(const Organism & org,
                       OrgPosition ppos,
                       OrgPosition target_pos,
                       bool do_mutations=true);


    /// A shortcut to DoBirth where only the parent position needs to be supplied;
    /// Return all offspring placed.
    Collection Replicate(OrgPosition ppos, Population & target_pop,
                          size_t birth_count=1, bool do_mutations=true) {
      return DoBirth(*ppos, ppos, target_pop, birth_count, do_mutations);
    }

    /// Remove all organisms from a population; does not change size.
    void ClearPop(Population & pop) {
      for (PopIterator pos = pop.begin(); pos != pop.end(); ++pos) ClearOrgAt(pos);
    }

    /// Resize a population while clearing all of the organisms in it.
    void EmptyPop(Population & pop, size_t new_size=0) {
      ClearPop(pop);
      MABEBase::ResizePop(pop, new_size);
    }

    /// Copy all of the organisms into a new population (clearing orgs already there)
    void CopyPop(const Population & from_pop, Population & to_pop) {
      EmptyPop(to_pop, from_pop.GetSize()); 
      for (size_t pos=0; pos < from_pop.GetSize(); ++pos) {
        if (from_pop.IsEmpty(pos)) continue;
        InjectAt(from_pop[pos], to_pop.IteratorAt(pos));
      }
    }

    /// Move all organisms from one population to another.
    void MoveOrgs(Population & from_pop, Population & to_pop, bool reset_to);

    /// Return a ramdom position from a desginated population.
    OrgPosition GetRandomPos(Population & pop) {
      emp_assert(pop.GetSize() > 0);
      return pop.IteratorAt( random.GetUInt(pop.GetSize()) );
    }

    /// Return a ramdom position from the population with the specified id.
    OrgPosition GetRandomPos(size_t pop_id) { return GetRandomPos(GetPopulation(pop_id)); }

    /// Return a ramdom position from a desginated population with a living organism in it.
    OrgPosition GetRandomOrgPos(Population & pop);

    /// Return a ramdom position of a living organism from the population with the specified id.
    OrgPosition GetRandomOrgPos(size_t pop_id) { return GetRandomOrgPos(GetPopulation(pop_id)); }


    // --- Collection Management ---

    std::string ToString(const mabe::Collection & collect) const { return collect.ToString(); }
    Collection ToCollection(const std::string & load_str);

    Collection GetAlivePopulation(size_t id) {
      Collection col(GetPopulation(id));
      col.RemoveEmpty();
      return col;
    }


    // --- Module Management ---

    /// Get the unique id of a module with the specified name.
    int GetModuleID(const std::string & mod_name) const {
      return emp::FindEval(modules, [mod_name](const auto & m){ return m->GetName() == mod_name; });
    }

    /// Get a reference to a module with the specified ID.
    const ModuleBase & GetModule(int id) const { return *modules[(size_t) id]; }
    ModuleBase & GetModule(int id) { return *modules[(size_t) id]; }

    /// Get a reference to a module with the specified name.
    const ModuleBase & GetModule(const std::string & mod_name) const {
      return *modules[(size_t) GetModuleID(mod_name)];
    }
    ModuleBase & GetModule(const std::string & mod_name) {
      return *modules[(size_t) GetModuleID(mod_name)];
    }

    /// Add a new module of the specified type.
    template <typename MOD_T, typename... ARGS>
    MOD_T & AddModule(ARGS &&... args) {
      auto new_mod = emp::NewPtr<MOD_T>(*this, std::forward<ARGS>(args)...);
      modules.push_back(new_mod);
      return *new_mod;
    }

    // --- Deal with Organism TRAITS ---
    TraitManager<ModuleBase> & GetTraitManager() { return trait_man; }

    /// Build a function to scan a data map, run a provided equation on its entries,
    /// and return the result.
    auto BuildTraitEquation(std::string equation) {
      equation = Preprocess(equation);
      auto dm_fun = dm_parser.BuildMathFunction(org_data_map, equation);
      return [dm_fun](const Organism & org){ return dm_fun(org.GetDataMap()); };
    }

    /// Scan an equation and return the names of all traits it is using.
    const std::set<std::string> & GetEquationTraits(const std::string & equation) {
      return dm_parser.GetNamesUsed(equation);
    }

    /// Build a function to scan a collection of organisms, reading the value for the given
    /// trait function from each, aggregating those values based on the trait_filter and returning
    /// the result as a string.  Output is a function in the form:  TO_T(const FROM_T &)
    template <typename FROM_T=Collection, typename TO_T=std::string>
    std::function<TO_T(const FROM_T &)> 
      BuildTraitSummary(std::string trait_fun, std::string trait_filter);

    /// Build a function that takes a trait equation, builds it, and runs it on a container.
    /// Output is a function in the form:  TO_T(const FROM_T &, trait_equation)
    template <typename FROM_T=Collection, typename TO_T=std::string> 
    auto BuildTraitFunction(const std::string & fun_type) {
      return [this,fun_type](FROM_T & pop, const std::string & trait_equation) {
        auto trait_fun = BuildTraitSummary<FROM_T,TO_T>(trait_equation, fun_type);
        return trait_fun(pop);
      };
    }


    // --- Manage configuration scope ---
    
    void SetupConfig();  ///< Setup config options for MABE, including for each module.    
    bool OK();           ///< Sanity checks for debugging


    // Checks for which modules are actively being triggered.
    using mod_ptr_t = emp::Ptr<ModuleBase>;
    bool BeforeUpdate_IsTriggered(mod_ptr_t mod) { return before_update_sig.cur_mod == mod; };
    bool OnUpdate_IsTriggered(mod_ptr_t mod) { return on_update_sig.cur_mod == mod; };
    bool BeforeRepro_IsTriggered(mod_ptr_t mod) { return before_repro_sig.cur_mod == mod; };
    bool OnOffspringReady_IsTriggered(mod_ptr_t mod) { return on_offspring_ready_sig.cur_mod == mod; };
    bool OnInjectReady_IsTriggered(mod_ptr_t mod) { return on_inject_ready_sig.cur_mod == mod; };
    bool BeforePlacement_IsTriggered(mod_ptr_t mod) { return before_placement_sig.cur_mod == mod; };
    bool OnPlacement_IsTriggered(mod_ptr_t mod) { return on_placement_sig.cur_mod == mod; };
    bool BeforeMutate_IsTriggered(mod_ptr_t mod) { return before_mutate_sig.cur_mod == mod; };
    bool OnMutate_IsTriggered(mod_ptr_t mod) { return on_mutate_sig.cur_mod == mod; };
    bool BeforeDeath_IsTriggered(mod_ptr_t mod) { return before_death_sig.cur_mod == mod; };
    bool BeforeSwap_IsTriggered(mod_ptr_t mod) { return before_swap_sig.cur_mod == mod; };
    bool OnSwap_IsTriggered(mod_ptr_t mod) { return on_swap_sig.cur_mod == mod; };
    bool BeforePopResize_IsTriggered(mod_ptr_t mod) { return before_pop_resize_sig.cur_mod == mod; };
    bool OnPopResize_IsTriggered(mod_ptr_t mod) { return on_pop_resize_sig.cur_mod == mod; };
    bool OnError_IsTriggered(mod_ptr_t mod) { return on_error_sig.cur_mod == mod; };
    bool OnWarning_IsTriggered(mod_ptr_t mod) { return on_warning_sig.cur_mod == mod; };
    bool BeforeExit_IsTriggered(mod_ptr_t mod) { return before_exit_sig.cur_mod == mod; };
    bool OnHelp_IsTriggered(mod_ptr_t mod) { return on_help_sig.cur_mod == mod; };
  };


  // ========================== OUT-OF-CLASS DEFINITIONS! ==========================

  // ---------------- PRIVATE MEMBER FUNCTIONS -----------------

  /// Print information on how to run the software.
  void MABE::ShowHelp() {
    std::cout << "MABE v" << VERSION << "\n";
    on_help_sig.Trigger();

    if (help_topic == "") {
      std::cout << "Usage: " << args[0] << " [options]\n"
                << "Options:\n";
      for (const auto & cur_arg : arg_set) {
        std::cout << "  " << cur_arg.flag << " " << cur_arg.args
                  << " : " << cur_arg.desc << " (or " << cur_arg.name << ")"
                  << std::endl;
      }
    }
    else {
      auto & mod_map = GetModuleMap();
      std::cout << "TOPIC: " << help_topic << std::endl;
      if (emp::Has(mod_map, help_topic)) {
        const auto & info = mod_map[help_topic];
        std::cout << "--- MABE Module ---\n"
                  << "Description: " << info.desc << "\n";
      }
      else {
        std::cout << "Unknown keyword.\n";
      }

    }
    exit_now = true;
  }

  /// List all of the available modules included in the current compilation.
  void MABE::ShowModules() {
    std::cout << "MABE v" << VERSION << "\n"
              << "Active modules:\n";
    // for (auto mod_ptr : modules) {
    //   std::cout << "  " << mod_ptr->GetName() << " : " << mod_ptr->GetDesc() << "\n";
    // }          
    std::cout << "Available modules:\n";
    for (auto & [type_name,mod] : GetModuleMap()) {
      std::cout << "  " << type_name << " : " << mod.desc << "\n";
    }          
    exit_now = true;;
  }

  void MABE::ProcessArgs() {
    arg_set.emplace_back("--filename", "-f", "[filename...] ", "Filenames of configuration settings",
      [this](const emp::vector<std::string> & in){ config_filenames = in; } );
    arg_set.emplace_back("--generate", "-g", "[filename]    ", "Generate a new output file",
      [this](const emp::vector<std::string> & in) {
        if (in.size() != 1) {
          std::cout << "'--generate' must be followed by a single filename.\n";
          exit_now = true;
        } else {
          // MABE config files can be generated FROM a *.gen file, typically creating a *.mabe
          // file.  If output file is *.gen assume an error. (for now; override should be allowed)
          if (in[0].size() > 4 && in[0].substr(in[0].size()-4) == ".gen") {
            error_man.AddError("Error: generated file ", in[0], " not allowed to be *.gen; typically should end in *.mabe.");
            exit_now = true;
          }
          else gen_filename = in[0];
        }
      });
    arg_set.emplace_back("--help", "-h", "              ", "Help; print command-line options for MABE",
      [this](const emp::vector<std::string> & in){
        show_help = true;
        if (in.size()) help_topic = in[0];
      });
    arg_set.emplace_back("--modules", "-m", "              ", "Module list",
      [this](const emp::vector<std::string> &){ ShowModules(); } );
    arg_set.emplace_back("--set", "-s", "[param=value] ", "Set specified parameter",
      [this](const emp::vector<std::string> & in){
        emp::Append(config_settings, in);
        config_settings.push_back(";"); // Extra semi-colon so not needed on command line.
      });
    arg_set.emplace_back("--version", "-v", "              ", "Version ID of MABE",
      [this](const emp::vector<std::string> &){
        std::cout << "MABE v" << VERSION << "\n";
        exit_now = true;
      });
    arg_set.emplace_back("--verbose", "-+", "              ", "Output extra setup info",
      [this](const emp::vector<std::string> &){ verbose = true; } );

    // Scan through all input argument positions.
    for (size_t pos = 1; pos < args.size(); pos++) {
      // Match the input argument to the function to call.
      bool found = false;
      for (auto & cur_arg : arg_set) {
        // If we have a match...
        if (args[pos] == cur_arg.name || args[pos] == cur_arg.flag) {
          // ...collect all of the options associated with this match.
          emp::vector<std::string> option_args;
          // We want args until we run out or hit another option.
          while (pos+1 < args.size() && args[pos+1][0] != '-') {
            option_args.push_back(args[++pos]);
          }

          // And call the function!
          cur_arg.action(option_args);
          found = true;
          break;            
        }
      }
      if (found == false) {
        std::cout << "Error: unknown command line argument '" << args[pos] << "'." << std::endl;
        show_help = true;
        break;
      }
    }

    if (show_help) ShowHelp();
  }

  /// As part of the main Setup(), run SetupModule() method on each module we've loaded.
  void MABE::Setup_Modules() {
    // Allow the user-defined module SetupModule() member functions run.  These are
    // typically used for any internal setup needed by modules are the configuration is
    // complete.
    for (emp::Ptr<ModuleBase> mod_ptr : modules) mod_ptr->SetupModule();
  }

  /// As part of the main Setup(), load in all of the organism traits that modules need to
  /// read or write and make sure that there aren't any conflicts.
  void MABE::Setup_Traits() {
    Verbose("Analyzing configuration of ", trait_man.GetSize(), " traits.");

    trait_man.Verify(verbose);            // Make sure modules are accessing traits consistently
    trait_man.RegisterAll(org_data_map);  // Load in all of the traits to the DataMap
    org_data_map.LockLayout();            // Freeze the data map into its current state

    // Alert modules (especially org managers) to the final set of traits.
    for (emp::Ptr<ModuleBase> mod_ptr : modules) {
      mod_ptr->SetupDataMap(org_data_map);
    }
  }

  /// Link signals to the modules that implement responses to those signals.
  void MABE::UpdateSignals() {
    // Clear all module vectors.
    for (auto modv : sig_ptrs) modv->resize(0);

    // Loop through each module to update its signals.
    for (emp::Ptr<ModuleBase> mod_ptr : modules) {
      // For the current module, loop through all of the signals.
      for (size_t sig_id = 0; sig_id < sig_ptrs.size(); sig_id++) {
        if (mod_ptr->has_signal[sig_id]) sig_ptrs[sig_id]->push_back(mod_ptr);
      }
    }

    // Now that we have scanned the signals, we can turn off the rescan flag.
    rescan_signals = false;
  }

  /// Find any instances of ${X} and eval the X.
  std::string MABE::Preprocess(const std::string & in_string) {
    std::string out_string = in_string;

    // Seek out instances of "${" to indicate the start of pre-processing.
    for (size_t i = 0; i < out_string.size(); ++i) {
      if (out_string[i] != '$') continue;   // Replacement tag must start with a '$'.
      if (out_string.size() <= i+2) break;  // Not enough room for a replacement tag.
      if (out_string[i+1] == '$') {         // Compress two $$ into on $
        out_string.erase(i,1);
        continue;
      }
      if (out_string[i+1] != '{') continue; // Eval must be surrounded by braces.

      // If we made it this far, we have a starting match!
      size_t end_pos = emp::find_paren_match(out_string, i+1, '{', '}', false);
      if (end_pos == i+1) return out_string;  // No end brace found!  @CAO -- exception here?
      const std::string replacement_text =
        config.Execute(emp::view_string_range(out_string, i+2, end_pos));
      out_string.replace(i, end_pos-i+1, replacement_text);

      i += replacement_text.size(); // Continue from the end point...
    }

    return out_string;
  }

  void MABE::Deprecate(const std::string & old_name, const std::string & new_name) {
    auto dep_fun = [this,old_name,new_name](const emp::vector<emp::Ptr<emplode::Symbol>> &){
        std::cerr << "Function '" << old_name << "' deprecated; use '" << new_name << "'\n";
        exit_now = true;
        return 0;      
      };

    config.AddFunction(old_name, dep_fun, std::string("Deprecated.  Use: ") + new_name);
  }

  // ---------------- PUBLIC MEMBER FUNCTIONS -----------------


  MABE::MABE(int argc, char* argv[])
    : error_man( [this](const std::string & msg){ on_error_sig.Trigger(msg); },
                 [this](const std::string & msg){ on_warning_sig.Trigger(msg); } )
    , trait_man(error_man)
    , args(emp::cl::args_to_strings(argc, argv))
  {
    // Setup "Population" as a type in the config file.
    auto pop_init_fun = [this](const std::string & name) { return &AddPopulation(name); };
    auto pop_copy_fun = [this](const EmplodeType & from, EmplodeType & to) {
      emp::Ptr<const Population> from_pop = dynamic_cast<const Population *>(&from);
      emp::Ptr<Population> to_pop = dynamic_cast<Population *>(&to);
      if (!from_pop || !to_pop) return false; // Wrong type!
      CopyPop(*from_pop, *to_pop);            // Do the actual copy.
      return true;
    };
    auto & pop_type = config.AddType<Population>("Population", "Collection of organisms",
                                                 pop_init_fun, pop_copy_fun);

    // Setup "Collection" as another config type.
    auto & collect_type = config.AddType<Collection>("OrgList", "Collection of organism pointers");

    // 'INJECT' allows a user to add an organism to a population; returns collection of added orgs.
    std::function<Collection(Population &, const std::string &, size_t)> inject_fun =
      [this](Population & pop, const std::string & org_type_name, size_t count) {
        return Inject(pop, org_type_name, count);
      };
    pop_type.AddMemberFunction("INJECT", inject_fun,
      "Inject organisms into population.  Args: org_name, org_count.  Return: OrgList of injected orgs.");
    pop_type.AddMemberFunction("REPLACE_WITH",
      [this](Population & to_pop, Population & from_pop){ MoveOrgs(from_pop, to_pop, true); return 0; },
      "Move all organisms organisms from another population, removing current orgs." );
    pop_type.AddMemberFunction("APPEND",
      [this](Population & to_pop, Population & from_pop){ MoveOrgs(from_pop, to_pop, false); return 0; },
      "Move all organisms organisms from another population, adding after current orgs." );

    pop_type.AddMemberFunction("TRAIT", BuildTraitFunction<Population,std::string>("0"),
      "Return the value of the provided trait for the first organism");
    pop_type.AddMemberFunction("CALC_RICHNESS", BuildTraitFunction<Population,double>("richness"),
      "Count the number of distinct values of a trait (or equation).");
    pop_type.AddMemberFunction("CALC_MODE", BuildTraitFunction<Population,std::string>("mode"),
      "Identify the most common value of a trait (or equation).");
    pop_type.AddMemberFunction("CALC_MEAN", BuildTraitFunction<Population,double>("mean"),
      "Calculate the average value of a trait (or equation).");
    pop_type.AddMemberFunction("CALC_MIN", BuildTraitFunction<Population,double>("min"),
      "Find the smallest value of a trait (or equation).");
    pop_type.AddMemberFunction("CALC_MAX", BuildTraitFunction<Population,double>("max"),
      "Find the largest value of a trait (or equation).");
    pop_type.AddMemberFunction("ID_MIN", BuildTraitFunction<Population,double>("min_id"),
      "Find the index of the smallest value of a trait (or equation).");
    pop_type.AddMemberFunction("ID_MAX", BuildTraitFunction<Population,double>("max_id"),
      "Find the index of the largest value of a trait (or equation).");
    pop_type.AddMemberFunction("CALC_MEDIAN", BuildTraitFunction<Population,double>("median"),
      "Find the 50-percentile value of a trait (or equation).");
    pop_type.AddMemberFunction("CALC_VARIANCE", BuildTraitFunction<Population,double>("variance"),
      "Find the variance of the distribution of values of a trait (or equation).");
    pop_type.AddMemberFunction("CALC_STDDEV", BuildTraitFunction<Population,double>("stddev"),
      "Find the variance of the distribution of values of a trait (or equation).");
    pop_type.AddMemberFunction("CALC_SUM", BuildTraitFunction<Population,double>("sum"),
      "Add up the total value of a trait (or equation).");
    pop_type.AddMemberFunction("CALC_ENTROPY", BuildTraitFunction<Population,double>("entropy"),
      "Determine the entropy of values for a trait (or equation).");
    pop_type.AddMemberFunction("FIND_MIN",
      [this](Population & pop, const std::string & trait_equation) -> Collection {
        auto trait_fun = BuildTraitSummary<Population,double>(trait_equation, "min_id");
        return pop.IteratorAt(trait_fun(pop)).AsPosition();
      },
      "Produce OrgList with just the org with the minimum value of the provided function.");
    pop_type.AddMemberFunction("FIND_MAX",
      [this](Population & pop, const std::string & trait_equation) -> Collection {
        auto trait_fun = BuildTraitSummary<Population,double>(trait_equation, "max_id");
        return pop.IteratorAt(trait_fun(pop)).AsPosition();
      },
      "Produce OrgList with just the org with the minimum value of the provided function.");
    pop_type.AddMemberFunction("FILTER",
      [this](Population & pop, const std::string & trait_equation) -> Collection {
        auto filter = BuildTraitEquation(trait_equation);
        Collection out_collect;
        for (auto it = pop.begin(); it != pop.end(); ++it) {
          if (filter(*it)) out_collect.Insert(it);
        }
        return out_collect;
      },
      "Produce OrgList with just the orgs that pass through the filter criteria.");

    collect_type.AddMemberFunction("TRAIT", BuildTraitFunction<Collection,std::string>("0"),
      "Return the value of the provided trait for the first organism");
    collect_type.AddMemberFunction("CALC_RICHNESS", BuildTraitFunction<Collection,double>("richness"),
      "Count the number of distinct values of a trait (or equation).");
    collect_type.AddMemberFunction("CALC_MODE", BuildTraitFunction<Collection,std::string>("mode"),
      "Identify the most common value of a trait (or equation).");
    collect_type.AddMemberFunction("CALC_MEAN", BuildTraitFunction<Collection,double>("mean"),
      "Calculate the average value of a trait (or equation).");
    collect_type.AddMemberFunction("CALC_MIN", BuildTraitFunction<Collection,double>("min"),
      "Find the smallest value of a trait (or equation).");
    collect_type.AddMemberFunction("CALC_MAX", BuildTraitFunction<Collection,double>("max"),
      "Find the largest value of a trait (or equation).");
    collect_type.AddMemberFunction("ID_MIN", BuildTraitFunction<Collection,double>("min_id"),
      "Find the index of the smallest value of a trait (or equation).");
    collect_type.AddMemberFunction("ID_MAX", BuildTraitFunction<Collection,double>("max_id"),
      "Find the index of the largest value of a trait (or equation).");
    collect_type.AddMemberFunction("CALC_MEDIAN", BuildTraitFunction<Collection,double>("median"),
      "Find the 50-percentile value of a trait (or equation).");
    collect_type.AddMemberFunction("CALC_VARIANCE", BuildTraitFunction<Collection,double>("variance"),
      "Find the variance of the distribution of values of a trait (or equation).");
    collect_type.AddMemberFunction("CALC_STDDEV", BuildTraitFunction<Collection,double>("stddev"),
      "Find the variance of the distribution of values of a trait (or equation).");
    collect_type.AddMemberFunction("CALC_SUM", BuildTraitFunction<Collection,double>("sum"),
      "Add up the total value of a trait (or equation).");
    collect_type.AddMemberFunction("CALC_ENTROPY", BuildTraitFunction<Collection,double>("entropy"),
      "Determine the entropy of values for a trait (or equation).");
    collect_type.AddMemberFunction("FIND_MIN",
      [this](Collection & collect, const std::string & trait_equation) -> Collection {
        auto trait_fun = BuildTraitSummary<Collection,double>(trait_equation, "min_id");
        return collect.IteratorAt(trait_fun(collect)).AsPosition();
      },
      "Produce OrgList with just the org with the minimum value of the provided function.");
    collect_type.AddMemberFunction("FIND_MAX",
      [this](Collection & collect, const std::string & trait_equation) -> Collection {
        auto trait_fun = BuildTraitSummary<Collection,double>(trait_equation, "max_id");
        return collect.IteratorAt(trait_fun(collect)).AsPosition();
      },
      "Produce OrgList with just the org with the minimum value of the provided function.");

    // Setup all known modules as available types in the config file.
    for (auto & [type_name,mod] : GetModuleMap()) {
      auto mod_init_fun = [this,mod=&mod](const std::string & name) -> emp::Ptr<emplode::EmplodeType> {
        return mod->obj_init_fun(*this,name);
      };
      auto & type_info = config.AddType(type_name, mod.desc, mod_init_fun, nullptr, mod.type_id);
      mod.type_init_fun(type_info);  // Setup functions for this module.
    }


    // ------ DEPRECATED FUNCTION NAMES ------
    Deprecate("EVAL", "EXEC");
    Deprecate("exit", "EXIT");
    Deprecate("inject", "INJECT");
    Deprecate("print", "PRINT");

    // Add other built-in functions to the config file.
    config.AddFunction("EXIT", [this](){ exit_now = true; return 0; }, "Exit from this MABE run.");
    config.AddFunction("GET_UPDATE", [this](){ return GetUpdate(); }, "Get current update.");

    std::function<std::string(const std::string &)> preprocess_fun =
      [this](const std::string & str) { return Preprocess(str); };
    config.AddFunction("PP", preprocess_fun, "Preprocess a string (replacing any ${...} with result.)");


    // --- TRAIT-BASED FUNCTIONS ---

    std::function<std::string(const std::string &, std::string)> trait_string_fun =
      [this](const std::string & target, std::string trait_filter) {
        std::string trait_name = emp::string_pop(trait_filter,':');
        auto fun = BuildTraitSummary(trait_name, trait_filter);
        return fun( ToCollection(target) );
      };
    config.AddFunction("TRAIT_STRING", trait_string_fun, "Collect information about a specified trait.");

    std::function<double(const std::string &, std::string)> trait_value_fun =
      [this](const std::string & target, std::string trait_filter) {
        std::string trait_name = emp::string_pop(trait_filter,':');
        auto fun = BuildTraitSummary(trait_name, trait_filter);
        return emp::from_string<double>(fun( ToCollection(target) ));
      };
    config.AddFunction("TRAIT_VALUE", trait_value_fun, "Collect information about a specified trait.");

    // Add in built-in event triggers; these are used to indicate when events should happen.
    config.AddEventType("start");   // Triggered at the beginning of a run.
    config.AddEventType("update");  // Tested every update.
  }

  bool MABE::Setup() {
    SetupConfig();                   // Load all of the parameters needed by modules, etc.
    ProcessArgs();                   // Deal with command-line inputs.

    // Sometimes command-line arguments will require an immediate exit (such as after '--help')
    if (exit_now) return false;

    // If configuration filenames have been specified, load each of them in order.
    if (config_filenames.size()) {
      std::cout << "Loading file(s): " << emp::to_quoted_list(config_filenames) << std::endl;
      config.Load(config_filenames);   // Load files
    }

    if (config_settings.size()) {
      std::cout << "Loading command-line settings." << std::endl;
      config.LoadStatements(config_settings, "command-line settings");      
    }

    // If we are writing a file, do so and then exit.
    if (gen_filename != "") {
      std::cout << "Generating file '" << gen_filename << "'." << std::endl;
      config.Write(gen_filename);
      exit_now = true;
    }

    // If any of the inital flags triggered an 'exit_now', do so.
    if (exit_now) return false;

    // Allow traits to be linked.
    trait_man.Unlock();

    Setup_Modules();        // Run SetupModule() on each module for linking traits or other setup.
    Setup_Traits();         // Make sure module traits do not clash.

    UpdateSignals();        // Setup the appropriate modules to be linked with each signal.

    error_man.Activate();

    // Only return success if there were no errors.
    return (error_man.GetNumErrors() == 0);
  }

  /// Update MABE world.
  void MABE::Update(size_t num_updates) {
    if (update == 0) config.TriggerEvents("start");
    for (size_t ud = 0; ud < num_updates && !exit_now; ud++) {
      emp_assert(OK(), update);                   // In debug mode, keep checking MABE integrity
      if (rescan_signals) UpdateSignals();        // If we have reason to, update module signals
      before_update_sig.Trigger(update);          // Signal that a new update is about to begin
      update++;                                   // Increment 'update' to start new update
      on_update_sig.Trigger(update);              // Signal all modules about the new update
      config.UpdateEventValue("update", update);  // Trigger any updated-based events
    }
  }

  /// Setup an organism as a placeholder for all "empty" positions in the population.
  template <typename EMPTY_MANAGER_T>
  void MABE::SetupEmpty() {
    if (empty_org) empty_org.Delete();  // If we already have an empty organism, replace it.
    auto & empty_manager =
      AddModule<EMPTY_MANAGER_T>("EmptyOrg", "Manager for all 'empty' organisms in any population.");
    empty_manager.SetBuiltIn();         // Don't write the empty manager to config.

    empty_org = empty_manager.template Make<Organism>();
  }

  /// New populations must be given a name and an optional size.
  Population & MABE::AddPopulation(const std::string & name, size_t pop_size) {
    int pop_id = (int) pops.size();
    emp::Ptr<Population> new_pop = emp::NewPtr<Population>(name, pop_id, pop_size, empty_org);
    pops.push_back(new_pop);

    // Setup default placement functions for the new population.
    new_pop->SetPlaceBirthFun( [this,new_pop](Organism & /*org*/, OrgPosition /*ppos*/) {
      return PushEmpty(*new_pop);
    });
    new_pop->SetPlaceInjectFun( [this,new_pop](Organism & /*org*/) {
      return PushEmpty(*new_pop);
    });
    new_pop->SetFindNeighborFun( [this,new_pop](OrgPosition pos) {
      if (pos.IsInPop(*new_pop)) return OrgPosition(); // Wrong pop!  No neighbor.
      // Return a random org since no structure to population.
      return OrgPosition(new_pop, GetRandom().GetUInt(new_pop->GetSize()));
    });

    return *new_pop;
  }

  /// Inject a copy of the provided organism and return the position it was placed in;
  /// if more than one is added, return the position of the final injection.
  Collection MABE::Inject(Population & pop, const Organism & org, size_t copy_count) {
    emp_assert(org.GetDataMap().SameLayout(org_data_map));
    Collection placement_set;
    for (size_t i = 0; i < copy_count; i++) {
      emp::Ptr<Organism> inject_org = org.CloneOrganism();
      on_inject_ready_sig.Trigger(*inject_org, pop);
      OrgPosition pos = pop.PlaceInject(*inject_org);
      if (pos.IsValid()) {
        AddOrgAt( inject_org, pos);
        placement_set.Insert(pos);
      } else {
        inject_org.Delete();          
        error_man.AddError("Invalid position; failed to inject organism ", i, "!");
      }
    }
    return placement_set;
  }

  /// Inject this specific instance of an organism and turn over the pointer to be managed
  /// by MABE.  Teturn the position the organism was placed in.
  OrgPosition MABE::InjectInstance(Population & pop, emp::Ptr<Organism> org_ptr) {
    emp_assert(org_ptr->GetDataMap().SameLayout(org_data_map));
    on_inject_ready_sig.Trigger(*org_ptr, pop);
    OrgPosition pos = pop.PlaceInject(*org_ptr);
    if (pos.IsValid()) AddOrgAt( org_ptr, pos);
    else {
      org_ptr.Delete();          
      error_man.AddError("Invalid position; failed to inject organism!");
    }
    return pos;
  }
  

  /// Add an organsim of a specified type to the world (provide the type name and the
  /// MABE controller will create instances of it.)  Returns the position of the last
  /// organism placed.
  Collection MABE::Inject(Population & pop, const std::string & type_name, size_t copy_count) {
    Verbose("Injecting ", copy_count, " orgs of type '", type_name,
            "' into population ", pop.GetID());

    auto & org_manager = GetModule(type_name);            // Look up type of organism.
    Collection placement_set;                             // Track set of positions placed.
    for (size_t i = 0; i < copy_count; i++) {             // Loop through, injecting each instance.
      auto org_ptr = org_manager.Make<Organism>(random);  // ...Build an org of this type.
      OrgPosition pos = InjectInstance(pop, org_ptr);     // ...Inject it into the population.
      placement_set.Insert(pos);                          // ...Record the position.
    }

    return placement_set;                                 // Return last position injected.
  }

  /// Add an organism of a specified type and population (provide names of both and they
  /// will be properly setup.)
  Collection MABE::InjectByName(const std::string & pop_name,
                                const std::string & type_name, 
                                size_t copy_count) {      
    int pop_id = GetPopID(pop_name);
    if (pop_id == -1) {
      error_man.AddError("Invalid population name used in inject: ",
                         "org_type= '", type_name, "'; ",
                         "pop_name= '", pop_name, "'; ",
                         "copy_count=", copy_count);
    }
    Population & pop = GetPopulation(pop_id);
    return Inject(pop, type_name, copy_count); // Inject the organisms.
  }

  /// Give birth to one or more offspring; return position of last placed.
  /// Triggers 'before repro' signal on parent (once) and 'offspring ready' on each offspring.
  /// Regular signal triggers occur in AddOrgAt.
  Collection MABE::DoBirth(const Organism & org,
                           OrgPosition ppos,
                           Population & target_pop,
                           size_t birth_count,
                           bool do_mutations) {
    emp_assert(org.IsEmpty() == false);             // Empty cells cannot reproduce.
    before_repro_sig.Trigger(ppos);                 // Signal reproduction event.
    OrgPosition pos;                                // Position of each offspring placed.
    emp::Ptr<Organism> new_org;
    Collection birth_list;                          // Track positions of all offspring.
    for (size_t i = 0; i < birth_count; i++) {      // Loop through offspring, adding each
      new_org = do_mutations ? org.MakeOffspringOrganism(random) : org.CloneOrganism();

      // Alert modules that offspring is ready, then find its birth position.
      on_offspring_ready_sig.Trigger(*new_org, ppos, target_pop);
      pos = target_pop.PlaceBirth(*new_org, ppos);

      // If this placement is valid, do so.  Otherwise delete the organism.
      if (pos.IsValid()) {
        AddOrgAt(new_org, pos, ppos);
        birth_list.Insert(pos);
      }
      else new_org.Delete();
    }
    return birth_list;
  }

  Collection MABE::DoBirth(const Organism & org,
                           OrgPosition ppos,
                           OrgPosition target_pos,
                           bool do_mutations) {
    emp_assert(org.IsEmpty() == false);  // Empty cells cannot reproduce.
    emp_assert(target_pos.IsValid());    // Target positions must already be valid.

    before_repro_sig.Trigger(ppos);
    emp::Ptr<Organism> new_org = do_mutations ? org.MakeOffspringOrganism(random) : org.CloneOrganism();
    on_offspring_ready_sig.Trigger(*new_org, ppos, target_pos.Pop());

    AddOrgAt(new_org, target_pos, ppos);

    return target_pos;
  }

  void MABE::MoveOrgs(Population & from_pop, Population & to_pop, bool reset_to) {
    // Get the starting point for the new organisms to ove to.
    Population::iterator_t it_to = reset_to ? to_pop.begin() : to_pop.end();

    // Prepare the "to" population before moving the new organisms in.
    if (reset_to) EmptyPop(to_pop, from_pop.GetSize());   // Clear out the population.
    else ResizePop(to_pop, to_pop.GetSize() + from_pop.GetSize());

    // Move the organisms over
    for (auto it_from = from_pop.begin(); it_from != from_pop.end(); ++it_from, ++it_to) {
      if (it_from.IsOccupied()) MoveOrg(it_from, it_to);
    }

    // Clear out the from population now that we're done with it.
    EmptyPop(from_pop, 0);
  }

  /// Return a ramdom position from a desginated population with a living organism in it.
  OrgPosition MABE::GetRandomOrgPos(Population & pop) {
    emp_assert(pop.GetNumOrgs() > 0, "GetRandomOrgPos cannot be called if there are no orgs.");
    OrgPosition pos = GetRandomPos(pop);
    while (pos.IsEmpty()) pos = GetRandomPos(pop);
    return pos;
  }


  // --- Collection Management ---

  Collection MABE::ToCollection(const std::string & load_str) {
    Collection out;
    auto slices = emp::view_slices(load_str, ',');
    for (auto name : slices) {
      int pop_id = GetPopID(name);
      if (pop_id == -1) error_man.AddError("Unknown population: ", name);
      else out.Insert(GetPopulation(pop_id));
    }
    return out;
  }


  /// Build a function to scan a collection of organisms, reading the value for the given
  /// trait_name from each, aggregating those values based on the trait_filter and returning
  /// the result as a string.
  ///
  ///  trait_filter option are:
  ///   <none>      : Default to the value of the trait for the first organism in the collection.
  ///   [ID]        : Value of this trait for the organism at the given index of the collection.
  ///   [OP][VALUE] : Count how often this value has the [OP] relationship with [VALUE].
  ///                  [OP] can be ==, !=, <, >, <=, or >=
  ///                  [VALUE] can be any numeric value
  ///   [OP][TRAIT] : Count how often this trait has the [OP] relationship with [TRAIT]
  ///                  [OP] can be ==, !=, <, >, <=, or >=
  ///                  [TRAIT] can be any other trait name
  ///   unique      : Return the number of distinct value for this trait (alias="richness").
  ///   mode        : Return the most common value in this collection (aliases="dom","dominant").
  ///   min         : Return the smallest value of this trait present.
  ///   max         : Return the largest value of this trait present.
  ///   ave         : Return the average value of this trait (alias="mean").
  ///   median      : Return the median value of this trait.
  ///   variance    : Return the variance of this trait.
  ///   stddev      : Return the standard deviation of this trait.
  ///   sum         : Return the summation of all values of this trait (alias="total")
  ///   entropy     : Return the Shannon entropy of this value.
  ///   :trait      : Return the mutual information with another provided trait.

  template <typename FROM_T, typename TO_T>
  std::function<TO_T(const FROM_T &)> MABE::BuildTraitSummary(
    std::string trait_fun,
    std::string trait_filter
  ) {
    static_assert( std::is_same<FROM_T,Collection>() ||  std::is_same<FROM_T,Population>(),
                   "BuildTraitSummary FROM_T must be Collection or Population." );
    static_assert( std::is_same<TO_T,double>() ||  std::is_same<TO_T,std::string>(),
                   "BuildTraitSummary TO_T must be double or std::string." );

    // Pre-process the trait function to allow for use of regular config variables.
    trait_fun = Preprocess(trait_fun);

    // The trait input has two components:
    // (1) the trait (or trait function) and
    // (2) how to calculate the trait SUMMARY, such as min, max, ave, etc.

    // If we have a single trait, we may want to use a string type.
    if (emp::is_identifier(trait_fun)           // If we have a single trait...
        && org_data_map.HasName(trait_fun)      // ...and it's in the data map...
        && !org_data_map.IsNumeric(trait_fun)   // ...and it's not numeric...
    ) {
      size_t trait_id = org_data_map.GetID(trait_fun);
      emp::TypeID result_type = org_data_map.GetType(trait_id);

      auto get_fun = [trait_id, result_type](const Organism & org) {
        return emp::to_literal( org.GetTraitAsString(trait_id, result_type) );
      };
      auto fun = emp::BuildCollectFun<std::string, Collection>(trait_filter, get_fun);

      // Go through all combinations of TO/FROM to return the correct types.
      if constexpr (std::is_same<FROM_T,Population>() && std::is_same<TO_T,double>()) {
        return [fun](const FROM_T & p){ return emp::from_string<double>(fun( Collection(p) )); };
      }
      else if constexpr (std::is_same<FROM_T,Collection>() && std::is_same<TO_T,double>()) {
        return [fun](const FROM_T & c){ return emp::from_string<double>(fun(c)); };
      }
      else if constexpr (std::is_same<FROM_T,Population>() && std::is_same<TO_T,std::string>()) {
        return [fun](const FROM_T & p){ return fun( Collection(p) ); };
      }
      else return fun;
    }

    // If we made it here, we are numeric.
    auto get_fun = BuildTraitEquation(trait_fun);
    auto fun = emp::BuildCollectFun<double, Collection>(trait_filter, get_fun);

    // If we don't have a fun, we weren't able to build an aggregation function.
    if (!fun) {
      error_man.AddError("Unknown trait filter '", trait_filter, "' for trait '", trait_fun, "'.");
      return [](const FROM_T &){ return TO_T(); };
    }

    // Go through all combinations of TO/FROM to return the correct types.
    // @CAO need to adjust BuildCollectFun so that it returns correct type; not always string.
    if constexpr (std::is_same<FROM_T,Population>() && std::is_same<TO_T,double>()) {
      return [fun](const Population & p){ return emp::from_string<double>(fun( Collection(p) )); };
    }
    else if constexpr (std::is_same<FROM_T,Collection>() && std::is_same<TO_T,double>()) {
      return [fun](const Collection & c){ return emp::from_string<double>(fun(c)); };
    }
    else if constexpr (std::is_same<FROM_T,Population>() && std::is_same<TO_T,std::string>()) {
      return [fun](const Population & p){ return fun( Collection(p) ); };
    }
    else return fun;
  }


  void MABE::SetupConfig() {
    // Setup main MABE variables.
    auto & root_scope = config.GetSymbolTable().GetRootScope();
    root_scope.LinkFuns<int>("random_seed",
                             [this](){ return random.GetSeed(); },
                             [this](int seed){ random.ResetSeed(seed); },
                             "Seed for random number generator; use 0 to base on time.");
  }


  bool MABE::OK() {
    bool result = true;
    for (auto mod_ptr : modules) result &= mod_ptr->OK(); // Ensure modules are okay.
    for (auto pop_ptr : pops) result &= pop_ptr->OK();    // Ensure populations are okay.
    return result;
  }


}

#endif
