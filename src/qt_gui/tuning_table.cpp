// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "tuning_page.h"

// Meanings, ranges and prerequisites follow the comments on the settings in
// core/emulator_settings.h and the checks the emulator runs when the renderer starts.
std::vector<TuningPage::Group> TuningPage::Table() {
    constexpr u32 Any = std::numeric_limits<u32>::max();
    const Need skip_caches{"adaptive_skipcaches_mode", 1, Any};
    const Need copy_lane{"stream_copy_workers", 1, Any};
    return {
        {tr("Thread Placement"),
         true,
         {
             {"one_thread_per_core", tr("One Thread per Core (Windows)"),
              tr("Windows only. Restricts the whole process to the first logical CPU of every "
                 "physical core, the same as ticking only the even CPUs in Task Manager, so no two "
                 "emulator or guest threads share a core. Works together with the core reserved "
                 "for the GPU thread.")},
             {"gpu_thread_core_reserve", tr("Reserve a Core for the GPU Thread"),
              tr("Pins the GPU command thread to a physical core of its own, both hyperthreads, "
                 "and takes that core away from every other thread of the process, guest threads "
                 "included. Hosts with fewer than 6 logical CPUs or 4 physical cores are left "
                 "alone.")},
         }},
        {tr("Renderer Behavior and Pacing"),
         true,
         {
             {"adaptive_skipcaches_mode",
              tr("Skip Caches Mode"),
              tr("Master switch of the skip caches. Disabled turns off the memos that depend on "
                 "it. Forced pins every consumer cache on at boot; the controller, the verify "
                 "tripwire, telemetry and timer sampling never run."),
              {},
              {{0, tr("Disabled")},
               {1, tr("Adaptive")},
               {2, tr("Forced")},
               {3, tr("Validate Only")}}},
             {"occlude_all", tr("Answer Occlusion Queries as Occluded"),
              tr("Answers every occlusion query as fully occluded instead of fully visible. Titles "
                 "that gate effects on visibility, like the lens flares of inFAMOUS, then cull "
                 "those draws themselves. This changes what the game sees.")},
             {"flush_draw_interval", tr("Flush Draw Interval"),
              tr("Flushes the graphics command buffer every this many draws, so a guest readback "
                 "waits on at most that many draws. 0 flushes only at submit-done and on faults; "
                 "values from 1 to 63 are raised to 64.")},
             {"ring_drain_flush_draws",
              tr("Ring Drain Flush Draws"),
              tr("Flushes the open graphics batch early once it holds this many draws and every "
                 "batch submitted so far has retired. Rounded up to a multiple of 32; 0 turns it "
                 "off. Needs a Flush Draw Interval larger than the rounded value."),
              {{"flush_draw_interval", 1, Any}}},
             {"pending_pop_throttle", tr("Pending Pop Throttle"),
              tr("While a deferred operation waits out its GPU tick, the pending pop, a lock and a "
                 "fence query, is attempted once per this many polls. 0 and 1 attempt it on every "
                 "poll.")},
             {"stream_copy_workers",
              tr("Stream Copy Lane"),
              tr("Moves small read-only stream copies off the GPU command thread onto worker "
                 "threads. Fast Path has no foreign-producer refusal and no unmap push windows, so "
                 "it is only for titles that never unmap memory during play. Hardened is safe "
                 "everywhere."),
              {},
              {{0, tr("Off")}, {1, tr("Fast Path")}, {2, tr("Hardened")}}},
             {"stream_copy_lane_threads",
              tr("Stream Copy Threads"),
              tr("Worker threads of the stream copy lane, 1 to 4; 0 uses two. Read only while the "
                 "Stream Copy Lane is on."),
              {copy_lane},
              {},
              4},
             {"stream_copy_idle_us",
              tr("Stream Copy Idle Wait (us)"),
              tr("Idle wait of a stream copy worker between drain attempts, in microseconds. 0 "
                 "keeps the pause spin; a positive value parks the worker in a timed monitor "
                 "wait, which frees its core sibling. Ignored on CPUs without MWAITX. Read only "
                 "while the Stream Copy Lane is on."),
              {copy_lane}},
         }},
        {tr("Skip Caches and Register Stamps"),
         false,
         {
             {"dyn_state_memo",
              tr("Dynamic State Memo"),
              tr("Skips the five dynamic-state updaters and their commit when the stamped "
                 "graphics registers, the pipeline and the dirty-bit re-arm generation all match "
                 "the previous draw. Needs a Skip Caches Mode other than Disabled."),
              {skip_caches}},
             {"dyn_state_stamp",
              tr("Dynamic State Stamp Lane"),
              tr("Keys the dynamic-state memo on a stamp lane bumped only by the registers its "
                 "updaters read, and on the pipeline's write masks instead of its identity. Needs "
                 "the Dynamic State Memo."),
              {{"dyn_state_memo", 1, 1}}},
             {"rt_state_stamp",
              tr("Render Target Stamp Lane"),
              tr("Keys the render-target memo and the render-scope cache on a stamp lane bumped "
                 "only by the CB and DB registers they read, and on the MRT mask and color sample "
                 "count instead of the pipeline identity. Needs a Skip Caches Mode other than "
                 "Disabled."),
              {skip_caches}},
             {"push_vp_memo",
              tr("Viewport Push Constant Memo"),
              tr("Rebuilds the four viewport push constants only when the register stamp lane "
                 "moved, and clears only the push-constant prefixes the previous draw wrote. Needs "
                 "a Skip Caches Mode other than Disabled."),
              {skip_caches}},
             {"br_mem_fast_state",
              tr("Render Scope Fast-State Check"),
              tr("Re-certifies the render-scope cache on a moved memory generation from the "
                 "fast-state word of each bound attachment instead of rebuilding. Needs Image Fast "
                 "State and a Skip Caches Mode other than Disabled."),
              {skip_caches, {"image_fast_state", 1, 1}}},
             {"draw_glue_memo",
              tr("Draw Glue Memo"),
              tr("One certificate for the three per-draw memos on the all-hits path. Fold folds "
                 "their probes; Shadow builds and compares. Needs the Skip Caches Mode Forced, the "
                 "Dynamic State Memo and Readback Linear Images off."),
              {{"adaptive_skipcaches_mode", 2, 2},
               {"dyn_state_memo", 1, 1},
               {"readback_linear_images_enabled", 0, 0}},
              {{0, tr("Off")}, {1, tr("Fold")}, {3, tr("Shadow")}}},
             {"runtime_info_stamp_gate",
              tr("Runtime Info Stamp Gate"),
              tr("Skips building the runtime info and its fingerprint hash for the vertex and "
                 "fragment stages while the graphics register stamp is unchanged. Needs a Skip "
                 "Caches Mode other than Disabled."),
              {skip_caches}},
             {"pipeline_key_stamp_reuse",
              tr("Pipeline Key Stamp Reuse"),
              tr("Reuses the previous graphics pipeline key while the register stamp repeats, so "
                 "only the stage resolve runs again. Needs the Runtime Info Stamp Gate and a "
                 "driver with dynamic vertex input."),
              {{"runtime_info_stamp_gate", 1, 1}}},
             {"key_reuse_hash_diff",
              tr("Key Reuse Hash Accumulator"),
              tr("Decides the key reuse from a running XOR of the stage hashes the resolve "
                 "rewrites instead of reading the hash array again. Needs Pipeline Key Stamp "
                 "Reuse."),
              {{"pipeline_key_stamp_reuse", 1, 1}}},
             {"push_const_dedup",
              tr("Push Constant Dedup"),
              tr("Skips a push constant update when the previous push on the same command buffer "
                 "carried the same bytes with the same layout. Acts only while the Skip Caches "
                 "Mode is not Disabled."),
              {skip_caches}},
             {"runtime_info_input_memo", tr("Runtime Info Input Memo"),
              tr("A two-entry memo per stage of the register words the vertex, fragment and "
                 "compute runtime info builds read; an equal snapshot restores the runtime info "
                 "and its fingerprint hash.")},
             {"ri_memo_fused_cmp",
              tr("Runtime Info Fused Compare"),
              tr("Folds the compare of the runtime info snapshot against the last memo entry into "
                 "the snapshot itself. Needs the Runtime Info Input Memo."),
              {{"runtime_info_input_memo", 1, 1}}},
             {"vertex_layout_memo", tr("Vertex Layout Memo"),
              tr("Rebuilds the vertex input layout only when the pipeline, the instance step "
                 "rates or the format or stride of an attribute changed, instead of on every "
                 "draw.")},
             {"parser_reg_run", tr("Parser Register Runs"),
              tr("Runs consecutive register writes, padding and empty NOPs in a tight loop inside "
                 "the graphics packet parser instead of one trip through the packet dispatch "
                 "each.")},
         }},
        {tr("Shader Specialization and Pipeline Lookup"),
         false,
         {
             {"spec_fp_cache", tr("Specialization Fingerprint Cache"),
              tr("Resolves shader permutations through an address-masked specialization "
                 "fingerprint: a hit skips the specialization rebuild and the deep permutation "
                 "compares.")},
             {"spec_fp_canonical",
              tr("Canonical Fingerprint"),
              tr("A specialization fingerprint over the sharp bits the specialization reads. Tier "
                 "keys the tier on it and carries the resolved module in the MRU; Stage Slot adds "
                 "a per-stage slot answered by a memcmp. Needs the Specialization Fingerprint "
                 "Cache."),
              {{"spec_fp_cache", 1, 1}},
              {{0, tr("Off")}, {1, tr("Tier")}, {2, tr("Tier and Stage Slot")}}},
             {"spec_fp_slot_inplace",
              tr("In-Place Slot Compare"),
              tr("Compares the gathered specialization key against its per-stage slot and stores "
                 "it in one pass. Needs the Canonical Fingerprint with the stage slot."),
              {{"spec_fp_canonical", 2, 2}}},
             {"spec_fp_front",
              tr("Fingerprint Front Table"),
              tr("A 16-entry associative front over the fingerprint table of each program, for "
                 "programs that cycle through more specializations per frame than the MRU pair "
                 "holds. Needs the Canonical Fingerprint."),
              {{"spec_fp_canonical", 1, 2}}},
             {"spec_key_fast",
              tr("Fast Specialization Key"),
              tr("Aligned starts every key word on an 8-byte boundary, so the loads of the "
                 "in-place fold forward from the stores of the gather; Prefetched also warms the "
                 "slot lines ahead of the gather. Needs the Canonical Fingerprint."),
              {{"spec_fp_canonical", 1, 2}},
              {{0, tr("Off")}, {1, tr("Aligned")}, {2, tr("Aligned and Prefetched")}}},
             {"spec_key_fused",
              tr("Fused Specialization Key"),
              tr("Gathers the canonical specialization key straight into its compare slot, "
                 "folding the compare into the stores of the gather. Needs the In-Place Slot "
                 "Compare and the Fast Specialization Key."),
              {{"spec_fp_slot_inplace", 1, 1}, {"spec_key_fast", 1, Any}}},
             {"gather_input_memo",
              tr("Gather Input Memo"),
              tr("Keeps the inputs of the specialization key gather in the per-stage slot; a "
                 "byte-identical repeat for the same program is a slot hit without the gather. "
                 "Needs the Fused Specialization Key."),
              {{"spec_key_fused", 1, 1}}},
             {"spec_mru_perm_probe", tr("MRU Permutation Probe"),
              tr("Probes the most recently matched shader permutation before the linear search in "
                 "the pipeline cache. May select a different compare-equal permutation when "
                 "several stored specializations satisfy the probe.")},
             {"shader_params_memo", tr("Shader Params Memo"),
              tr("Reuses the result of the binary info search for a stage while its code pointer "
                 "and the hash stored inside the binary repeat.")},
             {"shader_params_memo_entries",
              tr("Shader Params Table Entries"),
              tr("Entries of a direct-mapped table behind the binary info memo, indexed by the "
                 "code address. Rounded up to a power of two; 0 keeps the single entry. Needs the "
                 "Shader Params Memo."),
              {{"shader_params_memo", 1, 1}},
              {},
              4096},
             {"static_color_write_mask", tr("Static Color Write Mask"),
              tr("Bakes the color write mask into the blend state of the pipeline instead of "
                 "declaring it dynamic. The mask is already a pipeline key field, so the pipeline "
                 "count is unchanged.")},
         }},
        {tr("Descriptor Sets"),
         false,
         {
             {"desc_delta_inplace",
              tr("In-Place Descriptor Delta"),
              tr("Compares and stores descriptor writes into the delta slot in one walk instead of "
                 "serializing to a scratch buffer and comparing afterwards. Acts only while the "
                 "Skip Caches Mode is not Disabled."),
              {skip_caches}},
             {"desc_delta_partial",
              tr("Partial Descriptor Pushes"),
              tr("Pushes only the descriptors whose bytes differ from the last push on the same "
                 "command buffer and layout. Needs the In-Place Descriptor Delta."),
              {{"desc_delta_inplace", 1, 1}}},
             {"desc_delta_flat",
              tr("Flat Descriptor Delta"),
              tr("On a Bind Write Plan hit, compares the two info arrays the plan tiles and "
                 "compacts from a change mask. Needs the In-Place Descriptor Delta, Shared "
                 "Descriptor Layouts and Bind Write Plan set to Plan."),
              {{"desc_delta_inplace", 1, 1},
               {"desc_layout_share", 1, 1},
               {"bind_write_plan", 1, 1}}},
             {"desc_layout_share", tr("Shared Descriptor Layouts"),
              tr("One descriptor set layout and pipeline layout per distinct binding list, shared "
                 "by every pipeline of that shape.")},
             {"desc_heap_recycle", tr("Descriptor Heap Recycling"),
              tr("Hands a heap descriptor set out again once the tick that recorded it retired, "
                 "instead of allocating a fresh one per push and resetting whole pools.")},
             {"push_desc_full_limit", tr("Inclusive Push Descriptor Limit"),
              tr("Lets a pipeline whose set 0 descriptor total equals maxPushDescriptors use push "
                 "descriptors.")},
             {"bind_write_plan",
              tr("Bind Write Plan"),
              tr("Plan uses the descriptor write plan of the pipeline in place of rebuilding the "
                 "writes for every bind. Shadow builds the plan and compares it with the rebuilt "
                 "list."),
              {},
              {{0, tr("Off")}, {1, tr("Plan")}, {2, tr("Shadow")}}},
         }},
        {tr("Texture Cache"),
         false,
         {
             {"texture_view_memo", tr("Texture View Memo"),
              tr("Hands a texture binding the view handle its image memo hit recorded, keyed on "
                 "the image backing; the view record scan runs only on a miss.")},
             {"bind_noop_memo",
              tr("Bind No-op Memo"),
              tr("Remembers per memo entry the backing epoch at which the shader-read transit was "
                 "a no-op; a repeat under that epoch skips the transit probe. Needs the Texture "
                 "View Memo."),
              {{"texture_view_memo", 1, 1}}},
             {"bind_image_lean",
              tr("Lean Image Bindings"),
              tr("Deferred image bindings prime the fields the memo probe reads in place instead "
                 "of running the full image description constructor. Needs the Bind No-op Memo."),
              {{"bind_noop_memo", 1, 1}}},
             {"bind_line_prefetch", tr("Bind Line Prefetch"),
              tr("Prefetches, during the first texture binding pass, the three image lines the "
                 "second pass reads first.")},
             {"findimg_trust_gen", tr("Trust Texture Generation"),
              tr("An image memo hit with an equal texture generation trusts the entry and skips "
                 "the uid check of the image record.")},
             {"findimg_range_invalidate",
              tr("Range-Scoped Memo Invalidation"),
              tr("Registering and unregistering an image clear only the image memo entries it "
                 "intersects instead of the whole memo. Needs Trust Texture Generation."),
              {{"findimg_trust_gen", 1, 1}}},
             {"findimg_memo_ways",
              tr("Image Memo Ways"),
              tr("Direct-Mapped keeps the 1024-slot probe; 1, 2 or 4 index the entries into sets "
                 "of that many ways with LRU replacement."),
              {},
              {{0, tr("Direct-Mapped")}, {1, "1"}, {2, "2"}, {4, "4"}}},
             {"findimg_memo_entries",
              tr("Image Memo Entries"),
              tr("Entry count of the image memo, clamped to 1024 to 32768 and rounded down to a "
                 "power of two; 0 keeps 2048. Has no effect with a direct-mapped image memo."),
              {{"findimg_memo_ways", 1, Any}},
              {},
              32768},
             {"findimg_memo_first", tr("Image Memo Before Validation"),
              tr("Probes the image memo before validating the texture descriptor; the validation "
                 "runs only on the routes that reach the full lookup.")},
             {"findimg_slot_hint", tr("Image Memo Slot Hint"),
              tr("Remembers per pipeline the image memo slot each image binding last matched; "
                 "the probe compares that entry before the hashed way scan.")},
             {"image_fast_state", tr("Image Fast State"),
              tr("Collapses the clean steady state of per-binding texture updates to one atomic "
                 "load instead of the touch, track and refresh pass.")},
             {"image_update_direct",
              tr("Direct Image Updates"),
              tr("Runs the per-image fast-state check directly for sampled bindings instead of "
                 "the per-binding dedup probe. Needs Image Fast State."),
              {{"image_fast_state", 1, 1}}},
             {"texture_lru_lazy_touch", tr("Lazy LRU Touches"),
              tr("Image touches only stamp the tick of the image; the LRU list is relinked when "
                 "the garbage collector meets an entry touched since its list tick.")},
             {"texture_invalidate_filter", tr("Texture Invalidate Filter"),
              tr("Answers a guest write fault against a lock-free coverage bitmap of the "
                 "registered images before the page table walk, so a fault in memory no image "
                 "covers skips the walk.")},
         }},
        {tr("Buffer Cache and Locks"),
         false,
         {
             {"covered_range_skip", tr("Covered Range Skip"),
              tr("Skips adding a buffer range to the barrier lists when one recorded range "
                 "already covers it.")},
             {"residency_bitmap", tr("Residency Bitmap"),
              tr("Answers whether a buffer is resident from one bit per sparse block instead of "
                 "searching the resident range list on every buffer bind.")},
             {"stream_barrier_skip", tr("Stream Barrier Skip"),
              tr("Leaves the stream buffer out of the barrier lists: every access it reports to "
                 "them is a read, so none of its binds can need a barrier.")},
             {"clean_sync_peek", tr("Clean Sync Peek"),
              tr("Looks at the CPU modified bits of a read-only bind before taking the tracker "
                 "lock, and skips the locked walk when none is set.")},
             {"backing_write_memo", tr("Backing Write Memo"),
              tr("Serves guest-visible backing writes from a per-thread memo of the last resolved "
                 "physical chunks; the memory map descent runs only on a miss.")},
             {"guest_copy_hold_segment", tr("Guest Copy Lock per Packet Run"),
              tr("Holds the guest copy shared lock once per graphics packet run instead of once "
                 "per draw.")},
             {"guest_copy_lock_batch", tr("Guest Copy Lock per Bind Batch"),
              tr("Holds the shared lock of the memory map across a whole buffer bind batch "
                 "instead of once per guest copy.")},
             {"tracker_lock_spin_rounds", tr("Tracker Lock Spin Rounds"),
              tr("On the GPU command thread a contended tracker region lock is spun on for up to "
                 "this many rounds before blocking. 0 keeps the plain blocking lock.")},
         }},
        // Edited on the other tabs of the dialog, or by no control.
        {{},
         false,
         {{"window_width"},
          {"window_height"},
          {"internal_screen_width"},
          {"internal_screen_height"},
          {"null_gpu"},
          {"copy_gpu_buffers"},
          {"readbacks_mode"},
          {"readback_linear_images_enabled", tr("Readback Linear Images")},
          {"readback_linear_images_async"},
          {"direct_memory_access_enabled"},
          {"dump_shaders"},
          {"patch_shaders"},
          {"vblank_frequency"},
          {"full_screen"},
          {"full_screen_mode"},
          {"present_mode"},
          {"hdr_allowed"},
          {"fsr_enabled"},
          {"rcas_enabled"},
          {"rcas_attenuation"},
          {"userfaultfd"},
          {"inline_fetch_shader"}}},
    };
}
