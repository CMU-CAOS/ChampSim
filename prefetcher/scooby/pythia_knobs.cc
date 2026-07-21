// Auto-generated for the champsim::modules port of Pythia/Scooby.
// Definitions extracted verbatim from Pythia src/knobs.cc; values for the
// knobs set in config/pythia.ini are baked in (Pythia's runtime .ini parser
// is dropped). Every scooby file's `extern knob::` declaration links here.
#include <string>
#include <vector>
#include <cstdint>
using namespace std;

namespace knob
{
	uint64_t warmup_instructions = 1000000;
	uint64_t simulation_instructions = 1000000;
	bool  	 knob_cloudsuite = false;
	bool     knob_low_bandwidth = false;
	vector<string> 	 l2c_prefetcher_types;
	vector<string> 	 l1d_prefetcher_types;
	bool     l1d_perfect = false;
	bool     l2c_perfect = false;
	bool     llc_perfect = false;
	bool     l1d_semi_perfect = false;
	bool     l2c_semi_perfect = false;
	bool     llc_semi_perfect = false;
	uint32_t semi_perfect_cache_page_buffer_size = 64;
	bool     measure_ipc = false;
	uint64_t measure_ipc_epoch = 1000;
	uint32_t dram_io_freq = 2400;
	bool     measure_dram_bw = true;
	uint64_t measure_dram_bw_epoch = 256;
	bool     measure_cache_acc = true;
	uint64_t measure_cache_acc_epoch = 1024;

	/* next-line */
	vector<int32_t>  next_line_deltas;
	vector<float>  next_line_delta_prob;
	uint32_t next_line_seed = 255;
	uint32_t next_line_pt_size = 256;
	bool     next_line_enable_prefetch_tracking = true;
	bool     next_line_enable_trace = false;
	uint32_t next_line_trace_interval = 5;
	string   next_line_trace_name = string("next_line_trace.csv");
	uint32_t next_line_pref_degree = 1;

	/* SMS */
	uint32_t sms_at_size = 32;
	uint32_t sms_ft_size = 64;
	uint32_t sms_pht_size = 16384;
	uint32_t sms_pht_assoc = 16;
	uint32_t sms_pref_degree = 4;
	uint32_t sms_region_size = 2048;
	uint32_t sms_region_size_log = 11;
	bool     sms_enable_pref_buffer = true;
	uint32_t sms_pref_buffer_size = 256;

	/* SPP */
	uint32_t spp_st_size = 256;
	uint32_t spp_pt_size = 512;
	uint32_t spp_max_outcomes = 4;
	double   spp_max_confidence = 25.0;
	uint32_t spp_max_depth = 64;
	uint32_t spp_max_prefetch_per_level = 1;
	uint32_t spp_max_confidence_counter_value = 16;
	uint32_t spp_max_global_counter_value = 1024;
	uint32_t spp_pf_size = 1024;
	bool     spp_enable_alpha = true;
	bool     spp_enable_pref_buffer = true;
	uint32_t spp_pref_buffer_size = 256;
	uint32_t spp_pref_degree = 4;
	bool     spp_enable_ghr = true;
	uint32_t spp_ghr_size = 8;
	uint32_t spp_signature_bits = 12;
	uint32_t spp_alpha_epoch = 1024;

	/* SPP_dev2 */
	uint32_t spp_dev2_fill_threshold = 90;
	uint32_t spp_dev2_pf_threshold = 25;

	/* BOP */
	vector<int32_t> bop_candidates;
	uint32_t bop_max_rounds = 100;
	uint32_t bop_max_score = 31;
	uint32_t bop_top_n = 1;
	bool     bop_enable_pref_buffer = false;
	uint32_t bop_pref_buffer_size = 256;
	uint32_t bop_pref_degree = 4;
	uint32_t bop_rr_size = 256;

	/* Sandbox */
	uint32_t sandbox_pref_degree = 4;
	bool     sandbox_enable_stream_detect = false;
	uint32_t sandbox_stream_detect_length = 4;
	uint32_t sandbox_num_access_in_phase = 256;
	uint32_t sandbox_num_cycle_offsets = 4;
	uint32_t sandbox_bloom_filter_size = 2048;
	uint32_t sandbox_seed = 200;

