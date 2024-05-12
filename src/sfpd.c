/*++

Copyright (c) 2021-2024 DuoWoA authors. All Rights Reserved.

Module Name:

	sfpd.c

Abstract:

	This file contains the SFPD functions.

Environment:

	Kernel-mode Driver Framework

--*/

#include "sfpd.h"

// ntifs header is incompatible with wdm header...

#if (NTDDI_VERSION >= NTDDI_WIN2K)
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSYSAPI
NTSTATUS
NTAPI
ZwQueryDirectoryFile(
	_In_ HANDLE FileHandle,
	_In_opt_ HANDLE Event,
	_In_opt_ PIO_APC_ROUTINE ApcRoutine,
	_In_opt_ PVOID ApcContext,
	_Out_ PIO_STATUS_BLOCK IoStatusBlock,
	_Out_writes_bytes_(Length) PVOID FileInformation,
	_In_ ULONG Length,
	_In_ FILE_INFORMATION_CLASS FileInformationClass,
	_In_ BOOLEAN ReturnSingleEntry,
	_In_opt_ PUNICODE_STRING FileName,
	_In_ BOOLEAN RestartScan
);
#endif

typedef struct _FILE_BOTH_DIR_INFORMATION {
	ULONG NextEntryOffset;
	ULONG FileIndex;
	LARGE_INTEGER CreationTime;
	LARGE_INTEGER LastAccessTime;
	LARGE_INTEGER LastWriteTime;
	LARGE_INTEGER ChangeTime;
	LARGE_INTEGER EndOfFile;
	LARGE_INTEGER AllocationSize;
	ULONG FileAttributes;
	ULONG FileNameLength;
	ULONG EaSize;
	CCHAR ShortNameLength;
	WCHAR ShortName[12];
	_Field_size_bytes_(FileNameLength) WCHAR FileName[1];
} FILE_BOTH_DIR_INFORMATION, * PFILE_BOTH_DIR_INFORMATION;

// end of workaround for ntifs

NTSTATUS GetSFPDPixelAlignmentData(WDFDEVICE device, PSFPD_DISPLAY_PIXEL_ALIGNMENT_DATA PixelAlignmentData)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	if (PixelAlignmentData == NULL)
	{
		status = STATUS_INVALID_PARAMETER;
		goto exit;
	}

	DWORD PixelAlignmentDataSize = sizeof(SFPD_DISPLAY_PIXEL_ALIGNMENT_DATA);
	DWORD ActualSFPDFileSize = 0;

	status = GetSFPDItemSize(device, PIXEL_ALIGNMENT_DATA_FILE_PATH, &ActualSFPDFileSize);

	if (!NT_SUCCESS(status))
	{
		goto exit;
	}

	if (PixelAlignmentDataSize != ActualSFPDFileSize)
	{
		status = STATUS_FILE_CORRUPT_ERROR;
		goto exit;
	}

	status = GetSFPDItem(device, PIXEL_ALIGNMENT_DATA_FILE_PATH, PixelAlignmentData, PixelAlignmentDataSize);

	if (!NT_SUCCESS(status))
	{
		goto exit;
	}

exit:
	return status;
}

