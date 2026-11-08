#pragma once

#include <ntddk.h>
#include <wdf.h>

//
// IOCTLs
//

#define FILE_DEVICE_QCFS 0xECAF

#define IOCTL_QCFS_READ_FILE \
    CTL_CODE(FILE_DEVICE_QCFS, 0xCB0, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

#define IOCTL_QCFS_GET_FILE_PROPERTY \
    CTL_CODE(FILE_DEVICE_QCFS, 0xCB1, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

#define IOCTL_QCFS_LIST_DIRECTORY_FILES \
    CTL_CODE(FILE_DEVICE_QCFS, 0xCB3, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)

//
// Properties
//

#define QCFS_PROTOCOL_HEADER_SIZE      296
#define QCFS_PATH_OFFSET               88
#define QCFS_FILE_PROPERTY_OFFSET      280
#define QCFS_DIRECTORY_PROPERTY_OFFSET 284
#define QCFS_FILE_PROPERTY_SIZE        2
#define QCFS_DIRECTORY_PROPERTY_FILES  10

//
// Directory layout
//

#define QCFS_DIRECTORY_ENTRY_SIZE       244

//
// Response
//

#pragma pack(push, 1)

typedef struct _QCFS_RESPONSE
{
    ULONG Ioctl;
    NTSTATUS Status;
    ULONG NeededSize;
    ULONG Reserved;
    ULONG DataSize;
    UCHAR Data[ANYSIZE_ARRAY];
} QCFS_RESPONSE, *PQCFS_RESPONSE;

#pragma pack(pop)

//
// Directory entry
//

#pragma pack(push, 1)

typedef struct _QCFS_DIRECTORY_ENTRY
{
    WCHAR FileName[49];
    WCHAR DirectoryName[49];
    ULONG FileAllocationSize;
    ULONG FileSize;
    ULONG Flags;
    ULONG FileOffset;
    ULONG Reserved[8];
} QCFS_DIRECTORY_ENTRY, *PQCFS_DIRECTORY_ENTRY;

#pragma pack(pop)

C_ASSERT(sizeof(QCFS_DIRECTORY_ENTRY) == 244);