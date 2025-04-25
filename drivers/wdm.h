#ifndef _WDM_H
#define _WDM_H

#include "ntdef.h"
#include "dpfilter.h"
#include "crt/string.h"
#include "crt/assert.h"
#include "guiddef.h"
#include <stddef.h>

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

NTKERNELAPI void ObfReferenceObject(PVOID Object);
NTKERNELAPI void ObfDereferenceObject(PVOID Object);

#define ObReferenceObject(Object) ObfReferenceObject(Object)
#define ObDereferenceObject(Object) ObfDereferenceObject(Object)

typedef enum _MEMORY_CACHING_TYPE {
	MmNonCached,
	MmCached,
	MmWriteCombined
} MEMORY_CACHING_TYPE;

NTKERNELAPI PVOID MmMapIoSpace(
	PHYSICAL_ADDRESS PhysicalAddress,
	SIZE_T NumberOfBytes,
	MEMORY_CACHING_TYPE CacheType);
NTKERNELAPI void MmUnmapIoSpace(PVOID BaseAddress, SIZE_T NumberOfBytes);

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

typedef struct _PCI_COMMON_HEADER {
	USHORT VendorID;
	USHORT DeviceID;
	USHORT Command;
	USHORT Status;
	UCHAR RevisionID;
	UCHAR ProgIf;
	UCHAR SubClass;
	UCHAR BaseClass;
	UCHAR CacheLineSize;
	UCHAR LatencyTimer;
	UCHAR HeaderType;
	UCHAR BIST;
	union {
		struct _PCI_HEADER_TYPE_0 {
			ULONG BaseAddresses[6];
			ULONG CIS;
			USHORT SubVendorID;
			USHORT SubSystemID;
			ULONG ROMBaseAddress;
			UCHAR CapabilitiesPtr;
			UCHAR Reserved1[3];
			ULONG Reserved2;
			UCHAR InterruptLine;
			UCHAR InterruptPin;
			UCHAR MinimumGrant;
			UCHAR MaximumLatency;
		} type0;

		// pci to pci bridge
		struct _PCI_HEADER_TYPE_1 {
			ULONG BaseAddresses[2];
			UCHAR PrimaryBus;
			UCHAR SecondaryBus;
			UCHAR SubordinateBus;
			UCHAR SecondaryLatency;
			UCHAR IOBase;
			UCHAR IOLimit;
			USHORT SecondaryStatus;
			USHORT MemoryBase;
			USHORT MemoryLimit;
			USHORT PrefetchBase;
			USHORT PrefetchLimit;
			ULONG PrefetchBaseUpper32;
			ULONG PrefetchLimitUpper32;
			USHORT IOBaseUpper16;
			USHORT IOLimitUpper16;
			UCHAR CapabilitiesPtr;
			UCHAR Reserved1[3];
			ULONG ROMBaseAddress;
			UCHAR InterruptLine;
			UCHAR InterruptPin;
			USHORT BridgeControl;
		} type1;

		// pci to cardbus bridge
		struct _PCI_HEADER_TYPE_2 {
			ULONG SocketRegistersBaseAddress;
			UCHAR CapabilitiesPtr;
			UCHAR Reserved;
			USHORT SecondaryStatus;
			UCHAR PrimaryBus;
			UCHAR SecondaryBus;
			UCHAR SubordinateBus;
			UCHAR SecondaryLatency;
			struct {
				ULONG Base;
				ULONG Limit;
			} Range[4];
			UCHAR InterruptLine;
			UCHAR InterruptPin;
			USHORT BridgeControl;
		} type2;
	} u;
} PCI_COMMON_HEADER;

#ifdef __cplusplus

typedef struct _PCI_COMMON_CONFIG : PCI_COMMON_HEADER {
	UCHAR DeviceSpecific[192];
} PCI_COMMON_CONFIG;

#else

typedef struct _PCI_COMMON_CONFIG {
	PCI_COMMON_HEADER;
	UCHAR DeviceSpecific[192];
} PCI_COMMON_CONFIG;

#endif

