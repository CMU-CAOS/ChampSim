.. _Tutorial:

================================================
Tutorial: Creating New Modules
================================================

This tutorial walks through creating modules for ChampSim from scratch, with detailed
line-by-line commentary. By the end, you will understand:

* How to create a prefetcher
* How to create a replacement policy
* How to create a branch predictor
* How the ``ModuleBuilder`` parameter system works
* How to define an entirely new module interface

.. contents:: Contents
   :local:
   :depth: 2

.. _Tutorial_Prefetcher:

------------------------------------------
Part 1: Creating a Prefetcher
------------------------------------------

We will build a stride prefetcher from scratch. A stride prefetcher tracks the
addresses accessed by each instruction pointer (IP). When it detects a repeating
stride pattern, it prefetches ahead in that pattern.

Step 1: Create the Directory
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Each module lives in its own directory under the interface directory it implements.
Prefetchers go under ``prefetcher/``::

    mkdir prefetcher/my_stride

The Makefile automatically discovers ``.cc`` files under ``prefetcher/``,
``replacement/``, ``branch/``, and ``btb/``, so there is no build system
configuration needed.

Step 2: Write the Header
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create ``prefetcher/my_stride/my_stride.h``:

.. code-block:: cpp
    :linenos:

    #ifndef MY_STRIDE_H
    #define MY_STRIDE_H

    // <map> gives us std::map for our tracking table.
    #include <map>

    // "address.h" provides champsim::address and champsim::block_number — ChampSim's
    // type-safe address types.  Raw uint64_t addresses are error-prone (byte vs block
    // vs page ambiguity), so ChampSim wraps them in distinct types.
    #include "address.h"

    // "modules.h" is the central module-system header.  It provides:
    //   - champsim::modules::prefetcher   (the interface we inherit from)
    //   - champsim::modules::ModuleBuilder (the constructor parameter)
    //   - champsim::modules::cache_module  (the parent type for prefetchers)
    //   - register_module<T>  (used to register our implementation)
    #include "modules.h"

    // --------------------------------------------------------------------------
    // my_stride — A stride prefetcher with configurable degree.
    //
    // Inherits from champsim::modules::prefetcher, which itself inherits from
    // module_base<prefetcher, cache_module>.  That template establishes two things:
    //   1. Prefetchers are attached to a cache_module (the "parent").
    //   2. The static registration/factory infrastructure (register_module,
    //      create_instance) is scoped to the prefetcher interface.
    // --------------------------------------------------------------------------
    struct my_stride : public champsim::modules::prefetcher {

        // --- Per-IP tracking state ---
        // We track each instruction pointer's last cache-line address and the
        // stride between the two most recent accesses.  When two consecutive
        // strides match, we have a pattern to prefetch.
        struct tracker_entry {
            champsim::block_number last_addr{};                 // cache-line address of last access
            champsim::block_number::difference_type stride{};   // last observed stride (in cache lines)
        };

        // --- Configurable fields ---
        // "degree" controls how many cache lines ahead we prefetch once a stride
        // pattern is detected.  It defaults to 3 but can be overridden from JSON.
        int degree = 3;

        // --- Parent cache pointer ---
        // We store a pointer to the parent cache_module so we can query runtime
        // state like MSHR occupancy.  This pointer is obtained from the
        // ModuleBuilder in the constructor.
        champsim::modules::cache_module* cache_ = nullptr;

        // --- Tracking table ---
        // Maps instruction pointers to their tracker entries.  Using std::map
        // here for simplicity; a real design might use a fixed-size LRU table
        // (see champsim::msl::lru_table in ip_stride).
        std::map<champsim::address, tracker_entry> table;

        // --- Constructor ---
        // Every module MUST accept exactly one argument: a ModuleBuilder.
        // The module system calls this constructor when it instantiates the
        // module, passing in a builder pre-populated with JSON parameters
        // and a reference to the parent module.
        my_stride(champsim::modules::ModuleBuilder builder);

        // --- Interface overrides ---
        // champsim::modules::prefetcher declares six pure virtual methods.
        // We MUST override all of them.  For methods we don't need, we provide
        // an empty body with "override {}".
        //
        // prefetcher_initialize: Called once before simulation begins.
        void prefetcher_initialize() override {}

        // prefetcher_cache_operate: Called on every cache access (hit or miss).
        // This is the main hook — it's where we detect strides and issue
        // prefetches.  Returns metadata to pass through to future callbacks.
        uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip,
            bool cache_hit, bool useful_prefetch, access_type type,
            uint32_t metadata_in) override;

        // prefetcher_cache_fill: Called when a cache line is filled (loaded into
        // the cache).  Useful for feedback-directed prefetching.  We don't need
        // it for a simple stride prefetcher, so we just pass through the metadata.
        uint32_t prefetcher_cache_fill(champsim::address addr, long set, long way,
            bool prefetch, champsim::address evicted_addr, uint32_t metadata_in) override
        { return metadata_in; }

        // prefetcher_cycle_operate: Called once every cycle.  Useful if your
        // prefetcher needs to do work that isn't tied to a specific cache access
        // (e.g. draining a lookahead queue, as ip_stride does).
        void prefetcher_cycle_operate() override {}

        // prefetcher_final_stats: Called at the end of simulation.  Use it to
        // print custom statistics.
        void prefetcher_final_stats() override {}

        // prefetcher_branch_operate: Called on every branch instruction executed
        // by the core attached to this cache.  Some prefetchers (e.g. TAGE-based)
        // use branch outcomes.  We don't, so we stub it.
        void prefetcher_branch_operate(champsim::address ip, uint8_t branch_type,
            champsim::address branch_target) override {}
    };

    #endif