NTSTATUS GetSFPDItem(WDFDEVICE device, WCHAR* ItemPath, PVOID Data, DWORD DataSize)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;
	WCHAR* FilePath = NULL;
	HANDLE FileHandle = NULL;

	if (Data == NULL || DataSize == 0 || ItemPath == NULL)
	{
		status = STATUS_INVALID_PARAMETER;
		goto exit;
	}

	FilePath = (WCHAR*)ExAllocatePoolWithTag(NonPagedPool, MAX_PATH * sizeof(WCHAR), POOL_TAG_FILEPATH);

	if (FilePath == NULL)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto exit;
	}

	status = GetSFPDVolumePath(device, FilePath, MAX_PATH);

	if (!NT_SUCCESS(status))
	{
		status = STATUS_NOT_FOUND;
		goto exit;
	}

	status = RtlStringCbCatW(FilePath, MAX_PATH * sizeof(WCHAR), ItemPath);

	if (!NT_SUCCESS(status))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto exit;
	}

	UNICODE_STRING FilePathUnicode;
	RtlInitUnicodeString(&FilePathUnicode, FilePath);

	OBJECT_ATTRIBUTES Attributes = { 0 };
	InitializeObjectAttributes(&Attributes, &FilePathUnicode, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

	IO_STATUS_BLOCK IOStatusBlock = { 0 };

	status = ZwCreateFile(&FileHandle, GENERIC_READ, &Attributes, &IOStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

	if (!NT_SUCCESS(status))
	{
		status = STATUS_FILE_INVALID;
		goto exit;
	}

	status = ZwReadFile(FileHandle, NULL, NULL, NULL, &IOStatusBlock, Data, DataSize, NULL, NULL);

	if (!NT_SUCCESS(status))
	{
		status = STATUS_FILE_INVALID;
		goto exit;
	}

exit:

	if (NULL != FilePath)
	{
		ExFreePool(FilePath);
	}

	if (NULL != FileHandle)
	{
		ZwClose(FileHandle);
	}

	return status;
}

NTSTATUS GetSFPDNumberOfFilesInDirectory(WDFDEVICE device, WCHAR* DirectoryPath, DWORD* NumberOfFiles)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	BOOLEAN bIsStarted = TRUE;
	UINT uSize = sizeof(FILE_BOTH_DIR_INFORMATION);
	FILE_BOTH_DIR_INFORMATION* pfbInfo = NULL;

	HANDLE FileHandle = NULL;
	WCHAR* FilePath = (WCHAR*)ExAllocatePoolWithTag(NonPagedPool, MAX_PATH * sizeof(WCHAR), POOL_TAG_FILEPATH);

	if (NULL == FilePath)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto exit;
	}

	if (NULL == NumberOfFiles)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto exit;
	}

	*NumberOfFiles = 0;

	status = GetSFPDVolumePath(device, FilePath, MAX_PATH);

	if (!NT_SUCCESS(status))
	{
		status = STATUS_NOT_FOUND;
		goto exit;
	}

	status = RtlStringCbCatW(FilePath, MAX_PATH * sizeof(WCHAR), DirectoryPath);

	if (!NT_SUCCESS(status))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto exit;
	}

	UNICODE_STRING FilePathUnicode;
	RtlInitUnicodeString(&FilePathUnicode, FilePath);

	OBJECT_ATTRIBUTES Attributes = { 0 };
	InitializeObjectAttributes(&Attributes, &FilePathUnicode, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

	IO_STATUS_BLOCK IOStatusBlock = { 0 };

	status = ZwCreateFile(&FileHandle, GENERIC_READ | SYNCHRONIZE, &Attributes, &IOStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		status = STATUS_FILE_NOT_AVAILABLE;
		goto exit;
	}

	//
	// https://community.osr.com/t/zwquerydirectoryfile-api-question-enumerating-files-in-a-directory-or-in-a-volumes/31914/3
	//

	pfbInfo = ExAllocatePoolWithTag(PagedPool, uSize, '0000');

	if (pfbInfo == NULL)
	{
		status = STATUS_NO_MEMORY;
		goto exit;
	}

	while (TRUE)
	{
	lbl_retry:
		RtlZeroMemory(pfbInfo, uSize);
		status = ZwQueryDirectoryFile(FileHandle, 0, NULL, NULL, &IOStatusBlock, pfbInfo,
			uSize, FileBothDirectoryInformation, FALSE, NULL, bIsStarted);

		if (STATUS_BUFFER_OVERFLOW == status)
		{
			ExFreePool(pfbInfo);

			uSize = uSize * 2;
			pfbInfo = ExAllocatePoolWithTag(PagedPool, uSize, '0000');

			if (pfbInfo == NULL)
			{
				status = STATUS_NO_MEMORY;
				goto exit;
			}

			goto lbl_retry;
		}
		else if (STATUS_NO_MORE_FILES == status)
		{
			status = STATUS_SUCCESS;
			goto exit;
		}
		else if (STATUS_SUCCESS != status)
		{
			goto exit;
		}

		if (bIsStarted)
		{
			bIsStarted = FALSE;
		}

		while (TRUE)
		{
			// We do not want to touch directories
			if ((pfbInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY)
			{
				*NumberOfFiles = *NumberOfFiles + 1;
			}

			if (pfbInfo->NextEntryOffset == 0)
			{
				break;
			}

			pfbInfo += pfbInfo->NextEntryOffset;
		}
	}

exit:
	if (pfbInfo != NULL)
	{
		ExFreePool(pfbInfo);
	}

	if (FilePath != NULL)
	{
		ExFreePool(FilePath);
	}

	if (FileHandle != NULL)
	{
		ZwClose(FileHandle);
	}

	return status;
}

// This function is very specifically crafted for SOCPartition, I know.
NTSTATUS GetSFPDFilesInDirectory(WDFDEVICE device, WCHAR* DirectoryPath, DWORD NumberOfFiles, PUCHAR Buffer)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	BOOLEAN bIsStarted = TRUE;
	UINT uSize = sizeof(FILE_BOTH_DIR_INFORMATION);
	FILE_BOTH_DIR_INFORMATION* pfbInfo = NULL;

	HANDLE FileHandle = NULL;
	WCHAR* FilePath = (WCHAR*)ExAllocatePoolWithTag(NonPagedPool, MAX_PATH * sizeof(WCHAR), POOL_TAG_FILEPATH);

	if (NULL == FilePath)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto exit;
	}

	status = GetSFPDVolumePath(device, FilePath, MAX_PATH);

	if (!NT_SUCCESS(status))
	{
		status = STATUS_NOT_FOUND;
		goto exit;
	}

	status = RtlStringCbCatW(FilePath, MAX_PATH * sizeof(WCHAR), DirectoryPath);

	if (!NT_SUCCESS(status))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto exit;
	}

	UNICODE_STRING FilePathUnicode;
	RtlInitUnicodeString(&FilePathUnicode, FilePath);

	OBJECT_ATTRIBUTES Attributes = { 0 };
	InitializeObjectAttributes(&Attributes, &FilePathUnicode, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

	IO_STATUS_BLOCK IOStatusBlock = { 0 };

	status = ZwCreateFile(&FileHandle, GENERIC_READ | SYNCHRONIZE, &Attributes, &IOStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		status = STATUS_FILE_NOT_AVAILABLE;
		goto exit;
	}

	//
	// https://community.osr.com/t/zwquerydirectoryfile-api-question-enumerating-files-in-a-directory-or-in-a-volumes/31914/3
	//

	pfbInfo = ExAllocatePoolWithTag(PagedPool, uSize, '0000');

	if (pfbInfo == NULL)
	{
		status = STATUS_NO_MEMORY;
		goto exit;
	}

	DWORD CurrentNumberOfFiles = 0;
	DWORD CurrentFileAllocation = 0;

	while (TRUE)
	{
	lbl_retry:
		RtlZeroMemory(pfbInfo, uSize);
		status = ZwQueryDirectoryFile(FileHandle, 0, NULL, NULL, &IOStatusBlock, pfbInfo,
			uSize, FileBothDirectoryInformation, FALSE, NULL, bIsStarted);

		if (STATUS_BUFFER_OVERFLOW == status)
		{
			ExFreePool(pfbInfo);

			uSize = uSize * 2;
			pfbInfo = ExAllocatePoolWithTag(PagedPool, uSize, '0000');

			if (pfbInfo == NULL)
			{
				status = STATUS_NO_MEMORY;
				goto exit;
			}

			goto lbl_retry;
		}
		else if (STATUS_NO_MORE_FILES == status)
		{
			status = STATUS_SUCCESS;
			goto exit;
		}
		else if (STATUS_SUCCESS != status)
		{
			goto exit;
		}

		if (bIsStarted)
		{
			bIsStarted = FALSE;
		}

		while (TRUE)
		{
			// We do not want to touch directories
			if ((pfbInfo->FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY)
			{
				CurrentNumberOfFiles++;
				if (CurrentNumberOfFiles > NumberOfFiles)
				{
					status = STATUS_NO_MEMORY;
					goto exit;
				}

				PUCHAR FileBlockBuffer = Buffer + (CurrentNumberOfFiles - 1) * 244;

				RtlCopyMemory(FileBlockBuffer, pfbInfo->FileName, min(pfbInfo->FileNameLength, 49 * sizeof(WCHAR)));
				RtlCopyMemory(FileBlockBuffer + (49 * sizeof(WCHAR)), L"JSON", sizeof(L"JSON"));

				DWORD FileSize = pfbInfo->EndOfFile.LowPart;
				DWORD FileAllocation = FileSize;

				if ((FileSize % 256) != 0)
				{
					FileAllocation = FileSize + (256 - (FileSize % 256));
				}

				*(DWORD*)(FileBlockBuffer + (49 * sizeof(WCHAR)) + (49 * sizeof(WCHAR))) = FileAllocation; // File Allocation
				*(DWORD*)(FileBlockBuffer + (49 * sizeof(WCHAR)) + (49 * sizeof(WCHAR)) + 4) = FileSize; // FileSize
				*(DWORD*)(FileBlockBuffer + (49 * sizeof(WCHAR)) + (49 * sizeof(WCHAR)) + 4 + 4) = 1; // ?, always one
				*(DWORD*)(FileBlockBuffer + (49 * sizeof(WCHAR)) + (49 * sizeof(WCHAR)) + 4 + 4 + 4) = CurrentFileAllocation; // Offset

				CurrentFileAllocation += FileAllocation;
			}

			if (pfbInfo->NextEntryOffset == 0)
			{
				break;
			}

			pfbInfo += pfbInfo->NextEntryOffset;
		}
	}

exit:
	if (pfbInfo != NULL)
	{
		ExFreePool(pfbInfo);
	}

	if (FilePath != NULL)
	{
		ExFreePool(FilePath);
	}

	if (FileHandle != NULL)
	{
		ZwClose(FileHandle);
	}

	return status;
}

NTSTATUS GetSFPDItemSize(WDFDEVICE device, WCHAR* ItemPath, DWORD* ItemSize)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	HANDLE FileHandle = NULL;
	WCHAR* FilePath = (WCHAR*)ExAllocatePoolWithTag(NonPagedPool, MAX_PATH * sizeof(WCHAR), POOL_TAG_FILEPATH);

	if (NULL == FilePath)
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto exit;
	}

	status = GetSFPDVolumePath(device, FilePath, MAX_PATH);

	if (!NT_SUCCESS(status))
	{
		status = STATUS_NOT_FOUND;
		goto exit;
	}

	status = RtlStringCbCatW(FilePath, MAX_PATH * sizeof(WCHAR), ItemPath);

	if (!NT_SUCCESS(status))
	{
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto exit;
	}

	UNICODE_STRING FilePathUnicode;
	RtlInitUnicodeString(&FilePathUnicode, FilePath);

	OBJECT_ATTRIBUTES Attributes = { 0 };
	InitializeObjectAttributes(&Attributes, &FilePathUnicode, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

	IO_STATUS_BLOCK IOStatusBlock = { 0 };

	status = ZwCreateFile(&FileHandle, GENERIC_READ, &Attributes, &IOStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);
	if (!NT_SUCCESS(status))
	{
		status = STATUS_FILE_NOT_AVAILABLE;
		goto exit;
	}

	FILE_STANDARD_INFORMATION FileStandardInfo = { 0 };

	status = ZwQueryInformationFile(FileHandle, &IOStatusBlock, &FileStandardInfo, sizeof(FILE_STANDARD_INFORMATION), FileStandardInformation);

	if (!NT_SUCCESS(status))
	{
		status = STATUS_FILE_INVALID;
		goto exit;
	}

	*ItemSize = FileStandardInfo.EndOfFile.LowPart;

exit:
	if (FilePath != NULL)
	{
		ExFreePool(FilePath);
	}

	if (FileHandle != NULL)
	{
		ZwClose(FileHandle);
	}

	return status;
}