// PCI_COMMON_CONFIG.Command
#define PCI_ENABLE_IO_SPACE 1
#define PCI_ENABLE_MEMORY_SPACE 2
#define PCI_ENABLE_BUS_MASTER 4
#define PCI_ENABLE_SPECIAL_CYCLES 8
#define PCI_ENABLE_WRITE_AND_INVALIDATE 0x10
#define PCI_ENABLE_VGA_COMPATIBLE_PALETTE 0x20
#define PCI_ENABLE_PARITY 0x40
#define PCI_ENABLE_WAIT_CYCLE 0x80
#define PCI_ENABLE_SERR 0x100
#define PCI_ENABLE_FAST_BACK_TO_BACK 0x200
#define PCI_DISABLE_LEVEL_INTERRUPT 0x400

#define PCI_CAPABILITY_ID_POWER_MANAGEMENT 1
#define PCI_CAPABILITY_ID_MSI 5
#define PCI_CAPABILITY_ID_PCI_EXPRESS 0x10
#define PCI_CAPABILITY_ID_MSIX 0x11

typedef struct _PCI_CAPABILITIES_HEADER {
	UCHAR CapabilityID;
	UCHAR Next;
} PCI_CAPABILITIES_HEADER;

// Power Management Capability
typedef struct _PCI_PMC {
	UCHAR Version : 3;
	UCHAR PMEClock : 1;
	UCHAR Rsvd1 : 1;
	UCHAR DeviceSpecificInitialization : 1;
	UCHAR Rsvd2 : 2;
	struct _PM_SUPPORT {
		UCHAR Rsvd2 : 1;
		UCHAR D1 : 1;
		UCHAR D2 : 1;
		UCHAR PMED0 : 1;
		UCHAR PMED1 : 1;
		UCHAR PMED2 : 1;
		UCHAR PMED3Hot : 1;
		UCHAR PMED3Cold : 1;
	} Support;
} PCI_PMC;

typedef struct _PCI_PMCSR {
	USHORT PowerState : 2;
	USHORT Rsvd1 : 1;
	USHORT NoSoftReset : 1;
	USHORT Rsvd2 : 4;
	USHORT PMEnable : 1;
	USHORT DataSelect : 4;
	USHORT DataScale : 2;
	USHORT PMEStatus : 1;
} PCI_PMCSR;

typedef struct _PCI_PMCSR_BSE {
	UCHAR Rsvd1 : 6;
	// B2_B3#
	UCHAR D3HotSupportsStopClock : 1;
	// BPCC_EN
	UCHAR BusPowerClockControlEnabled : 1;
} PCI_PMCSR_BSE;

typedef struct _PCI_PM_CAPABILITY {
	PCI_CAPABILITIES_HEADER Header;
	union {
		PCI_PMC Capabilities;
		USHORT AsUSHORT;
	} PMC;
	union {
		PCI_PMCSR ControlStatus;
		USHORT AsUSHORT;
	} PMCSR;
	union {
		PCI_PMCSR_BSE BridgeSupport;
		UCHAR AsUCHAR;
	} PMCSR_BSE;
	UCHAR Data;
} PCI_PM_CAPABILITY;

typedef struct _PCI_MSI_CAPABILITY {
	PCI_CAPABILITIES_HEADER Header;
	struct _PCI_MSI_MESSAGE_CONTROL {
		USHORT MSIEnable : 1;
		USHORT MultipleMessageCapable : 3;
		USHORT MultipleMessageEnable : 3;
		USHORT CapableOf64Bits : 1;
		USHORT PerVectorMaskCapable : 1;
		USHORT Reserved : 7;
	} MessageControl;
	union {
		struct _PCI_MSI_MESSAGE_ADDRESS {
			ULONG Reserved : 2;
			ULONG Address : 30;
		} Register;
		ULONG Raw;
	} MessageAddressLower;

	union {
		struct {
			USHORT MessageData;
			USHORT Reserved;
			ULONG MaskBits;
			ULONG PendingBits;
		} Option32Bit;
		// if CapableOf64Bits is 1
		struct {
			ULONG MessageAddressUpper;
			USHORT MessageData;
			USHORT Reserved;
			ULONG MaskBits;
			ULONG PendingBits;
		} Option64Bit;
	};
} PCI_MSI_CAPABILITY;

#define MSIX_TABLE_OFFSET_MASK 0xFFFFFFF8
#define MSIX_PBA_TABLE_ENTRY_SIZE 8
#define MSIX_PENDING_BITS_IN_PBA_TABLE_ENTRY 64