**Why inherit from** ``champsim::modules::prefetcher``?  It is the interface that defines
the contract between the cache and its prefetcher(s).  Every cache expects its prefetcher
submodules to implement these six methods.  The ``module_base`` machinery behind
``prefetcher`` provides the static factory (``create_instance``, ``register_module``) that
the config system uses to instantiate your module by name.

**Why must the constructor take** ``ModuleBuilder``?  The registration macro stores a
factory lambda ``[](ModuleBuilder b){ return std::make_unique<T>(b); }``.  If your
constructor has a different signature, that lambda won't compile and registration will
fail.

Step 3: Implement the Module
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create ``prefetcher/my_stride/my_stride.cc``:

.. code-block:: cpp
    :linenos:

    #include "my_stride.h"

    // "cache.h" provides the concrete CACHE class.  We don't strictly need it here
    // because we only interact through the cache_module interface, but some modules
    // may need it for additional helpers.
    #include "cache.h"

    // =========================================================================
    // REGISTRATION
    //
    // This single line registers our class with the module system under the name
    // "my_stride".  When a config file says "model": "my_stride", the factory
    // will call our constructor.
    //
    // The variable name (my_stride_reg) is arbitrary — it just needs to be a
    // global so it runs at static-initialization time.
    //
    // Breaking down the type:
    //   champsim::modules::prefetcher           — the interface we implement
    //   ::register_module<my_stride>            — template helper on module_base
    //   my_stride_reg("my_stride")             — constructor arg = model name
    // =========================================================================
    champsim::modules::prefetcher::register_module<my_stride>
        my_stride_reg("my_stride");

    // =========================================================================
    // CONSTRUCTOR
    //
    // The ModuleBuilder is pre-populated by the config system before we see it.
    // It contains:
    //   - A pointer to the parent module (a cache_module*)
    //   - All JSON parameters as a string->std::any map
    //   - The module name and model name
    //
    // get_parameter<T>(name, optional, default):
    //   - name:     JSON key to look up
    //   - optional: if true, return default_value when key is missing;
    //               if false, exit with an error when key is missing
    //   - default:  value to use when key is missing (only if optional=true)
    //
    // get_parent<T>():
    //   - Returns a T* to the parent module.  For prefetchers and replacement
    //     policies, T is cache_module.  For branch predictors and BTBs, T is
    //     core_module.
    // =========================================================================
    my_stride::my_stride(champsim::modules::ModuleBuilder builder)
        : degree(builder.get_parameter<int>("degree", true, 3)),
          cache_(builder.get_parent<champsim::modules::cache_module>())
    {
        // At this point:
        //   degree == the "degree" value from JSON, or 3 if absent
        //   cache_ == pointer to the parent cache (never null if config is valid)
    }

    // =========================================================================
    // MAIN HOOK: prefetcher_cache_operate
    //
    // Called by the cache on every access (loads, stores, prefetch fills).
    //
    // Parameters:
    //   addr           — full byte address of the access
    //   ip             — instruction pointer that caused the access
    //   cache_hit      — true if the access was a cache hit
    //   useful_prefetch— true if a prior prefetch for this address was used
    //   type           — LOAD, RFO (store), PREFETCH, WRITE, or TRANSLATION
    //   metadata_in    — metadata propagated from a prior prefetcher in the chain
    //
    // Returns: metadata to propagate to the next prefetcher in the chain.
    // =========================================================================
    uint32_t my_stride::prefetcher_cache_operate(
        champsim::address addr, champsim::address ip,
        bool cache_hit, bool useful_prefetch, access_type type,
        uint32_t metadata_in)
    {
        // Convert the byte address to a cache-line (block) number.
        // champsim::block_number strips the low bits (the block offset),
        // giving us a cache-line-granularity address for stride computation.
        champsim::block_number cl_addr{addr};

        // Look up whether we've seen this IP before.
        auto it = table.find(ip);
        if (it != table.end()) {
            // We've seen this IP before.  Compute the stride: the difference
            // between the current cache-line and the last one this IP accessed.
            // champsim::offset(a, b) returns b - a in block-number units.
            auto stride = champsim::offset(it->second.last_addr, cl_addr);

            // Only prefetch if:
            //   1. stride != 0   — the IP didn't access the same line twice
            //   2. stride == last_stride — the pattern is consistent
            if (stride != 0 && stride == it->second.stride) {
                for (int i = 0; i < degree; ++i) {
                    // Compute the prefetch address: current line + (stride * (i+1))
                    champsim::address pf_addr{cl_addr + (stride * (i + 1))};

                    // Decide whether to fill this level or a lower level.
                    // When MSHRs are less than 50% full, fill this cache level
                    // (L2C).  When they're busy, fill the level below (LLC)
                    // to avoid polluting the closer cache.
                    bool fill_this_level = cache_->get_mshr_occupancy_ratio() < 0.5;

                    // prefetch_line() is provided by the prefetcher base class.
                    // It issues the prefetch through the parent cache.
                    // Arguments: address, fill_this_level, metadata.
                    prefetch_line(pf_addr, fill_this_level, metadata_in);
                }
            }

            // Update the tracker for next time.
            it->second.stride = stride;
            it->second.last_addr = cl_addr;
        } else {
            // First time seeing this IP.  Create a tracker entry with stride 0
            // (we need at least two accesses to compute a stride).
            table[ip] = {cl_addr, 0};
        }

        // Pass metadata through unchanged.
        return metadata_in;
    }