NTSTATUS GetSFPDVolumePath(WDFDEVICE device, WCHAR* VolumePath, DWORD VolumePathLength)
{
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	WDFIOTARGET                   IOTarget = NULL;
	WDFREQUEST                    Request = NULL;
	WDFMEMORY                     IOCTLRequestMemoryBuffer = NULL;

	DRIVE_LAYOUT_INFORMATION_EX* DriverLayoutInformationExtended = NULL;
	DWORD HardDiskNumber = 0;
	DWORD PartitionNumber = 0;
	BOOL FoundPartition = FALSE;
	WCHAR* DevicePath = NULL;

	do
	{
		DevicePath = (WCHAR*)ExAllocatePoolWithTag(NonPagedPool, MAX_PATH * sizeof(WCHAR), POOL_TAG_FILEPATH);

		if (DevicePath == NULL)
		{
			goto exit;
		}

		status = RtlStringCbPrintfW(DevicePath, MAX_PATH * sizeof(WCHAR), L"\\Device\\Harddisk%u\\Partition0", HardDiskNumber);

		if (!NT_SUCCESS(status))
		{
			goto exit;
		}

		UNICODE_STRING DevicePathUnicode = { 0 };
		RtlInitUnicodeString(&DevicePathUnicode, DevicePath);

		WDF_OBJECT_ATTRIBUTES Attributes;
		WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);
		Attributes.ParentObject = device;

		status = WdfIoTargetCreate(device, &Attributes, &IOTarget);

		if (!NT_SUCCESS(status))
		{
			goto exit;
		}

		WDF_IO_TARGET_OPEN_PARAMS IOTargetOpenParams;
		WDF_IO_TARGET_OPEN_PARAMS_INIT_OPEN_BY_NAME(
			&IOTargetOpenParams,
			&DevicePathUnicode,
			GENERIC_READ | GENERIC_WRITE
		);

		IOTargetOpenParams.ShareAccess = FILE_SHARE_READ | FILE_SHARE_WRITE;

		for (DWORD OpenAttemptCounter = 0; OpenAttemptCounter < MAX_FILE_OPEN_ATTEMPTS; OpenAttemptCounter++)
		{
			status = WdfIoTargetOpen(IOTarget, &IOTargetOpenParams);

			if (status == STATUS_SHARING_VIOLATION)
			{
				LARGE_INTEGER timeout = { 0 };
				timeout.QuadPart = RELATIVE(MILLISECONDS(500));
				KeDelayExecutionThread(KernelMode, FALSE, &timeout);
			}
			else
			{
				break;
			}
		}

		if (!NT_SUCCESS(status))
		{
			WdfObjectDelete(IOTarget);
			IOTarget = NULL;

			goto exit;
		}

		DWORD PartitionCount = 4;

		do
		{
			WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);
			Attributes.ParentObject = IOTarget;

			status = WdfRequestCreate(&Attributes, IOTarget, &Request);
			if (!NT_SUCCESS(status))
			{
				goto exit;
			}

			DWORD IOCTLRequestMemoryBufferSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) + (PartitionCount * sizeof(PARTITION_INFORMATION_EX));

			WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);
			Attributes.ParentObject = Request;

			status = WdfMemoryCreate(
				&Attributes,
				NonPagedPool,
				POOL_TAG_DRIVEINFO,
				IOCTLRequestMemoryBufferSize,
				&IOCTLRequestMemoryBuffer,
				&DriverLayoutInformationExtended
			);

			if (!NT_SUCCESS(status))
			{
				goto exit;
			}

			status = WdfIoTargetFormatRequestForIoctl(
				IOTarget,
				Request,
				IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
				NULL,
				NULL,
				IOCTLRequestMemoryBuffer,
				NULL
			);

			if (!NT_SUCCESS(status))
			{
				goto exit;
			}

			WDF_REQUEST_SEND_OPTIONS RequestSendOptions;
			WDF_REQUEST_SEND_OPTIONS_INIT(
				&RequestSendOptions,
				WDF_REQUEST_SEND_OPTION_SYNCHRONOUS
			);

			if (WdfRequestSend(Request, IOTarget, &RequestSendOptions) == FALSE)
			{
				status = WdfRequestGetStatus(Request);

				goto exit;
			}

			status = WdfRequestGetStatus(Request);

			if (((status == STATUS_BUFFER_TOO_SMALL) || (status == STATUS_INSUFFICIENT_RESOURCES)) && PartitionCount < 256)
			{
				PartitionCount *= 2;

				WdfObjectDelete(IOCTLRequestMemoryBuffer);
				IOCTLRequestMemoryBuffer = NULL;

				WdfObjectDelete(Request);
				Request = NULL;
			}
			else if (!NT_SUCCESS(status))
			{
				goto exit;
			}
		} while (!NT_SUCCESS(status));

		if (DriverLayoutInformationExtended->PartitionStyle == PARTITION_STYLE_GPT)
		{
			for (DWORD i = 0; i < DriverLayoutInformationExtended->PartitionCount; i++)
			{
				if (RtlCompareMemory(&(DriverLayoutInformationExtended->PartitionEntry[i].Gpt.Name), SURFACE_FIRMWARE_PROVISIONING_DATA, sizeof(SURFACE_FIRMWARE_PROVISIONING_DATA)) == sizeof(SURFACE_FIRMWARE_PROVISIONING_DATA))
				{
					PartitionNumber = DriverLayoutInformationExtended->PartitionEntry[i].PartitionNumber;

					FoundPartition = TRUE;
					break;
				}
			}
		}

		WdfObjectDelete(IOCTLRequestMemoryBuffer);
		IOCTLRequestMemoryBuffer = NULL;

		WdfObjectDelete(Request);
		Request = NULL;

		WdfIoTargetClose(IOTarget);
		WdfObjectDelete(IOTarget);
		IOTarget = NULL;

		ExFreePool(DevicePath);
		DevicePath = NULL;
	} while ((FoundPartition == FALSE) && (HardDiskNumber++ < MAXIMUM_NUMBERS_OF_LUNS));

	if (FoundPartition == TRUE)
	{
		status = RtlStringCbPrintfW(VolumePath, VolumePathLength * sizeof(WCHAR), L"\\Device\\Harddisk%u\\Partition%u\\", HardDiskNumber, PartitionNumber);
	}
	else
	{
		status = STATUS_UNSUCCESSFUL;
	}

exit:
	if (IOCTLRequestMemoryBuffer != NULL)
	{
		WdfObjectDelete(IOCTLRequestMemoryBuffer);
	}

	if (Request != NULL)
	{
		WdfObjectDelete(Request);
	}

	if (IOTarget != NULL)
	{
		WdfIoTargetClose(IOTarget);
		WdfObjectDelete(IOTarget);
	}

	if (DevicePath != NULL)
	{
		ExFreePool(DevicePath);
	}

	return status;
}