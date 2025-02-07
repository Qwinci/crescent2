#pragma once
#include "types.hpp"

struct KSYSTEM_TIME {
	u32 low_part;
	i32 high1_time;
	i32 high2_time;
};

#define PF_COMPARE_EXCHANGE_DOUBLE 2
#define PF_MMX_INSTRUCTIONS_AVAILABLE 3
#define PF_PAE_ENABLED 9
#define PF_NX_ENABLED 12
#define PF_SSE3_INSTRUCTIONS_AVAILABLE 13
#define PF_COMPARE_EXCHANGE128 14
#define PF_CHANNELS_ENABLED 16
#define PF_XSAVE_ENABLED 17
#define PF_RDWRFSGSBASE_AVAILABLE 22
#define PF_FASTFAIL_AVAILABLE 23
#define PF_RDRAND_INSTRUCTION_AVAILABLE 28
#define PF_RDPID_INSTRUCTION_AVAILABLE 33
#define PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE 34
#define PF_MONITORX_INSTRUCTION_AVAILABLE 35
#define PF_SSSE3_INSTRUCTIONS_AVAILABLE 36
#define PF_SSE4_1_INSTRUCTIONS_AVAILABLE 37
#define PF_SSE4_2_INSTRUCTIONS_AVAILABLE 38
#define PF_AVX_INSTRUCTIONS_AVAILABLE 39
#define PF_AVX2_INSTRUCTIONS_AVAILABLE 40
#define PF_ERMS_AVAILABLE 42
#define PROCESSOR_FEATURE_MAX 64

enum class ALTERNATIVE_ARCHITECTURE_TYPE {
	Standard,
	Nec98x86,
	End
};

enum class NT_PRODUCT_TYPE {
	WinNt = 1,
	LanManNt = 2,
	Server = 3
};

struct XSTATE_FEATURE {
	u32 offset;
	u32 size;
};

#define MAXIMUM_XSTATE_FEATURES 64

struct XSTATE_CONFIGURATION {
	u64 enabled_features;
	u64 enabled_volatile_featured;
	u32 size;
	union {
		u32 control_flags;
		struct {
			u32 optimized_save : 1;
			u32 compaction_enabled : 1;
			u32 extended_feature_disable : 1;
		};
	};
	XSTATE_FEATURE features[MAXIMUM_XSTATE_FEATURES];
	u64 enabled_supervisor_features;
	u64 aligned_features;
	u32 all_feature_size;
	u32 all_features[MAXIMUM_XSTATE_FEATURES];
	u64 enabled_user_visible_supervisor_features;
	u64 extended_feature_disable_features;
	u32 all_non_large_feature_size;
	u32 spare;
};

struct KUSER_SHARED_DATA {
	u32 tick_count_low_legacy;
	u32 tick_count_multiplier;
	volatile KSYSTEM_TIME interrupt_time;
	volatile KSYSTEM_TIME system_time;
	volatile KSYSTEM_TIME time_zone_bias;
	u16 image_number_low;
	u16 image_number_high;
	wchar_t nt_system_root[260];
	u32 max_stack_trace_depth;
	u32 crypto_exponent;
	u32 time_zone_id;
	u32 large_page_minimum;
	u32 ait_sampling_value;
	u32 app_compat_flag;
	u64 rng_seed_version;
	u32 global_validation_run_level;
	volatile i32 time_zone_bias_stamp;
	u32 nt_build_number;
	NT_PRODUCT_TYPE nt_product_type;
	bool product_type_is_valid;
	bool reserved0;
	u16 native_processor_architecture;
	u32 nt_major_version;
	u32 nt_minor_version;
	bool processor_features[PROCESSOR_FEATURE_MAX];
	u32 reserved1[2];
	volatile u32 time_slip;
	ALTERNATIVE_ARCHITECTURE_TYPE alternative_architecture;
	u32 boot_id;
	u64 system_expiration_date;
	u32 suite_mask;
	bool kd_debugger_enabled;
	union {
		u8 mitigation_policies;
		struct {
			u8 nx_support_policy : 2;
			u8 seh_validation_policy : 2;
			u8 cur_dir_devices_skipped_for_dlls : 2;
			u8 reserved : 2;
		};
	};
	u16 cycles_per_yield;
	volatile u32 active_console_id;
	volatile u32 dismount_count;
	u32 com_plus_package;
	u32 last_system_rit_event_tick_count;
	u32 number_of_physical_pages;
	bool safe_boot_mode;
	union {
		u8 virtualization_flags;
#ifdef __aarch64__
		struct {
			u8 arch_started_in_el2 : 1;
			u8 qc_sl_is_supported : 1;
			u8 : 6;
		};
#endif
	};
	u8 reserved2[2];
	u32 shared_data_flags;
	u32 data_flags_pad;
	u64 test_ret_instruction;
	i64 qpc_frequency;
	u32 system_call;
	u32 reserved3;
	u64 system_call_pad[2];
	union {
		volatile KSYSTEM_TIME tick_count;
		volatile u64 tick_count_quad;
		struct {
			u32 reserved_tick_count_overlay[3];
			u32 tick_count_pad;
		};
	};
	u32 cookie;
	u32 cookie_pad[1];
	i64 console_session_foreground_process_id;
	u64 time_update_lock;
	u64 baseline_system_time_qpc;
	u64 baseline_interrupt_time_qpc;
	u64 qpc_system_time_increment;
	u64 qpc_interrupt_time_increment;
	u8 qpc_system_time_increment_shift;
	u8 qpc_interrupt_time_increment_shift;
	u16 unparked_processor_count;
	u32 enclave_feature_mask[4];
	u32 telemetry_coverage_round;
	u16 user_mode_global_logger[16];
	u32 image_file_execution_options;
	u32 lang_generation_count;
	u64 reserved4;
	volatile u64 interrupt_time_bias;
	volatile u64 qpc_bias;
	u32 active_processor_count;
	volatile u8 active_group_count;
	u8 reserved5;
	union {
		u16 qpc_data;
		struct {
			volatile u8 qpc_bypass_enabled;
			u8 qpc_shift;
		};
	};
	i64 time_zone_bias_effective_start;
	i64 time_zone_bias_effective_end;
	XSTATE_CONFIGURATION xstate;
	KSYSTEM_TIME feature_configuration_change_stamp;
	u32 spare;
	u64 user_pointer_auth_mask;
};

#define SharedUserData reinterpret_cast<KUSER_SHARED_DATA*>(0xFFFFF78000000000)
