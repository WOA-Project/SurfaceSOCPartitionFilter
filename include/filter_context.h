#pragma once

#include <ntddk.h>
#include <wdf.h>

typedef struct _FILTER_CONTEXT
{
    WDFREQUEST Request;
    WDFDEVICE Device;
    WDFMEMORY InputMemory;
    WDFMEMORY OutputMemory;
    PUCHAR InputBuffer;
    PUCHAR OutputBuffer;
    ULONG InputLength;
    ULONG OutputLength;
    ULONG Ioctl;
} FILTER_CONTEXT, *PFILTER_CONTEXT;