Step 4: Build
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Rebuild ChampSim.  The Makefile automatically discovers source files under
``prefetcher/``, ``replacement/``, ``branch/``, and ``btb/``::

    make -j$(nproc)

If you get a linker error about duplicate symbols, make sure your ``register_module``
variable name is unique across all modules.

Step 5: Configure and Run
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Legacy config** — the simplest option, just name the model::

    {
        "L2C": { "prefetcher": "my_stride" }
    }

To pass parameters to the prefetcher, use the object form (see
:ref:`Passing Parameters to Submodules <Legacy_Submodule_Params>`)::

    {
        "L2C": { "prefetcher": {"model": "my_stride", "degree": 4} }
    }

**Explicit config** — the prefetcher is a child of the cache module::

    {
        "name": "cpu0_L2C_prefetcher",
        "module": "prefetcher",
        "model": "my_stride",
        "degree": 4
    }

**Run**::

    bin/champsim --config my_config.json \
        --warmup-instructions 200000000 \
        --simulation-instructions 500000000 \
        trace.champsimtrace.xz

**Verify with** ``--dump`` — add ``--dump`` to see every module's parameters and
connections, confirming your prefetcher was constructed with the right degree::

    bin/champsim --config my_config.json --dump

.. _Tutorial_Replacement:

------------------------------------------
Part 2: Creating a Replacement Policy
------------------------------------------

This section builds a simple LRU (Least Recently Used) replacement policy.  It
follows the same three-step pattern as the prefetcher: inherit, override, register.

**Header** (``replacement/my_lru/my_lru.h``):

