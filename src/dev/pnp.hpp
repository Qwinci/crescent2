#pragma once
#include "types.hpp"
#include "sched/dpc.hpp"
#include "ntdef.h"
#include "utils/spinlock.hpp"
#include "sched/event.hpp"
#include "sched/apc.hpp"
#include "arch/irq.hpp"
#include "mem/mm.hpp"
#include "guiddef.h"

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

#define DO_BUFFERED_IO 4
#define DO_EXCLUSIVE 8
#define DO_DIRECT_IO 0x10
#define DO_MAP_IO_BUFFER 0x20
#define DO_BUS_ENUMERATED_DEVICE 0x1000

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

struct IrqInfo;

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
	IrqInfo* interrupts;
	i32 interrupt_count;
	void* verifier_ctx;
};

using PDRIVER_ADD_DEVICE = NTSTATUS (*)(DRIVER_OBJECT* driver, DEVICE_OBJECT* physical_device);

struct DRIVER_EXTENSION {
	DRIVER_OBJECT* driver;
	PDRIVER_ADD_DEVICE add_device;
	ULONG count;
	UNICODE_STRING service_key_name;
};

using PDRIVER_INITIALIZE = NTSTATUS (*)(DRIVER_OBJECT* driver, UNICODE_STRING* registry_path);
using PDRIVER_STARTIO = void (*)(DEVICE_OBJECT* device, IRP* irp);
using PDRIVER_UNLOAD = void (*)(DRIVER_OBJECT* driver);
using PDRIVER_DISPATCH = NTSTATUS (*)(DEVICE_OBJECT* device, IRP* irp);
using PDRIVER_CANCEL = void (*)(DEVICE_OBJECT* device, IRP* irp);

#define IRP_MJ_CREATE 0
#define IRP_MJ_CLOSE 2
#define IRP_MJ_READ 3
#define IRP_MJ_WRITE 4
#define IRP_MJ_FLUSH_BUFFERS 9
#define IRP_MJ_DEVICE_CONTROL 0xE
#define IRP_MJ_INTERNAL_DEVICE_CONTROL 0xF
#define IRP_MJ_SHUTDOWN 0x10
#define IRP_MJ_SYSTEM_CONTROL 0x17
#define IRP_MJ_PNP 0x1B
#define IRP_MJ_MAXIMUM_FUNCTION 0x1B

// pnp minor functions
#define IRP_MN_START_DEVICE 0
#define IRP_MN_QUERY_REMOVE_DEVICE 1
#define IRP_MN_REMOVE_DEVICE 2
#define IRP_MN_CANCEL_REMOVE_DEVICE 3
#define IRP_MN_STOP_DEVICE 4
#define IRP_MN_QUERY_STOP_DEVICE 5
#define IRP_MN_CANCEL_STOP_DEVICE 6

// bus
#define IRP_MN_QUERY_DEVICE_RELATIONS 7
#define IRP_MN_QUERY_INTERFACE 8
#define IRP_MN_QUERY_CAPABILITIES 9
#define IRP_MN_QUERY_RESOURCES 10
#define IRP_MN_QUERY_RESOURCE_REQUIREMENTS 11
#define IRP_MN_QUERY_DEVICE_TEXT 12
#define IRP_MN_FILTER_RESOURCE_REQUIREMENTS 13
#define IRP_MN_QUERY_ID 19
#define IRP_MN_QUERY_BUS_INFORMATION 21

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

#define SL_PENDING_RETURNED 1
#define SL_ERROR_RETURNED 2
#define SL_INVOKE_ON_CANCEL 0x20
#define SL_INVOKE_ON_SUCCESS 0x40
#define SL_INVOKE_ON_ERROR 0x80

struct KDEVICE_QUEUE_ENTRY {
	LIST_ENTRY device_list_entry;
	u32 sort_key;
	bool inserted;
};

struct IO_SECURITY_CONTEXT;

enum INTERFACE_TYPE {
	PCIBus = 5
};

struct PNP_BUS_INFORMATION {
	GUID bus_type_guid;
	INTERFACE_TYPE legacy_bus_type;
	ULONG bus_number;
};

#define CmResourceTypeNull 0
#define CmResourceTypePort 1
#define CmResourceTypeInterrupt 2
#define CmResourceTypeMemory 3
#define CmResourceTypeDma 4
#define CmResourceTypeDeviceSpecific 5
#define CmResourceTypeBusNumber 6
#define CmResourceTypeMemoryLarge 7

enum CM_SHARE_DISPOSITION {
	CmResourceShareUndetermined = 0,
	CmResourceShareDeviceExclusive,
	CmResourceShareDriverExclusive,
	CmResourceShareShared
};

#define CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE 0
#define CM_RESOURCE_INTERRUPT_LATCHED 1
#define CM_RESOURCE_INTERRUPT_MESSAGE 2
#define CM_RESOURCE_INTERRUPT_WAKE_HINT 0x20

