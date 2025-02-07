#ifndef _WDM_H
#define _WDM_H

#include "ntdef.h"
#include "dpfilter.h"
#include "crt/string.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NTKERNELAPI
#define NTKERNELAPI __declspec(dllimport)
#endif

__attribute__((format(printf, 3, 4)))
NTKERNELAPI ULONG DbgPrintEx(ULONG ComponentId, ULONG Level, PCSTR Format, ...);

typedef ULONG POOL_FLAGS;

NTKERNELAPI PVOID ExAllocatePool2(POOL_FLAGS Flags, SIZE_T NumberOfBytes, ULONG Tag);
NTKERNELAPI void ExFreePool(PVOID P);

typedef enum _MEMORY_CACHING_TYPE {
	MmNonCached,
	MmCached,
	MmWriteCombined
} MEMORY_CACHING_TYPE;

NTKERNELAPI PVOID MmMapIoSpace(
	PHYSICAL_ADDRESS PhysicalAddress,
	SIZE_T NumberOfBytes,
	MEMORY_CACHING_TYPE CacheType);

typedef enum _BUS_DATA_TYPE {
	PCIConfiguration = 4
} BUS_DATA_TYPE;

typedef struct _PCI_SLOT_NUMBER {
	union {
		struct {
			ULONG DeviceNumber : 5;
			ULONG FunctionNumber : 3;
			ULONG Reserved : 24;
		} bits;
		ULONG AsULONG;
	} u;
} PCI_SLOT_NUMBER;

NTKERNELAPI ULONG HalGetBusDataByOffset(
	BUS_DATA_TYPE BusDataType,
	ULONG BusNumber,
	ULONG SlotNumber,
	PVOID Buffer,
	ULONG Offset,
	ULONG Length);
NTKERNELAPI ULONG HalSetBusDataByOffset(
	BUS_DATA_TYPE BusDataType,
	ULONG BusNumber,
	ULONG SlotNumber,
	PVOID Buffer,
	ULONG Offset,
	ULONG Length);

#define IRP_MJ_SHUTDOWN 0x10
#define IRP_MJ_POWER 0x16
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
#define IRP_MN_QUERY_DEVICE_RELATIONS 0x7
#define IRP_MN_QUERY_INTERFACE 8
#define IRP_MN_QUERY_CAPABILITIES 9
#define IRP_MN_QUERY_RESOURCES 10
#define IRP_MN_QUERY_RESOURCE_REQUIREMENTS 11
#define IRP_MN_QUERY_DEVICE_TEXT 12
#define IRP_MN_FILTER_RESOURCE_REQUIREMENTS 13

#define DECLSPEC_ALIGN(align_value) __declspec(align(align_value))

typedef CCHAR KPROCESSOR_MODE;

#ifdef _WIN64
#define MEMORY_ALLOCATION_ALIGNMENT 16
#else
#define MEMORY_ALLOCATION_ALIGNMENT 8
#endif

typedef enum _MODE {
	KernelMode,
	UserMode
} MODE;

typedef UCHAR KIRQL;

#define PASSIVE_LEVEL 0
#define LOW_LEVEL 0
#define APC_LEVEL 1
#define DISPATCH_LEVEL 2

typedef struct _DISPATCHER_HEADER {
	union {
		union {
			volatile LONG Lock;
			LONG LockNV;
		};

		struct {
			UCHAR Type;
			UCHAR Signalling;
			UCHAR Size;
			UCHAR Reserved0;
		};

		struct {
			UCHAR MutantType;
			UCHAR MutantSize;
			BOOLEAN DpcActive;
			UCHAR MutantReserved;
		};
	};
	LONG SignalState;
	LIST_ENTRY WaitListHead;
} DISPATCHER_HEADER;

typedef struct _KEVENT {
	DISPATCHER_HEADER Header;
} KEVENT, *PKEVENT;

typedef struct _MDL {

} MDL, *PMDL;