typedef struct {
	union {
		struct {
			ULONG BaseIndexRegister : 3;
			ULONG Reserved : 29;
		};
		ULONG TableOffset;
	};
} MSIX_TABLE_POINTER;

typedef struct {
	PHYSICAL_ADDRESS MessageAddress;
	ULONG MessageData;
	struct {
		ULONG Mask : 1;
		ULONG Reserved : 15;
		ULONG StLower : 8;
		ULONG StUpper : 8;
	} VectorControl;
} PCI_MSIX_TABLE_ENTRY;

typedef struct {
	PCI_CAPABILITIES_HEADER Header;
	struct {
		USHORT TableSize : 11;
		USHORT Reserved : 3;
		USHORT FunctionMask : 1;
		USHORT MSIXEnable : 1;
	} MessageControl;
	MSIX_TABLE_POINTER MessageTable;
	MSIX_TABLE_POINTER PBATable;
} PCI_MSIX_CAPABILITY;

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

#define IRP_MJ_DEVICE_CONTROL 0xE
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
#define IRP_MN_QUERY_ID 19
#define IRP_MN_QUERY_BUS_INFORMATION 21

#define CTL_CODE(DeviceType, Function, Method, Access) \
	((ULONG) ((DeviceType) << 16 | (Access) << 14 | (Function) << 2 | (Method)))
#define DEVICE_TYPE_FROM_CTL_CODE(Code) (((ULONG) (Code) >> 16) & 0xFFFF)
#define METHOD_FROM_CTL_CODE(Code) ((ULONG) (Code) & 3)

#define METHOD_BUFFERED 0
#define METHOD_IN_DIRECT 1
#define METHOD_OUT_DIRECT 2
#define METHOD_NEITHER 3

#define METHOD_DIRECT_TO_HARDWARE METHOD_IN_DIRECT
#define METHOD_DIRECT_FROM_HARDWARE METHOD_OUT_DIRECT

#define FILE_ANY_ACCESS 0
#define FILE_SPECIAL_ACCESS FILE_ANY_ACCESS
#define FILE_READ_ACCESS 1
#define FILE_WRITE_ACCESS 2

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
#define HIGH_LEVEL 15

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
	struct _MDL* Next;
	CSHORT Size;
	CSHORT MdlFlags;
	struct _EPROCESS* Process;
	PVOID MappedSystemVa;
	PVOID StartVa;
	ULONG ByteCount;
	ULONG ByteOffset;
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

typedef struct _SECTION_OBJECT_POINTERS {
	PVOID DataSectionObject;
	PVOID SharedCacheMap;
	PVOID ImageSectionObject;
} SECTION_OBJECT_POINTERS, *PSECTION_OBJECT_POINTERS;

typedef struct _IO_COMPLETION_CONTEXT {
	PVOID Port;
	PVOID Key;
	LONG_PTR UsageCount;
} IO_COMPLETION_CONTEXT, *PIO_COMPLETION_CONTEXT;

typedef struct _FILE_OBJECT {
	CSHORT Type;
	CSHORT Size;
	PDEVICE_OBJECT DeviceObject;
	PVPB Vpb;
	PVOID FsContext;
	PVOID FsContext2;
	PSECTION_OBJECT_POINTERS SectionObjectPointer;
	PVOID PrivateCacheMap;
	NTSTATUS FinalStatus;
	struct _FILE_OBJECT* RelatedFileObject;
	BOOLEAN LockOperation;
	BOOLEAN DeletePending;
	BOOLEAN ReadAccess;
	BOOLEAN WriteAccess;
	BOOLEAN DeleteAccess;
	BOOLEAN SharedRead;
	BOOLEAN SharedWrite;
	BOOLEAN SharedDelete;
	ULONG Flags;
	UNICODE_STRING FileName;
	LARGE_INTEGER CurrentByteOffset;
	volatile ULONG Waiters;
	volatile ULONG Busy;
	PVOID LastLock;
	KEVENT Lock;
	KEVENT Event;
	volatile PIO_COMPLETION_CONTEXT CompletionContext;
	KSPIN_LOCK IrpListLock;
	LIST_ENTRY IrpList;
	volatile PVOID FileObjectExtension;
} FILE_OBJECT, *PFILE_OBJECT;

typedef struct _IRP IRP, *PIRP;