#define CM_RESOURCE_INTERRUPT_MESSAGE_TOKEN ((ULONG) -2)

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

typedef enum _IRQ_PRIORITY {
	IrqPriorityUndefined = 0,
	IrqPriorityLow,
	IrqPriorityNormal,
	IrqPriorityHigh
} IRQ_PRIORITY;

typedef enum _IRQ_DEVICE_POLICY {
	IrqPolicyMachineDefault = 0,
	IrqPolicyAllCloseProcessors,
	IrqPolicyOneCloseProcessor,
	IrqPolicyAllProcessorsInMachine,
	IrqPolicySpecifiedProcessors,
	IrqPolicySpreadMessagesAcrossAllProcessors,
	IrqPolicyAllProcessorsInMachineWhenSteered
} IRQ_DEVICE_POLICY;

#define IO_RESOURCE_PREFERRED 1
#define IO_RESOURCE_DEFAULT 2
#define IO_RESOURCE_ALTERNATIVE 8

struct IO_RESOURCE_DESCRIPTOR {
	UCHAR option;
	UCHAR type;
	UCHAR share_disposition;
	UCHAR spare1;
	USHORT flags;
	USHORT spare2;
	union {
		struct {
			ULONG length;
			ULONG alignment;
			PHYSICAL_ADDRESS minimum_address;
			PHYSICAL_ADDRESS maximum_address;
		} port;

		struct {
			ULONG length;
			ULONG alignment;
			PHYSICAL_ADDRESS minimum_address;
			PHYSICAL_ADDRESS maximum_address;
		} memory;

		struct {
			ULONG minimum_vector;
			ULONG maximum_vector;
			IRQ_DEVICE_POLICY affinity_policy;
			USHORT group;
			IRQ_PRIORITY priority_policy;
			KAFFINITY targeted_processors;
		} interrupt;

		struct {
			ULONG length;
			ULONG alignment;
			PHYSICAL_ADDRESS minimum_address;
			PHYSICAL_ADDRESS maximum_address;
		} generic;

		struct {
			ULONG data[3];
		} device_private;

		struct {
			ULONG length40;
			ULONG alignment40;
			PHYSICAL_ADDRESS minimum_address;
			PHYSICAL_ADDRESS maximum_address;
		} memory40;

		struct {
			ULONG length48;
			ULONG alignment48;
			PHYSICAL_ADDRESS minimum_address;
			PHYSICAL_ADDRESS maximum_address;
		} memory48;

		struct {
			ULONG length64;
			ULONG alignment64;
			PHYSICAL_ADDRESS minimum_address;
			PHYSICAL_ADDRESS maximum_address;
		} memory64;
	} u;
};

struct IO_RESOURCE_LIST {
	USHORT version;
	USHORT revision;
	ULONG count;
	IO_RESOURCE_DESCRIPTOR descriptors[];
};

struct IO_RESOURCE_REQUIREMENTS_LIST {
	ULONG list_size;
	INTERFACE_TYPE interface_type;
	ULONG bus_number;
	ULONG slot_number;
	ULONG reserved[3];
	ULONG alternative_lists;
	IO_RESOURCE_LIST list[];
};

enum class DEVICE_RELATION_TYPE {
	Bus,
	Ejection,
	Power,
	Removal,
	TargetDevice,
	SingleBus,
	Transport
};

enum class BUS_QUERY_ID_TYPE {
	DeviceId = 0,
	HardwareIds = 1,
	CompatibleIds = 2,
	InstanceId = 3,
	DeviceSerialNumber = 4,
	ContainerId = 5
};

struct DEVICE_RELATIONS {
	ULONG count;
	DEVICE_OBJECT* objects[];
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

		// QueryDeviceRelations
		struct {
			DEVICE_RELATION_TYPE type;
		} query_device_relations;

		// FilterResourceRequirements
		struct {
			IO_RESOURCE_REQUIREMENTS_LIST* list;
		} filter_resource_requirements;

