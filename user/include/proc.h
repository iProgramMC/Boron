#pragma once

BSTATUS OSCreateProcess(
	PHANDLE OutHandle,
	PHANDLE OutMainThreadHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	bool InheritHandles,
	bool CreateSuspended,
	const char* ImageName,
	const char* CommandLine
);