.. code-block:: cpp
    :linenos:

    #ifndef MY_LRU_H
    #define MY_LRU_H

    #include <vector>
    #include "modules.h"

    // -------------------------------------------------------------------------
    // my_lru — Least Recently Used replacement policy.
    //
    // Inherits from champsim::modules::replacement, which is
    // module_base<replacement, cache_module>.  Like prefetchers, replacement
    // policies are children of a cache_module.
    //
    // The replacement interface has four pure virtual methods:
    //   initialize_replacement()       — one-time setup
    //   find_victim(...)               — pick a way to evict
    //   update_replacement_state(...)  — called on every access (hit or miss)
    //   replacement_cache_fill(...)    — called when a new line is filled
    //   replacement_final_stats()      — end-of-simulation hook
    // -------------------------------------------------------------------------
    struct my_lru : public champsim::modules::replacement {

        // Number of ways per set — we read this from the parent cache at
        // construction time so our flat vector is correctly sized.
        long num_ways;

        // Flat vector of last-used timestamps, indexed as [set * num_ways + way].
        // A flat vector is faster than a map<pair<long,long>, ...> because the
        // access pattern is dense and predictable.
        std::vector<uint64_t> last_used;

        // Monotonically increasing timestamp.  We increment it on every access,
        // giving us a total ordering of recency.
        uint64_t cycle = 0;

        // Constructor — reads cache geometry from the parent.
        my_lru(champsim::modules::ModuleBuilder builder);

        // --- Interface overrides ---

        void initialize_replacement() override {}

        // find_victim: Return the way index within "set" that should be evicted.
        // The cache calls this when it needs to make room for a new line.
        long find_victim(uint32_t triggering_cpu, uint64_t instr_id, long set,
            const champsim::cache_block* current_set, champsim::address ip,
            champsim::address full_addr, access_type type) override;

        // update_replacement_state: Called on every cache access (hit or miss).
        // We use it to update the recency timestamp.
        void update_replacement_state(uint32_t triggering_cpu, long set, long way,
            champsim::address full_addr, champsim::address ip,
            champsim::address victim_addr, access_type type, bool hit) override;

        // replacement_cache_fill: Called when a new line is physically written
        // into the cache.  We also update the timestamp here.
        void replacement_cache_fill(uint32_t triggering_cpu, long set, long way,
            champsim::address full_addr, champsim::address ip,
            champsim::address victim_addr, access_type type) override;

        void replacement_final_stats() override {}
    };

    #endif

**Implementation** (``replacement/my_lru/my_lru.cc``):

.. code-block:: cpp
    :linenos:

    #include "my_lru.h"
    #include <algorithm>

    // Register under the name "my_lru".
    champsim::modules::replacement::register_module<my_lru>
        my_lru_reg("my_lru");

    // -------------------------------------------------------------------------
    // Constructor
    //
    // We query the parent cache for its geometry (num_sets, num_ways) so we can
    // size our data structures correctly.  This is the typical pattern for
    // replacement policies: they need to know how big the cache is.
    // -------------------------------------------------------------------------
    my_lru::my_lru(champsim::modules::ModuleBuilder builder)
        : num_ways(static_cast<long>(
              builder.get_parent<champsim::modules::cache_module>()->num_ways())),
          last_used(static_cast<std::size_t>(
              builder.get_parent<champsim::modules::cache_module>()->num_sets()
              * num_ways), 0)
    {
        // last_used is now a flat vector of size (sets * ways), all initialized
        // to 0.  Way indices within each set are at offsets [set*ways .. set*ways+ways).
    }

    // -------------------------------------------------------------------------
    // find_victim: Scan the last_used timestamps for this set and return the
    // way with the smallest (oldest) timestamp.
    // -------------------------------------------------------------------------
    long my_lru::find_victim(uint32_t /*triggering_cpu*/, uint64_t /*instr_id*/,
        long set, const champsim::cache_block* /*current_set*/,
        champsim::address /*ip*/, champsim::address /*full_addr*/,
        access_type /*type*/)
    {
        // Point to the start of this set's slice of the flat vector.
        auto begin = std::next(last_used.begin(), set * num_ways);
        auto end = std::next(begin, num_ways);

        // std::min_element finds the way with the smallest last-used time.
        auto victim = std::min_element(begin, end);
        return std::distance(begin, victim);
    }

    // -------------------------------------------------------------------------
    // update_replacement_state: Stamp the accessed way with the current time.
    //
    // We skip writeback hits because writebacks are not demand accesses — they
    // are cache maintenance operations.  Promoting a line on a writeback would
    // artificially extend its lifetime.
    // -------------------------------------------------------------------------
    void my_lru::update_replacement_state(uint32_t /*triggering_cpu*/, long set,
        long way, champsim::address /*full_addr*/, champsim::address /*ip*/,
        champsim::address /*victim_addr*/, access_type type, bool hit)
    {
        if (hit && type != access_type::WRITE)
            last_used.at(static_cast<std::size_t>(set * num_ways + way)) = cycle++;
    }

    // -------------------------------------------------------------------------
    // replacement_cache_fill: A new line was just written into set/way.
    // Mark it as recently used.
    // -------------------------------------------------------------------------
    void my_lru::replacement_cache_fill(uint32_t /*triggering_cpu*/, long set,
        long way, champsim::address /*full_addr*/, champsim::address /*ip*/,
        champsim::address /*victim_addr*/, access_type /*type*/)
    {
        last_used.at(static_cast<std::size_t>(set * num_ways + way)) = cycle++;
    }

