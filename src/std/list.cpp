#include "list.hpp"
#include "atomic.hpp"

NTAPI extern "C" PLIST_ENTRY ExInterlockedInsertHeadList(
	PLIST_ENTRY list_head,
	PLIST_ENTRY list_entry,
	KSPIN_LOCK* lock) {
	auto old = KfRaiseIrql(HIGH_LEVEL);
	KeAcquireSpinLockAtDpcLevel(lock);

	auto first = list_head->Flink;
	auto last = list_head->Blink;
	InsertHeadList(list_head, list_entry);

	KeReleaseSpinLockFromDpcLevel(lock);
	KeLowerIrql(old);
	return first != last ? first : nullptr;
}

NTAPI PLIST_ENTRY ExInterlockedInsertTailList(
	PLIST_ENTRY list_head,
	PLIST_ENTRY list_entry,
	KSPIN_LOCK* lock) {
	auto old = KfRaiseIrql(HIGH_LEVEL);
	KeAcquireSpinLockAtDpcLevel(lock);

	auto first = list_head->Flink;
	auto last = list_head->Blink;
	InsertTailList(list_head, list_entry);

	KeReleaseSpinLockFromDpcLevel(lock);
	KeLowerIrql(old);
	return first != last ? last : nullptr;
}

NTAPI extern "C" PLIST_ENTRY ExInterlockedRemoveHeadList(
	PLIST_ENTRY list_head,
	KSPIN_LOCK* lock) {
	auto old = KfRaiseIrql(HIGH_LEVEL);
	KeAcquireSpinLockAtDpcLevel(lock);

	auto first = list_head->Flink;
	auto last = list_head->Blink;
	RemoveHeadList(list_head);

	KeReleaseSpinLockFromDpcLevel(lock);
	KeLowerIrql(old);
	return first != last ? first : nullptr;
}

#ifdef __x86_64__

asm(R"(
.intel_syntax noprefix
.pushsection .text
.globl ExpInterlockedPushEntrySList
.def ExpInterlockedPushEntrySList
.type 32
.endef
// SLIST_ENTRY* ExpInterlockedPushEntrySList(SLIST_HEADER* list_head, SLIST_ENTRY* list_entry)
ExpInterlockedPushEntrySList:
	// save rbx
	mov r8, rbx

	// move list_header because rcx is used by cmpxchg16b
	mov r9, rcx

	// move list_entry because rdx is used by cmpxchg16b
	mov r10, rdx

	// desired high, new next (list_entry)
	mov rcx, rdx

	// expected
	mov rax, [r9]
	mov rdx, [r9 + 8]

0:
	// set list_entry->next to old next
	mov [r10], rdx

	// desired low, old depth/sequence both increased by one
	lea rbx, [rax + 0x10001]

	lock cmpxchg16b [r9]
	jnz 0b

	// old first entry
	mov rax, rdx

	// restore rbx
	mov rbx, r8
	ret

.globl ExpInterlockedPopEntrySList
.def ExpInterlockedPopEntrySList
.type 32
.endef
// SLIST_ENTRY* ExpInterlockedPopEntrySList(SLIST_HEADER* list_head)
ExpInterlockedPopEntrySList:
	// save rbx
	mov r8, rbx

	// move list_header because rcx is used by cmpxchg16b
	mov r9, rcx

	// expected
	mov rax, [r9]
	mov rdx, [r9 + 8]

0:
	// desired
	// start with same depth/sequence
	mov rbx, rax
	// next loaded from the first entry
	mov rcx, [rdx]

	// decrement depth
	dec bx

	lock cmpxchg16b [r9]
	jnz 0b

	// old first entry
	mov rax, rdx

	// restore rbx
	mov rbx, r8
	ret

.globl ExpInterlockedFlushSList
.def ExpInterlockedFlushSList
.type 32
.endef
// SLIST_ENTRY* ExpInterlockedFlushSList(SLIST_HEADER* list_head)
ExpInterlockedFlushSList:
	mov r8, rbx

	mov r9, rcx

	// expected
	mov rax, [rcx]
	mov rdx, [rcx + 8]

0:
	// start with same depth/sequence
	mov rbx, rax
	// clear depth
	xor bx, bx
	// clear next
	xor ecx, ecx
	lock cmpxchg16b [r9]
	jnz 0b

	mov rax, rdx

	mov rbx, r8
	ret

.popsection
)");

#else

#error missing architecture specific code

#endif

NTAPI USHORT ExQueryDepthSList(SLIST_HEADER* list_head) {
	return atomic_load(reinterpret_cast<u16*>(list_head), __ATOMIC_RELAXED);
}
