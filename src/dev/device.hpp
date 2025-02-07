#pragma once
#include "types.hpp"
#include "sched/dpc.hpp"
#include "ntdef.h"
#include "utils/spinlock.hpp"
#include "sched/event.hpp"
#include "sched/apc.hpp"

struct DRIVER_OBJECT;
struct IRP;
struct IO_TIMER;
struct VPB;
struct SECURITY_DESCRIPTOR;
struct DEVOBJ_EXTENSION;

using DEVICE_TYPE = u32;

struct KDEVICE_QUEUE {
	i16 type;
	u16 size;
	LIST_ENTRY device_list_head;
	KSPIN_LOCK lock;
#ifdef __x86_64__
	union {
		bool busy;
		i64 reserved;
	};
#else
	bool busy;
#endif
};

struct [[gnu::aligned(16)]] DEVICE_OBJECT {
	i16 type;
	u16 size;
	i32 reference_count;
	DRIVER_OBJECT* driver;
	DEVICE_OBJECT* next_device;
	DEVICE_OBJECT* attached_device;
	IRP* current_irp;
	IO_TIMER* timer;
	u32 flags;
	u32 characteristics;
	volatile VPB* vpb;
	void* device_extension;
	DEVICE_TYPE device_type;
	i8 stack_size;
	union {
		LIST_ENTRY list_entry;
	} queue;
	u32 alignment_requirement;
	KDEVICE_QUEUE device_queue;
	KDPC dpc;

	u32 active_thread_count;
	SECURITY_DESCRIPTOR* security_descriptor;
	KEVENT device_lock;
	u16 sector_size;
	u16 spare1;
	DEVOBJ_EXTENSION* device_object_extension;
	void* reserved;
};

struct DEVICE_OBJECT_POWER_EXTENSION;

struct DEVOBJ_EXTENSION {
	i16 type;
	u16 size;
	DEVICE_OBJECT* device;
	u32 power_flags;
	DEVICE_OBJECT_POWER_EXTENSION* dope;
	u32 extension_flags;
	void* device_node;
	DEVICE_OBJECT* attached_to;
	volatile i32 start_io_count;
	i32 start_io_key;
	u32 start_io_flags;
	VPB* vpb;
	void* dependency_node;
	void* interrupt_ctx;
	i32 interrupt_count;
	void* verifier_ctx;
};

struct DRIVER_EXTENSION;

using PDRIVER_INITIALIZE = NTSTATUS (*)(DRIVER_OBJECT* driver, UNICODE_STRING* registry_path);
using PDRIVER_STARTIO = void (*)(DEVICE_OBJECT* device, IRP* irp);
using PDRIVER_UNLOAD = void (*)(DRIVER_OBJECT* driver);
using PDRIVER_DISPATCH = NTSTATUS (*)(DEVICE_OBJECT* device, IRP* irp);
using PDRIVER_CANCEL = void (*)(DEVICE_OBJECT* device, IRP* irp);

#define IRP_MJ_DEVICE_CONTROL 0xE
#define IRP_MJ_INTERNAL_DEVICE_CONTROL 0xF
#define IRP_MJ_SYSTEM_CONTROL 0x17
#define IRP_MJ_PNP 0x1B
#define IRP_MJ_MAXIMUM_FUNCTION 0x1B

// pnp minor functions
#define IRP_MN_START_DEVICE 0
#define IRP_MN_REMOVE_DEVICE 2
#define IRP_MN_STOP_DEVICE 4
#define IRP_MN_READ_CONFIG 0xF
#define IRP_MN_WRITE_CONFIG 0x10

struct DRIVER_OBJECT {
	i16 type;
	i16 size;
	// device list
	DEVICE_OBJECT* device;
	u32 flags;
	// load location/size
	void* driver_start;
	u32 driver_size;
	void* driver_section;
	DRIVER_EXTENSION* driver_extension;
	UNICODE_STRING driver_name;
	UNICODE_STRING* hardware_database;
	void* fast_io_dispatch;
	PDRIVER_INITIALIZE driver_init;
	PDRIVER_STARTIO driver_start_io;
	PDRIVER_UNLOAD driver_unload;
	PDRIVER_DISPATCH major_function[IRP_MJ_MAXIMUM_FUNCTION + 1];
};

using PFN_NUMBER = u32;

// pages = (PFN_NUMBER*) (mdl + 1)
struct MDL {
	MDL* next;
	i16 size;
	i16 mdl_flags;
	Process* process;
	void* mapped_system_va;
	// in context of subject thread virt addr of buffer = start_va | byte_offset
	void* start_va;
	u32 byte_count;
	u32 byte_offset;
};

struct IORING_OBJECT;
struct FILE_OBJECT;

struct IO_STATUS_BLOCK {
	union {
		NTSTATUS status;
		void* pointer;
	};
	ULONG_PTR info;
};

using PIO_APC_ROUTINE = void (*)(void* apc_context, IO_STATUS_BLOCK* io_status_block, u32 reserved);
using PIO_COMPLETION_ROUTINE = NTSTATUS (*)(DEVICE_OBJECT* device, IRP* irp, void* ctx);

struct KDEVICE_QUEUE_ENTRY {
	LIST_ENTRY device_list_entry;
	u32 sort_key;
	bool inserted;
};

struct IO_SECURITY_CONTEXT;

enum class INTERFACE_TYPE {
	Undefined = -1
};

#pragma pack(push, 4)
struct CM_PARTIAL_RESOURCE_DESCRIPTOR {
	u8 type;
	u8 share_disposition;
	u16 flags;
	union {
		struct {
			PHYSICAL_ADDRESS start;
			u32 length;
		} generic;