	/* DSPatch */
	uint32_t dspatch_log2_region_size;
	uint32_t dspatch_num_cachelines_in_region;
	uint32_t dspatch_pb_size;
	uint32_t dspatch_num_spt_entries;
	uint32_t dspatch_compression_granularity;
	uint32_t dspatch_pred_throttle_bw_thr;
	uint32_t dspatch_bitmap_selection_policy;
	uint32_t dspatch_sig_type;
	uint32_t dspatch_sig_hash_type;
	uint32_t dspatch_or_count_max;
	uint32_t dspatch_measure_covP_max;
	uint32_t dspatch_measure_accP_max;
	uint32_t dspatch_acc_thr;
	uint32_t dspatch_cov_thr;
	bool     dspatch_enable_pref_buffer;
	uint32_t dspatch_pref_buffer_size;
	uint32_t dspatch_pref_degree;

	/* PPF */
	int32_t ppf_perc_threshold_hi = -5;
	int32_t ppf_perc_threshold_lo = -15;

	/* MLOP */
	uint32_t mlop_pref_degree;
	uint32_t mlop_num_updates;
	float 	mlop_l1d_thresh;
	float 	mlop_l2c_thresh;
	float 	mlop_llc_thresh;
	uint32_t	mlop_debug_level;

	/* Bingo */
	uint32_t bingo_region_size = 2048;
	uint32_t bingo_pattern_len = 32;
	uint32_t bingo_pc_width = 16;
	uint32_t bingo_min_addr_width = 5;
	uint32_t bingo_max_addr_width = 16;
	uint32_t bingo_ft_size = 64;
	uint32_t bingo_at_size = 128;
	uint32_t bingo_pht_size = 8192;
	uint32_t bingo_pht_ways = 16;
	uint32_t bingo_pf_streamer_size = 128;
	uint32_t bingo_debug_level = 0;
	float    bingo_l1d_thresh;
	float    bingo_l2c_thresh;
	float    bingo_llc_thresh;
	string   bingo_pc_address_fill_level;

	/* Stride */
	uint32_t stride_num_trackers = 64;
   	uint32_t stride_pref_degree = 2;

	/* Streamer */
	uint32_t streamer_num_trackers = 64;
	uint32_t streamer_pref_degree = 5; /* models IBM POWER7 */

	/* AMPM */
	uint32_t ampm_pb_size = 64;
	uint32_t ampm_pred_degree = 4;
	uint32_t ampm_pref_degree = 4;
	uint32_t ampm_pref_buffer_size = 256;
	bool	 ampm_enable_pref_buffer = false;
	uint32_t ampm_max_delta = 16;

	/* Context Prefetcher */
	uint32_t cp_cst_size = 2048;
	uint32_t cp_cst_assoc = 16;
	uint32_t cp_max_response_per_cst = 4;
	int32_t cp_init_reward = 0;
	uint32_t cp_prefetch_queue_size = 128;

	/* IBM POWER7 Prefetcher */
	uint32_t power7_explore_epoch = 1000;
	uint32_t power7_exploit_epoch = 100000;
	uint32_t power7_default_streamer_degree = 4;

