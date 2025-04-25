#pragma once

#include "ntdef.h"
#include "arch/irq.hpp"
#include <hz/list.hpp>

struct KINTERRUPT;

using PKMESSAGE_SERVICE_ROUTINE = BOOLEAN (*)(KINTERRUPT* irq, PVOID service_ctx, ULONG msg_id);

struct DEVICE_OBJECT;

struct KINTERRUPT {
	hz::list_hook hook {};
	union {
		PKSERVICE_ROUTINE fn;
		PKMESSAGE_SERVICE_ROUTINE msg_fn {};
	};
	void* service_ctx {};
	ULONG vector {};
	KSPIN_LOCK default_lock {};
	KSPIN_LOCK* lock {};
	DEVICE_OBJECT* device {};
	KIRQL synchronize_irql {};
	ULONG msg_id {};
	bool can_be_shared {};
};

struct IrqInfo {
	bool is_pin;
	union {
		struct {
			ULONG vector;
			ULONG pin;
			bool default_level_trigger;
			bool default_active_low;
		} pin;
		struct {
			ULONG vector;
		} msg;
	};
};

enum KINTERRUPT_MODE {
	LevelSensitive,
	Latched
};

enum KINTERRUPT_POLARITY {
	InterruptPolarityUnknown,
	InterruptActiveHigh,
	InterruptRisingEdge = InterruptActiveHigh,
	InterruptActiveLow,
	InterruptFallingEdge = InterruptActiveLow,
	InterruptActiveBoth,
	InterruptActiveBothTriggerLow = InterruptActiveBoth,
	InterruptActiveBothTriggerHigh
};

struct IO_INTERRUPT_MESSAGE_INFO_ENTRY {
	PHYSICAL_ADDRESS message_addr;
	KAFFINITY target_processor_set;
	KINTERRUPT* irq_obj;;
	ULONG message_data;
	ULONG vector;
	KIRQL irql;
	KINTERRUPT_MODE mode;
	KINTERRUPT_POLARITY polarity;
};

struct IO_INTERRUPT_MESSAGE_INFO {
	KIRQL unified_irql;
	ULONG message_count;
	IO_INTERRUPT_MESSAGE_INFO_ENTRY message_info[];
};

struct DEVICE_OBJECT;

struct IO_CONNECT_INTERRUPT_FULLY_SPECIFIED_PARAMETERS {
	DEVICE_OBJECT* physical_device;
	KINTERRUPT** irq_obj;
	PKSERVICE_ROUTINE service_routine;
	PVOID service_ctx;
	KSPIN_LOCK* spinlock;
	KIRQL synchronize_irql;
	BOOLEAN floating_save;
	BOOLEAN share_vector;
	ULONG vector;
	KIRQL irql;
	KINTERRUPT_MODE interrupt_mode;
	KAFFINITY processor_enable_mask;
	USHORT group;
};

struct IO_CONNECT_INTERRUPT_LINE_BASED_PARAMETERS {
	DEVICE_OBJECT* physical_device;
	KINTERRUPT** irq_obj;
	PKSERVICE_ROUTINE service_routine;
	PVOID service_ctx;
	KSPIN_LOCK* spinlock;
	KIRQL synchronize_irql;
	BOOLEAN floating_save;
};

struct IO_CONNECT_INTERRUPT_MESSAGE_BASED_PARAMETERS {
	DEVICE_OBJECT* physical_device;
	union {
		PVOID* generic;
		IO_INTERRUPT_MESSAGE_INFO** irq_msg_table;
		KINTERRUPT** irq_obj;
	} connection_ctx;
	PKMESSAGE_SERVICE_ROUTINE msg_service_routine;
	PVOID service_ctx;
	KSPIN_LOCK* spinlock;
	KIRQL synchronize_irql;
	BOOLEAN floating_save;
	PKSERVICE_ROUTINE fallback_service_routine;
};

#define CONNECT_FULLY_SPECIFIED 1
#define CONNECT_LINE_BASED 2
#define CONNECT_MESSAGE_BASED 3
#define CONNECT_FULLY_SPECIFIED_GROUP 4
#define CONNECT_MESSAGE_BASED_PASSIVE 5
#define CONNECT_CURRENT_VERSION 5

struct IO_CONNECT_INTERRUPT_PARAMETERS {
	ULONG version;
	union {
		IO_CONNECT_INTERRUPT_FULLY_SPECIFIED_PARAMETERS fully_specified;
		IO_CONNECT_INTERRUPT_LINE_BASED_PARAMETERS line_based;
		IO_CONNECT_INTERRUPT_MESSAGE_BASED_PARAMETERS message_based;
	};
};

struct IO_DISCONNECT_INTERRUPT_PARAMETERS {
	ULONG version;
	union {
		PVOID generic;
		KINTERRUPT* irq;
		IO_INTERRUPT_MESSAGE_INFO* irq_msg_table;
	} connection_ctx;
};

NTAPI extern "C" NTSTATUS IoConnectInterruptEx(IO_CONNECT_INTERRUPT_PARAMETERS* generic_params);
NTAPI extern "C" void IoDisconnectInterruptEx(IO_DISCONNECT_INTERRUPT_PARAMETERS* params);
