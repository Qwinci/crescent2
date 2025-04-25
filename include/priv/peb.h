#ifndef _PRIV_PEB_H
#define _PRIV_PEB_H

#include "ntdef.h"
#include <stddef.h>

#ifdef __x86_64__

typedef struct _PEB_LDR_DATA {
	ULONG Length;
	UCHAR Initialized;
	PVOID SsHandle;
	LIST_ENTRY InOrderModuleList;
	LIST_ENTRY InMemoryOrderModuleList;
	LIST_ENTRY InInitializationOrderModuleList;
	PVOID EntryInProgress;
	UCHAR ShutdownInProgress;
	PVOID ShutdownThreadId;
} PEB_LDR_DATA, *PPEB_LDR_DATA;

typedef struct _CURDIR {
	UNICODE_STRING DosPath;
	PVOID Handle;
} CURDIR;

typedef struct _RTL_DRIVE_LETTER_CURDIR {
	USHORT Flags;
	USHORT Length;
	ULONG TimeStamp;
	STRING DosPath;
} RTL_DRIVE_LETTER_CURDIR;

typedef struct _RTL_USER_PROCESS_PARAMETERS {
	ULONG MaximumLength;
	ULONG Length;
	ULONG Flags;
	ULONG DebugFlags;
	PVOID ConsoleHandle;
	ULONG ConsoleFlags;
	PVOID StandardInput;
	PVOID StandardOutput;
	PVOID StandardError;
	CURDIR CurrentDirectory;
	UNICODE_STRING DllPath;
	UNICODE_STRING ImagePathName;
	UNICODE_STRING CommandLine;
	PVOID Environment;
	ULONG StartingX;
	ULONG StartingY;
	ULONG CountX;
	ULONG CountY;
	ULONG CountCharsX;
	ULONG CountCharsY;
	ULONG FillAttribute;
	ULONG WindowFlags;
	ULONG ShowWindowFlags;
	UNICODE_STRING WindowTitle;
	UNICODE_STRING DesktopInfo;
	UNICODE_STRING ShellInfo;
	UNICODE_STRING RuntimeData;
	RTL_DRIVE_LETTER_CURDIR CurrentDirectores[32];
	ULONGLONG EnvironmentSize;
	ULONGLONG EnvironmentVersion;
	PVOID PackageDependencyData;
	ULONG ProcessGroupId;
	ULONG LoaderThreads;
	UNICODE_STRING RedirectionDllName;
	UNICODE_STRING HeapPartitionName;
	PULONGLONG DefaultThreadpoolCpuSetMasks;
	ULONG DefaultThreadpoolCpuSetMaskCount;
	ULONG DefaultThreadpoolThreadMaximum;
} RTL_USER_PROCESS_PARAMETERS, *PRTL_USER_PROCESS_PARAMETERS;

typedef struct _RTL_CRITICAL_SECTION {
	struct _RTL_CRITICAL_SECTION_DEBUG* DebugInfo;
	LONG LockCount;
	LONG RecursionCount;
	PVOID OwningThread;
	PVOID LockSemaphore;
	ULONGLONG SpinCount;
} RTL_CRITICAL_SECTION, *PRTL_CRITICAL_SECTION;

typedef union [[gnu::aligned(16)]] _SLIST_HEADER  {
	struct {
		ULONGLONG Alignment;
		ULONGLONG Region;
	};
	struct {
		ULONGLONG Depth : 16;
		ULONGLONG Sequence : 48;
		ULONGLONG Reserved : 4;
		ULONGLONG NextEntry : 60; // last 4 bits are always 0's
	} Header;
} SLIST_HEADER, *PSLIST_HEADER;

typedef struct _PEB {
	UCHAR InheritedAddressSpace;
	UCHAR ReadImageFileExecOptions;
	UCHAR BeingDebugged;
	union {
		UCHAR BitField;
		struct {
			UCHAR ImageUsesLargePages : 1;
			UCHAR IsProtectedProcess : 1;
			UCHAR IsImageDynamicallyRelocated : 1;
			UCHAR SkipPatchingUser32Forwarders : 1;
			UCHAR IsPackagedProcess : 1;
			UCHAR IsAppContainer : 1;
			UCHAR IsProtectedProcessLight : 1;
			UCHAR IsLongPathAwareProcess : 1;
		};
	};
	UCHAR pad0[4];
	PVOID Mutant;
	PVOID ImageBaseAddress;
	PPEB_LDR_DATA Ldr;
	PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
	PVOID SubSystemData;
	PVOID ProcessHeap;
	PRTL_CRITICAL_SECTION FastPebLock;
	volatile PSLIST_HEADER AtlThunkSListPtr;
	PVOID IFEOKey;
	union {
		ULONG CrossProcessFlags;
		struct {
			ULONG ProcessInJob : 1;
			ULONG ProcessInitializing : 1;
			ULONG ProcessUsingVEH : 1;
			ULONG ProcessUsingVCH : 1;
			ULONG ProcessUsingFTH : 1;
			ULONG ProcessPreviouslyThrottled : 1;
			ULONG ProcessCurrentlyThrottled : 1;
			ULONG ProcessImagesHotPatched : 1;
			ULONG ReservedBits0 : 24;
		};
	};
	UCHAR pad1[4];
	union {
		PVOID KernelCallbackTable;
		PVOID UserSharedInfoPtr;
	};
	ULONG SystemReserved;
	ULONG AtlThunkSListPtr32;
	PVOID ApiSetMap;
	ULONG TlsExpansionCounter;
	UCHAR pad2[4];
	PVOID TlsBitmap;
	ULONG TlsBitmapBits[2];
	PVOID ReadOnlySharedMemoryBase;
	PVOID SharedData;
	PVOID* ReadOnlyStaticServerData;
	PVOID AnsiCodePageData;
	PVOID OemCodePageData;
	PVOID UnicodeCaseTableData;
	ULONG NumberOfProcessors;
	ULONG NtGlobalFlag;
} PEB, *PPEB;