		// QueryId
		struct {
			BUS_QUERY_ID_TYPE id_type;
		} query_id;

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
		// if operation is buffered this is the address of the system space buffer
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
		IO_STATUS_BLOCK* user_iosb;
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

inline IO_STACK_LOCATION* IoGetCurrentIrpStackLocation(IRP* irp) {
	return irp->tail.overlay.current_stack_location;
}

inline void IoSkipCurrentIrpStackLocation(IRP* irp) {
	++irp->current_location;
	++irp->tail.overlay.current_stack_location;
}

inline IO_STACK_LOCATION* IoGetNextIrpStackLocation(IRP* irp) {
	return irp->tail.overlay.current_stack_location - 1;
}

inline void IoSetNextIrpStackLocation(IRP* irp) {
	--irp->current_location;
	--irp->tail.overlay.current_stack_location;
}

inline void IoMarkIrpPending(IRP* irp) {
	IoGetCurrentIrpStackLocation(irp)->control |= SL_PENDING_RETURNED;
}

inline USHORT IoSizeOfIrp(CCHAR stack_size) {
	return sizeof(IRP) + stack_size * sizeof(IO_STACK_LOCATION);
}

inline void IoSetCompletionRoutine(
	IRP* irp,
	PIO_COMPLETION_ROUTINE completion_routine,
	PVOID ctx,
	BOOLEAN invoke_on_success,
	BOOLEAN invoke_on_error,
	BOOLEAN invoke_on_cancel) {
	IO_STACK_LOCATION* slot = IoGetNextIrpStackLocation(irp);
	slot->completion_routine = completion_routine;
	slot->ctx = ctx;
	slot->control = 0;
	if (invoke_on_success) {
		slot->control = SL_INVOKE_ON_SUCCESS;
	}
	if (invoke_on_error) {
		slot->control |= SL_INVOKE_ON_ERROR;
	}
	if (invoke_on_cancel) {
		slot->control |= SL_INVOKE_ON_CANCEL;
	}
}

#define FILE_AUTOGENERATED_DEVICE_NAME 0x80

NTAPI extern "C" NTSTATUS IoCreateDevice(
	DRIVER_OBJECT* driver,
	ULONG device_ext_size,
	PUNICODE_STRING device_name,
	DEVICE_TYPE type,
	ULONG characteristics,
	BOOLEAN exclusive,
	DEVICE_OBJECT** device);

NTAPI extern "C" DEVICE_OBJECT* IoAttachDeviceToDeviceStack(DEVICE_OBJECT* source, DEVICE_OBJECT* target);
NTAPI extern "C" DEVICE_OBJECT* IoGetAttachedDevice(DEVICE_OBJECT* device);
NTAPI extern "C" DEVICE_OBJECT* IoGetAttachedDeviceReference(DEVICE_OBJECT* device);

NTAPI extern "C" DEVICE_OBJECT* IoGetLowerDeviceObject(DEVICE_OBJECT* device);

NTAPI extern "C" DEVICE_OBJECT* IoGetRelatedDeviceObject(FILE_OBJECT* file);

NTAPI extern "C" NTSTATUS IoGetDeviceObjectPointer(
	PUNICODE_STRING object_name,
	ACCESS_MASK desired_access,
	FILE_OBJECT** file_object,
	DEVICE_OBJECT** device);

enum DEVICE_REGISTRY_PROPERTY {
	DevicePropertyDeviceDescription = 0,
	DevicePropertyHardwareID = 1,
	DevicePropertyCompatibleIDs = 2,
	DevicePropertyBootConfiguration = 3,
	DevicePropertyBootConfigurationTranslated = 4,
	DevicePropertyClassName = 5,
	DevicePropertyClassGuid = 6,
	DevicePropertyDriverKeyName = 7,
	DevicePropertyManufacturer = 8,
	DevicePropertyFriendlyName = 9,
	DevicePropertyLocationInformation = 0xA,
	DevicePropertyPhysicalDeviceObjectName = 0xB,
	DevicePropertyBusTypeGuid = 0xC,
	DevicePropertyLegacyBusType = 0xD,
	DevicePropertyBusNumber = 0xE,
	DevicePropertyEnumeratorName = 0xF,
	DevicePropertyAddress = 0x10,
	DevicePropertyUINumber = 0x11,
	DevicePropertyInstallState = 0x12,
	DevicePropertyRemovalPolicy = 0x13,
	DevicePropertyResourceRequirements = 0x14,
	DevicePropertyAllocatedResources = 0x15,
	DevicePropertyContainerID = 0x16
};

NTAPI extern "C" NTSTATUS IoGetDeviceProperty(
	DEVICE_OBJECT* device,
	DEVICE_REGISTRY_PROPERTY device_property,
	ULONG buffer_length,
	PVOID property_buffer,
	PULONG result_length);

NTAPI extern "C" IRP* IoAllocateIrp(CCHAR stack_size, BOOLEAN charge_quota);
NTAPI extern "C" void IoFreeIrp(IRP* irp);
NTAPI extern "C" void IoInitializeIrp(IRP* irp, USHORT packet_size, CCHAR stack_size);

NTAPI extern "C" IRP* IoBuildSynchronousFsdRequest(
	ULONG major_function,
	DEVICE_OBJECT* device,
	PVOID buffer,
	ULONG length,
	LARGE_INTEGER* starting_offset,
	KEVENT* event,
	IO_STATUS_BLOCK* io_status_block);

NTAPI extern "C" NTSTATUS IofCallDriver(DEVICE_OBJECT* device, IRP* irp);
NTAPI extern "C" void IofCompleteRequest(IRP* irp, CCHAR priority_boost);

void enumerate_bus(DEVICE_OBJECT* device);
