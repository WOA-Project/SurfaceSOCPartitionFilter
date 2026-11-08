//
// response.h
//

#pragma once

#include "filter_context.h"
#include "qcfs.h"

VOID
InitializeResponse(
    _In_ PFILTER_CONTEXT Context,
    _In_ NTSTATUS Status,
    _In_ ULONG NeededSize
);

NTSTATUS
BuildStaticFileResponse(
    _In_ PFILTER_CONTEXT Context,
    _In_reads_bytes_(DataSize) const VOID* Data,
    _In_ ULONG DataSize
);

NTSTATUS
BuildFileSizeResponse(
    _In_ PFILTER_CONTEXT Context,
    _In_ ULONG FileSize
);

NTSTATUS
BuildDirectoryResponse(
    _In_ PFILTER_CONTEXT Context,
    _In_ ULONG NumberOfFiles
);

PVOID
GetResponsePayload(
    _In_ PFILTER_CONTEXT Context
);

ULONG
GetResponsePayloadSize(
    _In_ PFILTER_CONTEXT Context
);

NTSTATUS
CommitResponse(
    _In_ PFILTER_CONTEXT Context
);