typedef struct _IO_STATUS_BLOCK {
	union {
		NTSTATUS Status;
		PVOID Pointer;
	};
	ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef struct _DRIVER_OBJECT DRIVER_OBJECT, *PDRIVER_OBJECT;

typedef ULONG DEVICE_TYPE;

typedef struct _IO_TIMER* PIO_TIMER;

#define MAXIMUM_VOLUME_LABEL_LENGTH (32 * sizeof(WCHAR))

typedef struct _VPB {
	CSHORT Type;
	CSHORT Size;
	USHORT Flags;
	USHORT VolumeLabelLength;
	struct _DEVICE_OBJECT* DeviceObject;
	struct _DEVICE_OBJECT* RealDevice;
	ULONG SerialNumber;
	ULONG ReferenceCount;
	WCHAR VolumeLabel[MAXIMUM_VOLUME_LABEL_LENGTH / sizeof(WCHAR)];
} VPB, *PVPB;

typedef ULONG_PTR KSPIN_LOCK, *PKSPIN_LOCK;

typedef struct _KDEVICE_QUEUE_ENTRY {
	LIST_ENTRY DeviceListEntry;
	ULONG SortKey;
	BOOLEAN Inserted;
} KDEVICE_QUEUE_ENTRY;

typedef struct _KDEVICE_QUEUE {
	CSHORT Type;
	CSHORT Size;
	LIST_ENTRY DeviceListHead;
	KSPIN_LOCK Lock;
#ifdef __x86_64__
	union {
		BOOLEAN Busy;
		struct {
			LONG64 Reserved : 8;
			LONG64 Hint : 56;
		};
	};
#else
	BOOLEAN Busy;
#endif
} KDEVICE_QUEUE;

typedef enum _IO_ALLOCATION_ACTION {
	KeepObject = 1,
	DeallocateObject,
	DeallocateObjectKeepRegisters
} IO_ALLOCATION_ACTION;

typedef IO_ALLOCATION_ACTION (*PDRIVER_CONTROL)(
	struct _DEVICE_OBJECT* DeviceObject,
	struct _IRP* Irp,
	PVOID MapRegisterBase,
	PVOID Context);
typedef void (*PKDEFERRED_ROUTINE)(
	struct _KDPC* Dpc,
	PVOID DeferredContext,
	PVOID SystemArgument1,
	PVOID SystemArgument2);

typedef struct _KDPC {
	union {
		ULONG TargetInfoAsUlong;
		struct {
			UCHAR Type;
			UCHAR Importance;
			volatile USHORT Number;
		};
	};
	SINGLE_LIST_ENTRY DpcListEntry;
	KAFFINITY ProcessorHistory;
	PKDEFERRED_ROUTINE DeferredRoutine;
	PVOID DeferredContext;
	PVOID SystemArgument1;
	PVOID SystemArgument2;
} KDPC, *PKDPC;

typedef struct _WAIT_CONTEXT_BLOCK {
	union {
		KDEVICE_QUEUE_ENTRY WaitQueueEntry;
		struct {
			LIST_ENTRY DmaWaitEntry;
			ULONG NumberOfChannels;
			ULONG SyncCallback : 1;
			ULONG DmaContext : 1;
			ULONG ZeroMapRegisters : 1;
			ULONG Reserved : 9;
			ULONG NumberOfRemapPages : 20;
		};
	};
	PDRIVER_CONTROL DeviceRoutine;
	PVOID DeviceContext;
	ULONG NumberOfMapRegisters;
	PVOID DeviceObject;
	PVOID CurrentIrp;
	PKDPC BufferChainingDpc;
} WAIT_CONTEXT_BLOCK;

typedef PVOID PSECURITY_DESCRIPTOR;

typedef struct DECLSPEC_ALIGN(MEMORY_ALLOCATION_ALIGNMENT) _DEVICE_OBJECT {
	CSHORT Type;
	USHORT Size;
	LONG ReferenceCount;
	DRIVER_OBJECT* DriverObject;
	struct _DEVICE_OBJECT* NextDevice;
	struct _DEVICE_OBJECT* AttachedDevice;
	struct _IRP* CurrentIrp;
	PIO_TIMER Timer;
	ULONG Flags;
	ULONG Characteristics;
	volatile PVPB Vpb;
	PVOID DeviceExtension;
	DEVICE_TYPE DeviceType;
	CCHAR StackSize;
	union {
		LIST_ENTRY ListEntry;
		WAIT_CONTEXT_BLOCK Wcb;
	} Queue;
	ULONG AlignmentRequirement;
	KDEVICE_QUEUE DeviceQueue;
	KDPC Dpc;
	ULONG ActiveThreadCount;
	PSECURITY_DESCRIPTOR SecurityDescriptor;
	KEVENT DeviceLock;
	USHORT SectorSize;
	USHORT Unused0;
	struct _DEVOBJ_EXTENSION* DeviceObjectExtension;
	PVOID Reserved;
} DEVICE_OBJECT, *PDEVICE_OBJECT;

typedef void (*PIO_APC_ROUTINE)(
	PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock,
	ULONG Reserved);
typedef void (*PDRIVER_CANCEL)(
	DEVICE_OBJECT* DeviceObject,
	struct _IRP* Irp);

typedef struct _ETHREAD* PETHREAD;

typedef struct _FILE_OBJECT {

} FILE_OBJECT, *PFILE_OBJECT;

typedef struct _IRP IRP, *PIRP;

typedef NTSTATUS (*PIO_COMPLETION_ROUTINE)(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp,
	PVOID Context);

typedef enum _DEVICE_RELATION_TYPE {
	BusRelations,
	EjectionRelations,
	PowerRelations,
	RemovalRelations,
	TargetDeviceRelations,
	SingleBusRelations,
	TransportRelations
} DEVICE_RELATION_TYPE;

typedef struct _DEVICE_RELATIONS {
	ULONG Count;
	PDEVICE_OBJECT Objects[];
} DEVICE_RELATIONS;

typedef enum _INTERFACE_TYPE {
	PCIBus = 5
} INTERFACE_TYPE;

#pragma pack(push, 4)
typedef struct _CM_PARTIAL_RESOURCE_DESCRIPTOR {
	UCHAR Type;
	UCHAR ShareDisposition;
	USHORT Flags;
	union {
		struct {
			PHYSICAL_ADDRESS Start;
			ULONG Length;
		} Generic;

		struct {
			PHYSICAL_ADDRESS Start;
			ULONG Length;
		} Port;

		struct {
			USHORT Level;
			USHORT Group;
			ULONG Vector;
			KAFFINITY Affinity;
		} Interrupt;

		struct {
			union {
				struct {
					USHORT Group;
					USHORT MessageCount;
					ULONG Vector;
					KAFFINITY Affinity;
				} Raw;

				struct {
					USHORT Level;
					USHORT Group;
					ULONG Vector;
					KAFFINITY Affinity;
				} Translated;
			};
		} MessageInterrupt;

		struct {
			PHYSICAL_ADDRESS Start;
			ULONG Length;
		} Memory;

		struct {
			ULONG Data[3];
		} DevicePrivate;

		struct {
			PHYSICAL_ADDRESS Start;
			ULONG Length40;
		} Memory40;

		struct {
			PHYSICAL_ADDRESS Start;
			ULONG Length48;
		} Memory48;

		struct {
			PHYSICAL_ADDRESS Start;
			ULONG Length64;
		} Memory64;
	};
} CM_PARTIAL_RESOURCE_DESCRIPTOR;
#pragma pack(pop)

typedef struct _CM_PARTIAL_RESOURCE_LIST {
	USHORT Version;
	USHORT Revision;
	ULONG Count;
	CM_PARTIAL_RESOURCE_DESCRIPTOR PartialDescriptors[];
} CM_PARTIAL_RESOURCE_LIST;

typedef struct _CM_FULL_RESOURCE_DESCRIPTOR {
	INTERFACE_TYPE InterfaceType;
	ULONG BusNumber;
	CM_PARTIAL_RESOURCE_LIST PartialResourceList;
} CM_FULL_RESOURCE_DESCRIPTOR;

typedef struct _CM_RESOURCE_LIST {
	ULONG Count;
	CM_FULL_RESOURCE_DESCRIPTOR List[];
} CM_RESOURCE_LIST, *PCM_RESOURCE_LIST;

typedef struct _IO_STACK_LOCATION {
	UCHAR MajorFunction;
	UCHAR MinorFunction;
	UCHAR Flags;
	UCHAR Control;
	union {
		struct {
			DEVICE_RELATION_TYPE Type;
		} QueryDeviceRelations;

		struct {
			PCM_RESOURCE_LIST AllocatedResources;
			PCM_RESOURCE_LIST AllocatedResourcesTranslated;
		} StartDevice;

		struct {
			PVOID Argument1;
			PVOID Argument2;
			PVOID Argument3;
			PVOID Argument4;
		} Others;
	} Parameters;
	PDEVICE_OBJECT DeviceObject;
	PFILE_OBJECT FileObject;
	PIO_COMPLETION_ROUTINE CompletionRoutine;
	PVOID Context;
} IO_STACK_LOCATION, *PIO_STACK_LOCATION;

typedef struct _KAPC {
	UCHAR Type;
	UCHAR AllFlags;
	UCHAR Size;
	UCHAR Unused0;
	ULONG Unused1;
	struct _ETHREAD* Thread;
	LIST_ENTRY ApcListEntry;
	PVOID Reserved[3];
	PVOID NormalContext;
	PVOID SystemArgument1;
	PVOID SystemArgument2;
	CCHAR ApcStateIndex;
	KPROCESSOR_MODE ApcMode;
	BOOLEAN Inserted;
} KAPC;

struct DECLSPEC_ALIGN(MEMORY_ALLOCATION_ALIGNMENT) _IRP {
	CSHORT Type;
	USHORT Size;
	PMDL MdlAddress;
	ULONG Flags;
	union {
		struct _IRP* MasterIrp;
		volatile LONG IrpCount;
		PVOID SystemBuffer;
	} AssociatedIrp;
	LIST_ENTRY ThreadListEntry;
	IO_STATUS_BLOCK IoStatus;
	KPROCESSOR_MODE RequestorMode;
	BOOLEAN PendingReturned;
	CHAR StackCount;
	CHAR CurrentLocation;
	BOOLEAN Cancel;
	KIRQL CancelIrql;
	CCHAR ApcEnvironment;
	UCHAR AllocationFlags;
	union {
		PIO_STATUS_BLOCK UserIosb;
		PVOID IoRingContext;
	};
	PKEVENT UserEvent;
	union {
		struct {
			union {
				PIO_APC_ROUTINE UserApcRoutine;
				PVOID IssuingProcess;
			};
			union {
				PVOID UserApcContext;
				struct _IORING_OBJECT* IoRing;
			};
		} AsynchronousParameters;
		LARGE_INTEGER AllocationSize;
	} Overlay;
	volatile PDRIVER_CANCEL CancelRoutine;
	PVOID UserBuffer;
	union {
		struct {
			union {
				KDEVICE_QUEUE_ENTRY DeviceQueueEntry;
				struct {
					PVOID DriverContext[4];
				};
			};
			PETHREAD Thread;
			PCHAR AuxiliaryBuffer;
			struct {
				LIST_ENTRY ListEntry;
				union {
					IO_STACK_LOCATION* CurrentStackLocation;
					ULONG PacketType;
				};
			};
			PFILE_OBJECT OriginalFileObject;
		} Overlay;
		KAPC Apc;
		PVOID CompletionKey;
	} Tail;
};

typedef NTSTATUS (*PDRIVER_ADD_DEVICE)(
	DRIVER_OBJECT* DriverObject,
	DEVICE_OBJECT* PhysicalDeviceObject);

typedef struct _DRIVER_EXTENSION {
	DRIVER_OBJECT* DriverObject;
	PDRIVER_ADD_DEVICE AddDevice;
	ULONG Count;
	UNICODE_STRING ServiceKeyName;
} DRIVER_EXTENSION, *PDRIVER_EXTENSION;

typedef NTSTATUS (*PDRIVER_INITIALIZE)(
	DRIVER_OBJECT* DriverObject,
	PUNICODE_STRING RegistryPath);
typedef void (*PDRIVER_STARTIO)(
	DEVICE_OBJECT* DeviceObject,
	IRP* Irp);
typedef void (*PDRIVER_UNLOAD)(DRIVER_OBJECT* DriverObject);
typedef NTSTATUS (*PDRIVER_DISPATCH)(
	DEVICE_OBJECT* DeviceObject,
	IRP* Irp);

struct _DRIVER_OBJECT {
	CSHORT Type;
	CSHORT Size;
	// list of device objects created by driver
	PDEVICE_OBJECT DeviceObject;
	ULONG Flags;
	// driver executable start/size/section
	PVOID DriverStart;
	ULONG DriverSize;
	PVOID DriverSection;
	PDRIVER_EXTENSION DriverExtension;
	UNICODE_STRING DriverName;
	PUNICODE_STRING HardwareDatabase;
	void* FastIoDispatch;
	PDRIVER_INITIALIZE DriverInit;
	PDRIVER_STARTIO DriverStartIo;
	PDRIVER_UNLOAD DriverUnload;
	PDRIVER_DISPATCH MajorFunction[IRP_MJ_MAXIMUM_FUNCTION + 1];
};

#define FILE_AUTOGENERATED_DEVICE_NAME 0x80

NTKERNELAPI NTSTATUS IoCreateDevice(
	PDRIVER_OBJECT DriverObject,
	ULONG DeviceExtensionSize,
	PUNICODE_STRING DeviceName,
	DEVICE_TYPE DeviceType,
	ULONG DeviceCharacteristics,
	BOOLEAN Exclusive,
	PDEVICE_OBJECT* DeviceObject);

FORCEINLINE PIO_STACK_LOCATION IoGetCurrentIrpStackLocation(PIRP Irp) {
	return Irp->Tail.Overlay.CurrentStackLocation;
}

#define IO_NO_INCREMENT 0
#define IO_DISK_INCREMENT 1
#define IO_VIDEO_INCREMENT 1
#define IO_NETWORK_INCREMENT 2
#define IO_KEYBOARD_INCREMENT 6
#define IO_MOUSE_INCREMENT 6
#define IO_SOUND_INCREMENT 8

NTKERNELAPI void IofCompleteRequest(PIRP Irp, CCHAR PriorityBoost);

#define IoCompleteRequest(Irp, PriorityBoost) IofCompleteRequest(Irp, PriorityBoost)

typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemFirmwareTableInformation = 76
} SYSTEM_INFORMATION_CLASS;

NTKERNELAPI NTSTATUS ZwQuerySystemInformation(
	SYSTEM_INFORMATION_CLASS SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength);

#ifdef __cplusplus
}
#endif

#endif