.. _Tutorial_BranchPredictor:

------------------------------------------
Part 3: Creating a Branch Predictor
------------------------------------------

Branch predictors follow the same pattern, but they are children of ``core_module``
instead of ``cache_module``.

The ``branch_predictor`` interface has three pure virtual methods:

* ``initialize_branch_predictor()`` — one-time setup
* ``predict_branch(ip, predicted_target, always_taken, branch_type)`` — return ``true``
  for taken, ``false`` for not-taken
* ``last_branch_result(ip, target, taken, branch_type)`` — called after the branch is
  resolved so you can update predictor state

**A simple bimodal predictor** (``branch/my_bimodal/my_bimodal.h``):

.. code-block:: cpp
    :linenos:

    #ifndef MY_BIMODAL_H
    #define MY_BIMODAL_H

    #include <vector>
    #include "modules.h"

    // -------------------------------------------------------------------------
    // my_bimodal — A simple bimodal branch predictor.
    //
    // Uses a table of 2-bit saturating counters indexed by the low bits of the
    // instruction pointer.  This is the simplest dynamic branch predictor.
    //
    // Inherits from champsim::modules::branch_predictor, which is
    // module_base<branch_predictor, core_module>.  Branch predictors are
    // children of a core_module.
    // -------------------------------------------------------------------------
    struct my_bimodal : public champsim::modules::branch_predictor {

        // Number of index bits — controls table size (2^index_bits entries).
        int index_bits;

        // Table of 2-bit saturating counters.  Each entry is 0–3:
        //   0,1 = predict not-taken
        //   2,3 = predict taken
        // We use int8_t to save space.
        std::vector<int8_t> table;

        // Constructor reads table_size from JSON config.
        my_bimodal(champsim::modules::ModuleBuilder builder);

        void initialize_branch_predictor() override {}

        // predict_branch: Return true (taken) or false (not-taken).
        bool predict_branch(champsim::address ip, champsim::address predicted_target,
            bool always_taken, uint8_t branch_type) override;

        // last_branch_result: Update the counter after the branch is resolved.
        void last_branch_result(champsim::address ip, champsim::address target,
            bool taken, uint8_t branch_type) override;
    };

    #endif

**Implementation** (``branch/my_bimodal/my_bimodal.cc``):

.. code-block:: cpp
    :linenos:

    #include "my_bimodal.h"

    // Register as "my_bimodal".
    champsim::modules::branch_predictor::register_module<my_bimodal>
        my_bimodal_reg("my_bimodal");

    // -------------------------------------------------------------------------
    // Constructor
    //
    // Read table_size from JSON (default 4096 entries = 12 index bits).
    // Initialize all counters to 2 (weakly taken), a common starting point.
    // -------------------------------------------------------------------------
    my_bimodal::my_bimodal(champsim::modules::ModuleBuilder builder)
        : index_bits(builder.get_parameter<int>("index_bits", true, 12)),
          table(1 << index_bits, 2)  // 2^index_bits entries, init to "weakly taken"
    {
    }

    // -------------------------------------------------------------------------
    // predict_branch
    //
    // Index into the counter table using the low bits of the IP.
    // Counter >= 2 means "predict taken".
    // -------------------------------------------------------------------------
    bool my_bimodal::predict_branch(champsim::address ip,
        champsim::address /*predicted_target*/, bool /*always_taken*/,
        uint8_t /*branch_type*/)
    {
        // Use ip.to<std::size_t>() to extract the raw integer value.
        // Mask with (table_size - 1) to get an index.
        std::size_t index = ip.to<std::size_t>() & ((1u << index_bits) - 1);
        return table[index] >= 2;
    }

    // -------------------------------------------------------------------------
    // last_branch_result
    //
    // Increment the counter if the branch was taken, decrement if not-taken.
    // Saturate at 0 and 3 (2-bit counter).
    // -------------------------------------------------------------------------
    void my_bimodal::last_branch_result(champsim::address ip,
        champsim::address /*target*/, bool taken, uint8_t /*branch_type*/)
    {
        std::size_t index = ip.to<std::size_t>() & ((1u << index_bits) - 1);
        if (taken && table[index] < 3)
            table[index]++;
        else if (!taken && table[index] > 0)
            table[index]--;
    }