typedef NTSTATUS (*PIO_COMPLETION_ROUTINE)(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp,
	PVOID Context);

// IO_STACK_LOCATION control flags
#define SL_PENDING_RETURNED 1
#define SL_ERROR_RETURNED 2
#define SL_INVOKE_ON_CANCEL 0x20
#define SL_INVOKE_ON_SUCCESS 0x40
#define SL_INVOKE_ON_ERROR 0x80

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

typedef struct _PNP_BUS_INFORMATION {
	GUID BusTypeGuid;
	INTERFACE_TYPE LegacyBusType;
	ULONG BusNumber;
} PNP_BUS_INFORMATION;

#define CmResourceTypeNull 0
#define CmResourceTypePort 1
#define CmResourceTypeInterrupt 2
#define CmResourceTypeMemory 3
#define CmResourceTypeDma 4
#define CmResourceTypeDeviceSpecific 5
#define CmResourceTypeBusNumber 6
#define CmResourceTypeMemoryLarge 7

typedef enum _CM_SHARE_DISPOSITION {
	CmResourceShareUndetermined = 0,
	CmResourceShareDeviceExclusive,
	CmResourceShareDriverExclusive,
	CmResourceShareShared
} CM_SHARE_DISPOSITION;

#define CM_RESOURCE_INTERRUPT_LEVEL_SENSITIVE 0
#define CM_RESOURCE_INTERRUPT_LATCHED 1
#define CM_RESOURCE_INTERRUPT_MESSAGE 2
#define CM_RESOURCE_INTERRUPT_WAKE_HINT 0x20

#define CM_RESOURCE_INTERRUPT_LEVEL_LATCHED_BITS 1
#define CM_RESOURCE_INTERRUPT_MESSAGE_TOKEN ((ULONG) -2)

#define CM_RESOURCE_MEMORY_READ_WRITE 0
#define CM_RESOURCE_MEMORY_READ_ONLY 1
#define CM_RESOURCE_MEMORY_WRITE_ONLY 2
#define CM_RESOURCE_MEMORY_WRITEABILITY_MASK 3
#define CM_RESOURCE_MEMORY_PREFETCHABLE 4

#define CM_RESOURCE_MEMORY_COMBINEDWRITE 8
#define CM_RESOURCE_MEMORY_24 0x10
#define CM_RESOURCE_MEMORY_CACHEABLE 0x20
#define CM_RESOURCE_MEMORY_WINDOW_DECODE 0x40
#define CM_RESOURCE_MEMORY_BAR 0x80

#define CM_RESOURCE_MEMORY_LARGE 0xE00
#define CM_RESOURCE_MEMORY_LARGE_40 0x200
#define CM_RESOURCE_MEMORY_LARGE_48 0x400
#define CM_RESOURCE_MEMORY_LARGE_64 0x800

#define CM_RESOURCE_MEMORY_LARGE_40_MAXLEN 0xFFFFFFFF00
#define CM_RESOURCE_MEMORY_LARGE_48_MAXLEN 0xFFFFFFFF0000
#define CM_RESOURCE_MEMORY_LARGE_64_MAXLEN 0xFFFFFFFF00000000

#define CM_RESOURCE_PORT_MEMORY 0
#define CM_RESOURCE_PORT_IO 1
#define CM_RESOURCE_PORT_10_BIT_DECODE 4
#define CM_RESOURCE_PORT_12_BIT_DECODE 8
#define CM_RESOURCE_PORT_16_BIT_DECODE 0x10
#define CM_RESOURCE_PORT_POSITIVE_DECODE 0x20
#define CM_RESOURCE_PORT_PASSIVE_DECODE 0x40
#define CM_RESOURCE_PORT_WINDOW_DECODE 0x80
#define CM_RESOURCE_PORT_BAR 0x100

#define ALL_PROCESSOR_GROUPS 0xFFFF

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
	} u;
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

