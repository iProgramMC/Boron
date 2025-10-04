#pragma once

#ifdef __cplusplus
extern "C" {
#endif

BSTATUS OSCreateProcess(
	PHANDLE OutHandle,
	PHANDLE OutMainThreadHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	bool InheritHandles,
	bool CreateSuspended,
	const char* ImageName,
	const char* CommandLine
);

#ifdef __cplusplus
}
#endif
