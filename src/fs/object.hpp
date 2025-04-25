#pragma once

#include "types.hpp"
#include "assert.hpp"
#include "ntdef.h"
#include "sched/misc.hpp"
#include "object_init.hpp"

struct SECURITY_DESCRIPTOR;

struct OBJECT_ATTRIBUTES {
	ULONG length;
	HANDLE root_directory;
	PUNICODE_STRING object_name;
	ULONG attributes;
	SECURITY_DESCRIPTOR* security_descriptor;
	PVOID security_quality_of_service;
};

inline void InitializeObjectAttributes(
	OBJECT_ATTRIBUTES* attribs,
	PUNICODE_STRING object_name,
	ULONG attributes,
	HANDLE root_directory,
	SECURITY_DESCRIPTOR* security_descriptor) {
	attribs->length = sizeof(OBJECT_ATTRIBUTES);
	attribs->root_directory = root_directory;
	attribs->object_name = object_name;
	attribs->attributes = attributes;
	attribs->security_descriptor = security_descriptor;
	attribs->security_quality_of_service = nullptr;
}

#define OBJ_INHERIT 2
#define OBJ_PERMANENT 0x10
#define OBJ_EXCLUSIVE 0x20
#define OBJ_CASE_INSENSITIVE 0x40
#define OBJ_OPENIF 0x80
#define OBJ_OPENLINK 0x100
#define OBJ_KERNEL_HANDLE 0x200
#define OBJ_FORCE_ACCESS_CHECK 0x400
#define OBJ_KERNEL_EXCLUSIVE 0x800
#define OBJ_DONT_REPARSE 0x1000
#define OBJ_VALID_ATTRIBUTES 0x1FF2

struct OBJECT_HANDLE_INFORMATION {
	ULONG handle_attribs;
	ACCESS_MASK granted_access;
};

NTAPI extern "C" NTSTATUS ObCreateObjectType(
	PUNICODE_STRING type_name,
	OBJECT_TYPE_INITIALIZER* object_type_initializer,
	SECURITY_DESCRIPTOR* security_descriptor,
	POBJECT_TYPE* object_type);

OBJECT_TYPE_INITIALIZER* object_get_type(PVOID object);

NTAPI extern "C" NTSTATUS ObCreateObject(
	KPROCESSOR_MODE probe_mode,
	POBJECT_TYPE object_type,
	OBJECT_ATTRIBUTES* object_attribs,
	KPROCESSOR_MODE ownership_mode,
	PVOID parse_context,
	ULONG object_body_size,
	ULONG paged_pool_charge,
	ULONG non_paged_pool_charge,
	PVOID* object);
NTAPI extern "C" NTSTATUS ObInsertObject(
	PVOID object,
	PACCESS_STATE passed_access_state,
	ACCESS_MASK desired_access,
	ULONG object_pointer_bias,
	PVOID* new_object,
	PHANDLE handle);

NTAPI extern "C" NTSTATUS NtCreateDirectoryObject(
	PHANDLE directory_handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* object_attribs);
NTAPI extern "C" NTSTATUS ZwCreateDirectoryObject(
	PHANDLE directory_handle,
	ACCESS_MASK desired_access,
	OBJECT_ATTRIBUTES* object_attribs);

NTAPI extern "C" void ObfReferenceObject(PVOID object);
NTAPI extern "C" void ObfDereferenceObject(PVOID object);

NTAPI extern "C" NTSTATUS ObReferenceObjectByPointer(
	PVOID object,
	ACCESS_MASK desired_access,
	POBJECT_TYPE object_type,
	KPROCESSOR_MODE access_mode);

NTAPI extern "C" NTSTATUS ObReferenceObjectByHandle(
	HANDLE handle,
	ACCESS_MASK desired_access,
	POBJECT_TYPE object_type,
	KPROCESSOR_MODE access_mode,
	PVOID* object,
	OBJECT_HANDLE_INFORMATION* handle_info);

NTAPI extern "C" NTSTATUS ObReferenceObjectByName(
	PUNICODE_STRING object_name,
	ULONG attribs,
	PACCESS_STATE access_state,
	ACCESS_MASK desired_access,
	POBJECT_TYPE object_type,
	KPROCESSOR_MODE access_mode,
	PVOID parse_ctx,
	PVOID* object);

NTAPI extern "C" NTSTATUS ObOpenObjectByName(
	OBJECT_ATTRIBUTES* object_attribs,
	POBJECT_TYPE object_type,
	KPROCESSOR_MODE access_mode,
	PACCESS_STATE access_state,
	ACCESS_MASK desired_access,
	PVOID parse_ctx,
	PHANDLE handle);