.. _Tutorial_ModuleBuilder:

------------------------------------------
Part 4: Understanding ModuleBuilder
------------------------------------------

``ModuleBuilder`` is the object the module system passes to your constructor.  It carries
all the information your module needs to initialize itself.

Reading Parameters
^^^^^^^^^^^^^^^^^^^^^

``get_parameter<T>(name, optional, default_value)`` reads a JSON value:

.. code-block:: cpp

    // REQUIRED: exits with an error if "sets" is not in the JSON.
    // The third argument is ignored when optional=false, but you must provide it
    // due to the template signature.
    int sets = builder.get_parameter<int>("sets", false, 0);

    // OPTIONAL: returns 3 if "degree" is not in the JSON.
    int degree = builder.get_parameter<int>("degree", true, 3);

    // STRING parameter:
    auto mode = builder.get_parameter<std::string>("mode", true, "normal");

    // SIZE parameter — uses std::size_t, which is 64-bit unsigned.
    // numeric_any_cast handles converting JSON integers (stored as int64_t)
    // to std::size_t automatically.
    auto sz = builder.get_parameter<std::size_t>("table_size", true, 256);

The parameter name corresponds to a key in the module's JSON config entry.
See :ref:`Passing Parameters to Submodules <Legacy_Submodule_Params>` and the
:ref:`Explicit Configuration Format <Explicit_Config>` for how these values
get into the ``ModuleBuilder``.

Accessing the Parent Module
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``get_parent<T>()`` returns a pointer to the module that owns this submodule:

.. code-block:: cpp

    // Prefetchers and replacement policies → parent is cache_module
    auto* cache = builder.get_parent<champsim::modules::cache_module>();

    // Branch predictors and BTBs → parent is core_module
    auto* core = builder.get_parent<champsim::modules::core_module>();

Store the pointer in a member variable if you need it after construction.  Methods
available on ``cache_module`` include:

* ``get_mshr_occupancy_ratio()`` — MSHR load as a fraction (0.0 to 1.0)
* ``get_mshr_size()`` — total MSHR capacity
* ``num_sets()`` / ``num_ways()`` — cache geometry
* ``is_virtual_prefetch()`` — whether prefetch addresses are virtual
* ``get_rq_occupancy()`` / ``get_wq_occupancy()`` / ``get_pq_occupancy()`` — queue loads

Querying Module Name and Model
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: cpp

    // The instance name (e.g. "cpu0_L2C_prefetcher"):
    std::string name = builder.get_name();

    // The model name (e.g. "my_stride"):
    std::string model = builder.get_model();

.. _Tutorial_Debugging:

------------------------------------------
Part 5: Debugging Module Construction
------------------------------------------

Use ``--dump`` to see every module that was constructed, its parameters, and its
connections::

    bin/champsim --config my_config.json --dump

Example output:

.. code-block:: text

    [cpu0_L2C_prefetcher] degree = 4 (set)
    [cpu0_L2C_prefetcher] created_module = my_stride (set)
    [cpu0_L2C_replacement] created_module = lru (set)

This is invaluable for verifying that:

* Your module was instantiated (look for ``created_module = your_model_name``)
* Parameters have the expected values (``(set)`` vs ``(default)``)
* Parent/child connections are correct

.. _Tutorial_NewInterface:

------------------------------------------
Part 6: Defining a New Interface
------------------------------------------