	/* Scooby */
	float scooby_alpha = 0.006508802942367162;
	float scooby_gamma = 0.556300959940946;
	float scooby_epsilon = 0.0018228444309622588;
	uint32_t scooby_state_num_bits = 10;
	uint32_t scooby_max_states = 1024;
	uint32_t scooby_seed = 200;
	string scooby_policy = std::string("EGreedy");
	string scooby_learning_type = std::string("SARSA");
	vector<int32_t> scooby_actions = {1, 3, 4, 5, 10, 11, 12, 22, 23, 30, 32, -1, -3, -6, 0};
	uint32_t scooby_max_actions = 15;
	uint32_t scooby_pt_size = 256;
	uint32_t scooby_st_size = 64;
	int32_t scooby_reward_none = -4;
	int32_t scooby_reward_incorrect = -8;
	int32_t scooby_reward_correct_untimely = 12;
	int32_t scooby_reward_correct_timely = 20;
	uint32_t scooby_max_pcs = 5;
	uint32_t scooby_max_offsets = 5;
	uint32_t scooby_max_deltas = 5;
	bool scooby_brain_zero_init = false;
	bool scooby_enable_reward_all = false;
	bool scooby_enable_track_multiple = false;
	bool scooby_enable_reward_out_of_bounds = true;
	int32_t scooby_reward_out_of_bounds = -12;
	uint32_t scooby_state_type = 1;
	bool scooby_access_debug = false;
	bool scooby_print_access_debug = false;
	uint64_t scooby_print_access_debug_pc = 0xdeadbeef;
	uint32_t scooby_print_access_debug_pc_count = 0;
	bool     scooby_print_trace = false;
	bool scooby_enable_state_action_stats = true;
	bool scooby_enable_reward_tracker_hit = false;
	int32_t scooby_reward_tracker_hit = -2;
	bool     scooby_enable_shaggy = false;
	uint32_t scooby_state_hash_type = 11;
	bool     scooby_prefetch_with_shaggy = false;
	bool scooby_enable_featurewise_engine = true;
	uint32_t scooby_pref_degree = 1;
	bool scooby_enable_dyn_degree = true;
	vector<float> scooby_max_to_avg_q_thresholds = {0.5, 1, 2};
	vector<int32_t> scooby_dyn_degrees = {1, 2, 4, 4};
	uint64_t scooby_early_exploration_window = 0;
	uint32_t scooby_multi_deg_select_type = 2;
	vector<int32_t> scooby_last_pref_offset_conf_thresholds = {1, 3, 8};
	vector<int32_t> scooby_dyn_degrees_type2 = {1, 2, 4, 6};
	uint32_t scooby_action_tracker_size = 2;
	uint32_t scooby_high_bw_thresh = 4;
	bool scooby_enable_hbw_reward = true;
	int32_t scooby_reward_hbw_correct_timely = 20;
	int32_t scooby_reward_hbw_correct_untimely = 12;
	int32_t scooby_reward_hbw_incorrect = -14;
	int32_t scooby_reward_hbw_none = -2;
	int32_t scooby_reward_hbw_out_of_bounds = -12;
	int32_t scooby_reward_hbw_tracker_hit = -2;
	vector<int32_t> scooby_last_pref_offset_conf_thresholds_hbw = {1, 3, 8};
	vector<int32_t> scooby_dyn_degrees_type2_hbw = {1, 2, 4, 6};

	/* Learning Engine */
	bool     le_enable_trace;
	uint32_t le_trace_interval;
	string   le_trace_file_name;
	uint32_t le_trace_state;
	bool     le_enable_score_plot;
	vector<int32_t> le_plot_actions;
	string   le_plot_file_name;
	bool     le_enable_action_trace;
	uint32_t le_action_trace_interval;
	std::string le_action_trace_name;
	bool     le_enable_action_plot;

	/* Featurewise Learning Engine */
	vector<int32_t> le_featurewise_active_features = {0, 10};
	vector<int32_t> le_featurewise_num_tilings = {3, 3};
	vector<int32_t> le_featurewise_num_tiles = {128, 128};
	vector<int32_t> le_featurewise_hash_types = {2, 2};
	vector<int32_t> le_featurewise_enable_tiling_offset = {1, 1};
	float le_featurewise_max_q_thresh = 0.50;
	bool le_featurewise_enable_action_fallback = true;
	vector<float> le_featurewise_feature_weights = {1.00, 1.00};
	bool le_featurewise_enable_dynamic_weight = false;
	float le_featurewise_weight_gradient = 0.001;
	bool le_featurewise_disable_adjust_weight_all_features_align = true;
	bool le_featurewise_selective_update = false;
	uint32_t le_featurewise_pooling_type = 2;
	bool le_featurewise_enable_dyn_action_fallback = true;
	uint32_t le_featurewise_bw_acc_check_level = 1;
	uint32_t le_featurewise_acc_thresh = 2;

	bool 			le_featurewise_enable_trace = false;
	uint32_t		le_featurewise_trace_feature_type;
	string 			le_featurewise_trace_feature;
	uint32_t 		le_featurewise_trace_interval;
	uint32_t 		le_featurewise_trace_record_count;
	std::string 	le_featurewise_trace_file_name;
	bool 			le_featurewise_enable_score_plot;
	vector<int32_t> le_featurewise_plot_actions;
	std::string 	le_featurewise_plot_file_name;
	bool 			le_featurewise_remove_plot_script;
}