typedef struct _IO_RESOURCE_DESCRIPTOR {
	UCHAR Option;
	UCHAR Type;
	UCHAR ShareDisposition;
	UCHAR Spare1;
	USHORT Flags;
	USHORT Spare2;
	union {
		struct {
			ULONG Length;
			ULONG Alignment;
			PHYSICAL_ADDRESS MinimumAddress;
			PHYSICAL_ADDRESS MaximumAddress;
		} Port;

		struct {
			ULONG Length;
			ULONG Alignment;
			PHYSICAL_ADDRESS MinimumAddress;
			PHYSICAL_ADDRESS MaximumAddress;
		} Memory;

		struct {
			ULONG MinimumVector;
			ULONG MaximumVector;
			IRQ_DEVICE_POLICY AffinityPolicy;
			USHORT Group;
			IRQ_PRIORITY PriorityPolicy;
			KAFFINITY TargetedProcessors;
		} Interrupt;

		struct {
			ULONG Length;
			ULONG Alignment;
			PHYSICAL_ADDRESS MinimumAddress;
			PHYSICAL_ADDRESS MaximumAddress;
		} Generic;

		struct {
			ULONG Data[3];
		} DevicePrivate;

		struct {
			ULONG Length40;
			ULONG Alignment40;
			PHYSICAL_ADDRESS MinimumAddress;
			PHYSICAL_ADDRESS MaximumAddress;
		} Memory40;

		struct {
			ULONG Length48;
			ULONG Alignment48;
			PHYSICAL_ADDRESS MinimumAddress;
			PHYSICAL_ADDRESS MaximumAddress;
		} Memory48;

		struct {
			ULONG Length64;
			ULONG Alignment64;
			PHYSICAL_ADDRESS MinimumAddress;
			PHYSICAL_ADDRESS MaximumAddress;
		} Memory64;
	} u;
} IO_RESOURCE_DESCRIPTOR, *PIO_RESOURCE_DESCRIPTOR;

typedef struct _IO_RESOURCE_LIST {
	USHORT Version;
	USHORT Revision;
	ULONG Count;
	IO_RESOURCE_DESCRIPTOR Descriptors[];
} IO_RESOURCE_LIST;

typedef struct _IO_RESOURCE_REQUIREMENTS_LIST {
	ULONG ListSize;
	INTERFACE_TYPE InterfaceType;
	ULONG BusNumber;
	ULONG SlotNumber;
	ULONG Reserved[3];
	ULONG AlternativeLists;
	IO_RESOURCE_LIST List[];
} IO_RESOURCE_REQUIREMENTS_LIST, *PIO_RESOURCE_REQUIREMENTS_LIST;

typedef enum {
	BusQueryDeviceID = 0,
	BusQueryHardwareIDs = 1,
	BusQueryCompatibleIDs = 2,
	BusQueryInstanceID = 3,
	BusQueryDeviceSerialNumber = 4,
	BusQueryContainerID = 5
} BUS_QUERY_ID_TYPE, *PBUS_QUERY_ID_TYPE;

#define MAX_DEVICE_ID_LEN 200
#define REGSTR_VAL_MAX_HCID_LEN 1024

#ifdef _WIN64
#define POINTER_ALIGNMENT DECLSPEC_ALIGN(8)
#else
#define POINTER_ALIGNMENT
#endif

