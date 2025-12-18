#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	// If the created process should inherit the current process'
	// handles (except for uninheritable ones)
	OS_PROCESS_INHERIT_HANDLES = 1 << 0,
	
	// If this process' main thread should be created suspended.
	OS_PROCESS_CREATE_SUSPENDED = 1 << 1,
	
	// If the command line passed to OSCreateProcess is already
	// in environment description format (entries separated by
	// null characters and the list terminating with 2 null chars)
	OS_PROCESS_CMDLINE_PARSED = 1 << 2,
	
	// If the created process should deeply clone all the handles
	// instead of simply inheriting them. This also copies object
	// references opened with OB_FLAG_NO_INHERIT. This is used by
	// the POSIX fork support implementation.
	OS_PROCESS_DEEP_CLONE_HANDLES = 1 << 3,
};

BSTATUS OSCreateProcess(
	PHANDLE OutHandle,
	PHANDLE OutMainThreadHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	int ProcessFlags,
	const char* ImageName,
	const char* CommandLine,
	const char* Environment
);

#ifdef __cplusplus
}
#endif