		struct {
			PHYSICAL_ADDRESS start;
			u32 length;
		} port;

		struct {
			u16 level;
			// NT_PROCESSOR_GROUPS
			u16 group;
			u32 vector;
			KAFFINITY affinity;
		} interrupt;

		struct {
			union {
				struct {
					// NT_PROCESSOR_GROUPS
					u16 group;
					u16 message_count;
					u32 vector;
					KAFFINITY affinity;
				} raw;

				struct {
					u16 level;
					// NT_PROCESSOR_GROUPS
					u16 group;
					u32 vector;
					KAFFINITY affinity;
				} translated;
			};
		} message_interrupt;

		struct {
			PHYSICAL_ADDRESS start;
			u32 length;
		} memory;

		struct {
			u32 data[3];
		} device_private;

		struct {
			u32 start;
			u32 length;
			u32 reserved;
		} bus_number;

		struct {
			PHYSICAL_ADDRESS start;
			u32 length40;
		} memory40;

		struct {
			PHYSICAL_ADDRESS start;
			u32 length48;
		} memory48;

		struct {
			PHYSICAL_ADDRESS start;
			u32 length64;
		} memory64;
	} u;
};
#pragma pack(pop)

struct CM_PARTIAL_RESOURCE_LIST {
	u16 version;
	u16 revision;
	u32 count;
	CM_PARTIAL_RESOURCE_DESCRIPTOR partial_descriptors[];
};

struct CM_FULL_RESOURCE_DESCRIPTOR {
	// unused for wdm
	INTERFACE_TYPE interface_type;
	// unused for wdm
	u32 bus_number;
	CM_PARTIAL_RESOURCE_LIST PartialResourceList;
};

struct CM_RESOURCE_LIST {
	u32 count;
	CM_FULL_RESOURCE_DESCRIPTOR list[];
};

struct IO_STACK_LOCATION {
	u8 major_function;
	u8 minor_function;
	u8 flags;
	u8 control;
	union {
		// NtCreateFile
		struct {
			IO_SECURITY_CONTEXT* security_context;
			u32 options;
			[[gnu::aligned(8)]] u16 file_attributes;
			u16 share_access;
			[[gnu::aligned(8)]] u32 ea_length;
		} create;

		// NtReadFile
		struct {
			u32 length;
			[[gnu::aligned(8)]] u32 key;
			u32 flags;
			i64 byte_offset;
		} read;

		// NtWriteFile
		struct {
			u32 length;
			[[gnu::aligned(8)]] u32 key;
			u32 flags;
			i64 byte_offset;
		} write;

		// NtDeviceIoControlFile
		struct {
			u32 output_buffer_len;
			[[gnu::aligned(8)]] u32 input_buffer_len;
			[[gnu::aligned(8)]] u32 io_control_code;
			void* type3_input_buffer;
		} device_io_control;

		// StartDevice
		struct {
			CM_RESOURCE_LIST* allocated_resources;
			CM_RESOURCE_LIST* allocated_resources_translated;
		} start_device;
	} parameters;
	DEVICE_OBJECT* device;
	FILE_OBJECT* file;
	PIO_COMPLETION_ROUTINE completion_routine;
	void* ctx;
};

struct IRP {
	i16 type;
	u16 size;
	// memory descriptor list (used with direct io)
	MDL* mdl;
	u32 flags;
	union {
		// if this is associated irp, points to the main irp
		IRP* main_irp;
		// if this is main irp, contains the number of associated irps that must complete
		// before the main irp can complete
		volatile i32 irp_count;
		// if operating is buffered this is the address of the system space buffer
		void* system_buffer;
	} associated_irp;
	// used for queueing the irp to the thread's pending io request packet list
	LIST_ENTRY thread_list_entry;
	IO_STATUS_BLOCK io_status;
	KPROCESSOR_MODE requestor_mode;
	// true if pending was returned as the status initially
	bool pending_returned;
	i8 stack_count;
	i8 current_location;
	// canceled
	bool cancel;
	// irql at which the cancel spinlock was acquired
	KIRQL cancel_irql;
	// apc environment saved at the packet init time
	i8 apc_environment;
	u8 allocation_flags;
	union {
		IO_STATUS_BLOCK user_iosb;
		void* io_ring_context;
	};
	KEVENT* user_event;
	union {
		struct {
			union {
				PIO_APC_ROUTINE user_apc_routine;
				void* issuing_process;
			};
			union {
				void* user_apc_context;
				IORING_OBJECT* io_ring;
			};
		} asynchronous_parameters;
		i64 allocation_size;
	} overlay;
	PDRIVER_CANCEL cancel_routine;
	void* user_buffer;
	union {
		struct {
			union {
				KDEVICE_QUEUE_ENTRY device_queue_entry;
				struct {
					void* driver_context[4];
				};
			};
			Thread* thread;
			i8* auxiliary_buffer;
			struct {
				LIST_ENTRY list_entry;
				union {
					IO_STACK_LOCATION* current_stack_location;
					u32 packet_type;
				};
			};
			FILE_OBJECT* original_file_object;
		} overlay;
		KAPC apc;
		void* completion_key;
	} tail;
};

extern "C" NTSTATUS IoCreateDevice(
	DRIVER_OBJECT* driver,
	ULONG device_ext_size,
	PUNICODE_STRING device_name,
	DEVICE_TYPE type,
	ULONG characteristics,
	BOOLEAN exclusive,
	DEVICE_OBJECT* device);

extern "C" void IofCompleteRequest(IRP* irp, CCHAR priority_boost);