ChampSim ships with built-in interfaces for prefetchers, replacement policies, branch
predictors, and BTBs.  But the module system is fully extensible — you can define your
own interface types and attach them to existing parent modules.

This section walks through creating a hypothetical **cache listener** interface: a
module that is notified of cache events (hits, misses, evictions) for observability
purposes, without modifying the cache's behavior.

Step 1: Define the Interface
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create ``inc/cache_listener.h``:

.. code-block:: cpp
    :linenos:

    #ifndef CACHE_LISTENER_H
    #define CACHE_LISTENER_H

    #include "modules.h"

    namespace champsim::modules {

    // -------------------------------------------------------------------------
    // cache_listener — An interface for cache event observers.
    //
    // Inherits from module_base<cache_listener, cache_module>.
    //
    // The first template argument (cache_listener) is the interface type itself.
    // The second (cache_module) is the parent type — cache listeners are children
    // of a cache, just like prefetchers and replacement policies.
    //
    // module_base provides:
    //   - register_module<T>  : register an implementation by model name
    //   - register_interface  : register this interface type by string name
    //   - create_instance()   : factory function used by the config system
    // -------------------------------------------------------------------------
    struct cache_listener : public module_base<cache_listener, cache_module> {

        virtual ~cache_listener() = default;

        // --- Pure virtual methods that implementations must override ---

        // Called when the simulation starts.
        virtual void on_initialize() = 0;

        // Called on every cache hit.  Parameters mirror the cache's internal state.
        virtual void on_hit(champsim::address addr, champsim::address ip,
                            long set, long way, access_type type) = 0;

        // Called on every cache miss (after victim selection, before fill).
        virtual void on_miss(champsim::address addr, champsim::address ip,
                             access_type type) = 0;

        // Called at end of simulation for final reporting.
        virtual void on_final_stats() = 0;
    };

    } // namespace champsim::modules

    #endif

**Why** ``module_base<cache_listener, cache_module>``?  The first template parameter
is always the interface class itself (CRTP-style).  This ensures that the static
factory (``module_map``, ``instance_map``, ``register_module``) is unique to this
interface — a ``cache_listener`` registration won't collide with a ``prefetcher``
registration.  The second parameter is the parent type, which determines what
``get_parent<T>()`` returns.

Step 2: Register the Interface
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The config system needs to know about your interface so it can create modules by
interface name (``"module": "cache_listener"``).  Add a registration in a ``.cc``
file — you can add it to ``src/modules.cc`` or create a new file:

.. code-block:: cpp

    // In src/modules.cc (or a new file that gets compiled):
    #include "cache_listener.h"

    static champsim::modules::cache_listener::register_interface
        cache_listener_iface_reg("cache_listener");

This single line makes ``interface_registry::create("cache_listener", builder, parent)``
work.  The string ``"cache_listener"`` is what you use as the ``"module"`` key in
the JSON config.

Step 3: Write an Implementation
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Create ``modules/hit_counter/hit_counter.h``:

.. code-block:: cpp
    :linenos:

    #ifndef HIT_COUNTER_H
    #define HIT_COUNTER_H

    #include <cstdint>
    #include "cache_listener.h"

    // -------------------------------------------------------------------------
    // hit_counter — A simple cache_listener that counts hits and misses.
    //
    // Demonstrates how to implement a custom interface.  The pattern is
    // identical to prefetchers and replacement policies:
    //   1. Inherit from the interface
    //   2. Override all pure virtual methods
    //   3. Accept a ModuleBuilder constructor
    //   4. Register with register_module
    // -------------------------------------------------------------------------
    struct hit_counter : public champsim::modules::cache_listener {

        uint64_t hits = 0;
        uint64_t misses = 0;

        hit_counter(champsim::modules::ModuleBuilder /*builder*/) {}

        void on_initialize() override {}

        void on_hit(champsim::address /*addr*/, champsim::address /*ip*/,
                    long /*set*/, long /*way*/, access_type /*type*/) override {
            ++hits;
        }

        void on_miss(champsim::address /*addr*/, champsim::address /*ip*/,
                     access_type /*type*/) override {
            ++misses;
        }

        void on_final_stats() override {
            fmt::print("HIT_COUNTER: hits={} misses={} rate={:.4f}\n",
                       hits, misses,
                       static_cast<double>(hits) / (hits + misses));
        }
    };

    #endif

