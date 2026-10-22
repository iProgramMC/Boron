#include <boron.h>

BSTATUS OSCreateFile(
	PHANDLE OutFileHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	PIO_STATUS_BLOCK IoStatusBlock,
	uint32_t FileAttributes,
	uint64_t AllocationSize,
	uint32_t CreateDisposition,
	uint32_t CreateOptions,
	const char* TargetName,
	size_t TargetNameLength
)
{
	if (!FileAttributes && !AllocationSize && !TargetName && !TargetNameLength)
	{
		return OSCreateFileCall(
			OutFileHandle,
			ObjectAttributes,
			IoStatusBlock,
			CreateDisposition,
			CreateOptions,
			NULL   // ExtraParams
		);
	}

	CREATE_FILE_PARAMS ExtraParams;
	ExtraParams.FileAttributes = FileAttributes;
	ExtraParams.AllocationSize = AllocationSize;
	ExtraParams.TargetName = TargetName;
	ExtraParams.TargetNameLength = TargetNameLength;

	return OSCreateFileCall(
		OutFileHandle,
		ObjectAttributes,
		IoStatusBlock,
		CreateDisposition,
		CreateOptions,
		&ExtraParams
	);
}

BSTATUS OSOpenFile(
	PHANDLE OutFileHandle,
	POBJECT_ATTRIBUTES ObjectAttributes
)
{
	return OSCreateFileCall(
		OutFileHandle,
		ObjectAttributes,
		NULL,    // IoStatusBlock
		IO_OPEN, // CreateDisposition
		IO_FILE_ANY_TYPE,
		NULL
	);
}

BSTATUS OSOpenFileEx(
	PHANDLE OutFileHandle,
	POBJECT_ATTRIBUTES ObjectAttributes,
	uint32_t FileType
)
{
	return OSCreateFileCall(
		OutFileHandle,
		ObjectAttributes,
		NULL,     // IoStatusBlock
		IO_OPEN,  // CreateDisposition
		FileType, // CreateOptions
		NULL
	);
}