typedef struct _IO_STACK_LOCATION {
	UCHAR MajorFunction;
	UCHAR MinorFunction;
	UCHAR Flags;
	UCHAR Control;
	union {
		// if method != METHOD_NEITHER:
		// user output buffer == UserBuffer
		// user input buffer == SystemBuffer
		// driver reads from SystemBuffer and writes to SystemBuffer
		// else
		// user output buffer == UserBuffer
		// user input buffer == Type3InputBuffer
		struct {
			ULONG OutputBufferLength;
			ULONG POINTER_ALIGNMENT InputBufferLength;
			ULONG POINTER_ALIGNMENT IoControlCode;
			PVOID Type3InputBuffer;
		} DeviceIoControl;

		struct {
			DEVICE_RELATION_TYPE Type;
		} QueryDeviceRelations;

		struct {
			BUS_QUERY_ID_TYPE IdType;
		} QueryId;

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

#define IoSizeOfIrp(StackSize) \
	(USHORT) (sizeof(IRP) + (StackSize) * sizeof(IO_STACK_LOCATION))

FORCEINLINE PIO_STACK_LOCATION IoGetCurrentIrpStackLocation(PIRP Irp) {
	return Irp->Tail.Overlay.CurrentStackLocation;
}

FORCEINLINE void IoSkipCurrentIrpStackLocation(PIRP Irp) {
	++Irp->CurrentLocation;
	++Irp->Tail.Overlay.CurrentStackLocation;
}

FORCEINLINE PIO_STACK_LOCATION IoGetNextIrpStackLocation(PIRP Irp) {
	return Irp->Tail.Overlay.CurrentStackLocation - 1;
}

FORCEINLINE void IoSetNextIrpStackLocation(PIRP Irp) {
	--Irp->CurrentLocation;
	--Irp->Tail.Overlay.CurrentStackLocation;
}

FORCEINLINE void IoMarkIrpPending(PIRP Irp) {
	IoGetCurrentIrpStackLocation(Irp)->Control |= SL_PENDING_RETURNED;
}

FORCEINLINE void IoSetCompletionRoutine(
	PIRP Irp,
	PIO_COMPLETION_ROUTINE CompletionRoutine,
	PVOID Context,
	BOOLEAN InvokeOnSuccess,
	BOOLEAN InvokeOnError,
	BOOLEAN InvokeOnCancel) {
	PIO_STACK_LOCATION slot = IoGetNextIrpStackLocation(Irp);
	slot->CompletionRoutine = CompletionRoutine;
	slot->Context = Context;
	slot->Control = 0;
	if (InvokeOnSuccess) {
		slot->Control = SL_INVOKE_ON_SUCCESS;
	}
	if (InvokeOnError) {
		slot->Control |= SL_INVOKE_ON_ERROR;
	}
	if (InvokeOnCancel) {
		slot->Control |= SL_INVOKE_ON_CANCEL;
	}
}

#define IO_NO_INCREMENT 0
#define IO_DISK_INCREMENT 1
#define IO_VIDEO_INCREMENT 1
#define IO_NETWORK_INCREMENT 2
#define IO_KEYBOARD_INCREMENT 6
#define IO_MOUSE_INCREMENT 6
#define IO_SOUND_INCREMENT 8

NTKERNELAPI PIRP IoAllocateIrp(CCHAR StackSize, BOOLEAN ChargeQuota);
NTKERNELAPI void IoFreeIrp(PIRP Irp);
NTKERNELAPI void IoInitializeIrp(PIRP Irp, USHORT PacketSize, CCHAR StackSize);

NTKERNELAPI NTSTATUS IofCallDriver(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTKERNELAPI void IofCompleteRequest(PIRP Irp, CCHAR PriorityBoost);

#define IoCallDriver(DeviceObject, Irp) IofCallDriver(DeviceObject, Irp)
#define IoCompleteRequest(Irp, PriorityBoost) IofCompleteRequest(Irp, PriorityBoost)

typedef struct _KINTERRUPT* PKINTERRUPT;

typedef enum _KINTERRUPT_MODE {
	LevelSensitive,
	Latched
} KINTERRUPT_MODE;

typedef enum _KINTERRUPT_POLARITY {
	InterruptPolarityUnknown,
	InterruptActiveHigh,
	InterruptRisingEdge = InterruptActiveHigh,
	InterruptActiveLow,
	InterruptFallingEdge = InterruptActiveLow,
	InterruptActiveBoth,
	InterruptActiveBothTriggerLow = InterruptActiveBoth,
	InterruptActiveBothTriggerHigh
} KINTERRUPT_POLARITY;

typedef struct _IO_INTERRUPT_MESSAGE_INFO_ENTRY {
	PHYSICAL_ADDRESS MessageAddress;
	KAFFINITY TargetProcessorSet;
	PKINTERRUPT InterruptObject;
	ULONG MessageData;
	ULONG Vector;
	KIRQL Irql;
	KINTERRUPT_MODE Mode;
	KINTERRUPT_POLARITY Polarity;
} IO_INTERRUPT_MESSAGE_INFO_ENTRY;

typedef struct _IO_INTERRUPT_MESSAGE_INFO {
	KIRQL UnifiedIrql;
	ULONG MessageCount;
	IO_INTERRUPT_MESSAGE_INFO_ENTRY MessageInfo[];
} IO_INTERRUPT_MESSAGE_INFO, *PIO_INTERRUPT_MESSAGE_INFO;

typedef BOOLEAN (*PKSERVICE_ROUTINE)(
	struct _KINTERRUPT* Interrupt,
	PVOID ServiceContext);

typedef BOOLEAN (*PKMESSAGE_SERVICE_ROUTINE)(
	struct _KINTERRUPT* Interrupt,
	PVOID ServiceContext,
	ULONG MessageID);

typedef struct _IO_CONNECT_INTERRUPT_FULLY_SPECIFIED_PARAMETERS {
	PDEVICE_OBJECT PhysicalDeviceObject;
	PKINTERRUPT* InterruptObject;
	PKSERVICE_ROUTINE ServiceRoutine;
	PVOID ServiceContext;
	PKSPIN_LOCK SpinLock;
	KIRQL SynchronizeIrql;
	BOOLEAN FloatingSave;
	BOOLEAN ShareVector;
	ULONG Vector;
	KIRQL Irql;
	KINTERRUPT_MODE InterruptMode;
	KAFFINITY ProcessorEnableMask;
	USHORT Group;
} IO_CONNECT_INTERRUPT_FULLY_SPECIFIED_PARAMETERS;

typedef struct _IO_CONNECT_INTERRUPT_LINE_BASED_PARAMETERS {
	PDEVICE_OBJECT PhysicalDeviceObject;
	PKINTERRUPT* InterruptObject;
	PKSERVICE_ROUTINE ServiceRoutine;
	PVOID ServiceContext;
	PKSPIN_LOCK SpinLock;
	KIRQL SynchronizeIrql;
	BOOLEAN FloatingSave;
} IO_CONNECT_INTERRUPT_LINE_BASED_PARAMETERS;

typedef struct _IO_CONNECT_INTERRUPT_MESSAGE_BASED_PARAMETERS {
	PDEVICE_OBJECT PhysicalDeviceObject;
	union {
		PVOID* Generic;
		PIO_INTERRUPT_MESSAGE_INFO* InterruptMessageTable;
		PKINTERRUPT* InterruptObject;
	} ConnectionContext;
	PKMESSAGE_SERVICE_ROUTINE MessageServiceRoutine;
	PVOID ServiceContext;
	PKSPIN_LOCK SpinLock;
	KIRQL SynchronizeIrql;
	BOOLEAN FloatingSave;
	PKSERVICE_ROUTINE FallBackServiceRoutine;
} IO_CONNECT_INTERRUPT_MESSAGE_BASED_PARAMETERS;

#define CONNECT_FULLY_SPECIFIED 1
#define CONNECT_LINE_BASED 2
#define CONNECT_MESSAGE_BASED 3
#define CONNECT_FULLY_SPECIFIED_GROUP 4
#define CONNECT_MESSAGE_BASED_PASSIVE 5
#define CONNECT_CURRENT_VERSION 5

typedef struct _IO_CONNECT_INTERRUPT_PARAMETERS {
	ULONG Version;
	union {
		IO_CONNECT_INTERRUPT_FULLY_SPECIFIED_PARAMETERS FullySpecified;
		IO_CONNECT_INTERRUPT_LINE_BASED_PARAMETERS LineBased;
		IO_CONNECT_INTERRUPT_MESSAGE_BASED_PARAMETERS MessageBased;
	};
} IO_CONNECT_INTERRUPT_PARAMETERS, *PIO_CONNECT_INTERRUPT_PARAMETERS;

NTKERNELAPI NTSTATUS IoConnectInterruptEx(PIO_CONNECT_INTERRUPT_PARAMETERS Parameters);

typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemFirmwareTableInformation = 76
} SYSTEM_INFORMATION_CLASS;

NTKERNELAPI NTSTATUS ZwQuerySystemInformation(
	SYSTEM_INFORMATION_CLASS SystemInformationClass,
	PVOID SystemInformation,
	ULONG SystemInformationLength,
	PULONG ReturnLength);

typedef LONG KPRIORITY;

NTKERNELAPI KIRQL KfRaiseIrql(KIRQL NewIrql);
NTKERNELAPI void KeLowerIrql(KIRQL NewIrql);

FORCEINLINE void KeInitializeSpinLock(PKSPIN_LOCK SpinLock) {
	*SpinLock = 0;
}

NTKERNELAPI void KeAcquireSpinLockAtDpcLevel(PKSPIN_LOCK SpinLock);
NTKERNELAPI KIRQL KeAcquireSpinLockRaiseToDpc(PKSPIN_LOCK SpinLock);
NTKERNELAPI void KeReleaseSpinLock(PKSPIN_LOCK SpinLock, KIRQL NewIrql);
NTKERNELAPI void KeReleaseSpinLockFromDpcLevel(PKSPIN_LOCK SpinLock);

typedef enum _EVENT_TYPE {
	NotificationEvent,
	SynchronizationEvent
} EVENT_TYPE;

NTKERNELAPI void KeInitializeEvent(PKEVENT Event, EVENT_TYPE Type, BOOLEAN State);
NTKERNELAPI LONG KeSetEvent(PKEVENT Event, KPRIORITY Increment, BOOLEAN Wait);
NTKERNELAPI void KeClearEvent(PKEVENT Event);
NTKERNELAPI LONG KeResetEvent(PKEVENT Event);

typedef struct _KSEMAPHORE {
	DISPATCHER_HEADER Header;
	LONG Limit;
} KSEMAPHORE, *PKSEMAPHORE;

NTKERNELAPI void KeInitializeSemaphore(PKSEMAPHORE Semaphore, LONG Count, LONG Limit);
NTKERNELAPI LONG KeReleaseSemaphore(
	PKSEMAPHORE Semaphore,
	KPRIORITY Increment,
	LONG Adjustment,
	BOOLEAN Wait);

typedef enum _KWAIT_REASON {
	Executive,
	FreePage,
	PageIn,
	PoolAllocation,
	DelayExecution,
	Suspended,
	UserRequest
} KWAIT_REASON;

typedef struct _KWAIT_BLOCK {
	LIST_ENTRY WaitListEntry;
	UCHAR WaitType;
	volatile UCHAR BlockState;
	USHORT WaitKey;
#ifdef _WIN64
	LONG SpareLong;
#endif
	union {
		struct _KTHREAD* Thread;
		struct _KQUEUE* NotificationQueue;
		struct _KDPC* Dpc;
	};
	PVOID Object;
	PVOID SparePtr;
} KWAIT_BLOCK, *PKWAIT_BLOCK;

NTKERNELAPI NTSTATUS KeWaitForSingleObject(
	PVOID Object,
	KWAIT_REASON WaitReason,
	KPROCESSOR_MODE WaitMode,
	BOOLEAN Alertable,
	PLARGE_INTEGER Timeout);
NTKERNELAPI NTSTATUS KeWaitForMultipleObjects(
	ULONG Count,
	PVOID Object[],
	WAIT_TYPE WaitType,
	KWAIT_REASON WaitReason,
	KPROCESSOR_MODE WaitMode,
	BOOLEAN Alertable,
	PLARGE_INTEGER Timeout,
	PKWAIT_BLOCK WaitBlockArray);

NTKERNELAPI LARGE_INTEGER KeQueryPerformanceCounter(PLARGE_INTEGER PerformanceFrequency);

typedef struct _KTHREAD* PKTHREAD;

#ifdef _M_AMD64

ULONG64 __readgsqword(ULONG Offset);
#pragma intrinsic(__readgsqword)

FORCEINLINE PKTHREAD KeGetCurrentThread(void) {
	return (PKTHREAD) __readgsqword(0x188);
}

#else

NTKERNELAPI PKTHREAD KeGetCurrentThread(void);

#endif

typedef struct _OBJECT_ATTRIBUTES {
	ULONG Length;
	HANDLE RootDirectory;
	PUNICODE_STRING ObjectName;
	ULONG Attributes;
	PVOID SecurityDescriptor;
	PVOID SecurityQualityOfService;
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

typedef struct _CLIENT_ID {
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
} CLIENT_ID, *PCLIENT_ID;

typedef void (*PKSTART_ROUTINE)(PVOID StartContext);

NTKERNELAPI NTSTATUS PsCreateSystemThread(
	PHANDLE ThreadHandle,
	ULONG DesiredAccess,
	POBJECT_ATTRIBUTES ObjectAttributes,
	HANDLE ProcessHandle,
	PCLIENT_ID ClientId,
	PKSTART_ROUTINE StartRoutine,
	PVOID StartContext);

NTKERNELAPI NTSTATUS KeDelayExecutionThread(
	KPROCESSOR_MODE WaitMode,
	BOOLEAN Alertable,
	PLARGE_INTEGER Interval);

#define CONTAINING_RECORD(ptr, type, field) ((type*) ((PCHAR) (ptr) - offsetof(type, field)))

#ifdef __cplusplus
}
#endif

#endif