**Implementation file** (``modules/hit_counter/hit_counter.cc``):

.. code-block:: cpp

    #include "hit_counter.h"

    // Register "hit_counter" as an implementation of cache_listener.
    champsim::modules::cache_listener::register_module<hit_counter>
        hit_counter_reg("hit_counter");

Step 4: Wire It Up in the Parent Module
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For your interface to actually receive events, the parent module (in this case, the
``CACHE`` class) needs to call your methods at the appropriate times.

In the cache implementation, you would:

1. Store a vector of ``cache_listener*`` obtained from the builder's submodules
2. Call each listener's methods at the appropriate points

.. code-block:: cpp

    // In the CACHE constructor or initialization:
    for (auto& sub : builder.get_submodules("cache_listener")) {
        auto* listener = cache_listener::create_instance(sub, this);
        listeners_.push_back(listener);
    }

    // In the cache hit path:
    for (auto* l : listeners_) l->on_hit(addr, ip, set, way, type);

    // In the cache miss path:
    for (auto* l : listeners_) l->on_miss(addr, ip, type);

Step 5: Configure It
^^^^^^^^^^^^^^^^^^^^^^^^

In the explicit config, your listener appears as a child of a cache module::

    {
        "name": "cpu0_L2C",
        "module": "cache",
        "model": "DEFAULT_CACHE",
        "children": [
            {"name": "cpu0_L2C_prefetcher", "module": "prefetcher", "model": "no"},
            {"name": "cpu0_L2C_replacement", "module": "replacement", "model": "lru"},
            {"name": "cpu0_L2C_listener", "module": "cache_listener", "model": "hit_counter"}
        ]
    }

Summary
^^^^^^^^^^

Creating a new interface requires four pieces:

1. **Interface class** — inherits ``module_base<YourInterface, ParentType>``
2. **Interface registration** — ``register_interface("name")`` in a ``.cc`` file
3. **Implementation class(es)** — inherit from your interface, register with
   ``register_module<Impl>("model_name")``
4. **Parent integration** — the parent module creates instances from submodule builders
   and calls the interface methods at the right times

The ``module_base`` template provides all the factory and registration infrastructure.
You just define the virtual methods and the parent integration.

.. _Tutorial_Migration:

------------------------------------------
Part 7: Migrating from Legacy Modules
------------------------------------------

If you have modules written for an older version of ChampSim that used free-function
hooks (e.g. ``void CACHE::prefetcher_initialize()``), here is how to update them:

1. **Create a class** inheriting from the appropriate interface
   (``champsim::modules::prefetcher``, ``::replacement``, ``::branch_predictor``, or
   ``::btb``).

2. **Move your free functions** into the class as method overrides:

   * Remove the ``CACHE::`` or ``O3_CPU::`` prefix.
   * Update parameter types: ``uint64_t`` addresses become ``champsim::address``,
     ``uint8_t type`` becomes ``access_type type``, etc.
   * Add ``override`` to each method.

3. **Move global state** into member variables.  Each module instance gets its own
   state — you no longer need to key state by cache/CPU pointer.

4. **Add a constructor** that takes ``champsim::modules::ModuleBuilder``.  If you need
   access to the parent cache or core, call ``builder.get_parent<T>()``.

5. **Add a registration line** in your ``.cc`` file::

       champsim::modules::prefetcher::register_module<my_pref>("my_pref");

6. **Remove the** ``__legacy__`` **file** if one exists in your module directory.

7. **Stub any new pure virtual methods** that didn't exist in the old interface (e.g.
   ``prefetcher_branch_operate``, ``replacement_cache_fill``) with ``override {}``.

Example diff for a prefetcher::

    // OLD (free-function style):
    void CACHE::prefetcher_initialize() { ... }
    uint32_t CACHE::prefetcher_cache_operate(uint64_t addr, uint64_t ip, ...) { ... }

    // NEW (class-based style):
    struct my_pref : champsim::modules::prefetcher {
        my_pref(champsim::modules::ModuleBuilder builder) {}
        void prefetcher_initialize() override { ... }
        uint32_t prefetcher_cache_operate(champsim::address addr, champsim::address ip, ...) override { ... }
        // ... all other pure virtuals stubbed or implemented
    };
    champsim::modules::prefetcher::register_module<my_pref>("my_pref");