typedef struct _NT_TIB {
	struct _EXCEPTION_REGISTRATION_RECORD* ExceptionList;
	PVOID StackBase;
	PVOID StackLimit;
	PVOID SubSystemTib;
	union {
		PVOID FiberData;
		ULONG Version;
	};
	PVOID ArbitraryUserPointer;
	struct _NT_TIB* Self;
} NT_TIB, *PNT_TIB;

typedef struct _CLIENT_ID {
	PVOID UniqueProcess;
	PVOID UniqueThread;
} CLIENT_ID;

typedef struct _ACTIVATION_CONTEXT_STACK {
	struct _RTL_ACTIVATION_CONTEXT_STACK_FRAME* ActiveFrame;
	LIST_ENTRY FrameListCache;
	ULONG Flags;
	ULONG NextCookieSequenceNumber;
	ULONG StackId;
} ACTIVATION_CONTEXT_STACK, *PACTIVATION_CONTEXT_STACK;

typedef struct _GDI_TEB_BATCH {
	ULONG Offset : 31;
	ULONG HasRenderingCommand : 1;
	ULONGLONG HDC;
	ULONG Buffer[310];
} GDI_TEB_BATCH;

typedef struct _TEB {
	NT_TIB NtTib;
	PVOID EnvironmentPointer;
	CLIENT_ID ClientId;
	PVOID ActiveRpcHandle;
	PVOID ThreadLocalStoragePointer;
	PPEB ProcessEnvironmentBlock;
	ULONG LastErrorValue;
	ULONG CountOfOwnedCriticalSections;
	PVOID CsrClientThread;
	PVOID Win32ThreadInfo;
	ULONG User32Reserved[26];
	ULONG UserReserved[5];
	PVOID WOW32Reserved;
	ULONG CurrentLocale;
	ULONG FpSoftwareStatusRegister;
	PVOID ReservedForDebuggerInstrumentation[16];
	PVOID SystemReserved1[30];
	CHAR PlaceholderCompatibilityMode;
	UCHAR PlaceholderHydrationAlwaysExplicit;
	CHAR PlaceholderReserved[10];
	ULONG ProxiedProcessId;
	ACTIVATION_CONTEXT_STACK ActivationStack;
	UCHAR WorkingOnBehalfTicket[8];
	LONG ExceptionCode;
	UCHAR Padding0[4];
	PACTIVATION_CONTEXT_STACK ActivationContextStackPointer;
	ULONGLONG InstrumentationCallbackSp;
	ULONGLONG InstrumentationCallbackPreviousPc;
	ULONGLONG InstrumentationCallbackPreviousSp;
	ULONG TxFsContext;
	UCHAR InstrumentationCallbackDisabled;
	UCHAR UnalignedLoadStoreExceptions;
	UCHAR Padding1[2];
	GDI_TEB_BATCH GdiTebBatch;
	CLIENT_ID RealClientId;
	PVOID GdiCachedProcessHandle;
	ULONG GdiClientPID;
	ULONG GdiClientTID;
	PVOID GdiThreadLocalInfo;
	ULONGLONG Win32ClientInfo[62];
	PVOID glDispatchTable[233];
	ULONGLONG glReserved1[29];
	PVOID glReserved2;
	PVOID glSectionInfo;
	PVOID glSection;
	PVOID glTable;
	PVOID glCurrentRC;
	PVOID glContext;
	ULONG LastStatusValue;
	UCHAR Padding2[4];
	UNICODE_STRING StaticUnicodeString;
	WCHAR StaticUnicodeBuffer[261];
	UCHAR Padding3[6];
	PVOID DeallocationStack;
	PVOID TlsSlots[64];
	LIST_ENTRY TlsLinks;
} TEB, *PTEB;

#define NtCurrentPeb() (*(PEB* __seg_gs*) offsetof(TEB, ProcessEnvironmentBlock))
#define NtCurrentTeb() (*(TEB* __seg_gs*) offsetof(NT_TIB, Self))

#else
#error unsupported architecture
#endif

#endif
