//
// request.h
//

#pragma once

#include "filter_context.h"
#include "path.h"

NTSTATUS
FilterContextInitialize(
    _In_ WDFREQUEST Request,
    _In_ WDFIOTARGET Target,
    _In_ PWDF_REQUEST_COMPLETION_PARAMS Params,
    _In_ WDFCONTEXT RequestContext,
    _Out_ PFILTER_CONTEXT Context
);

VOID
FilterContextDestroy(
    _In_ PFILTER_CONTEXT Context
);

NTSTATUS
GetInputUlong(
    _In_ PFILTER_CONTEXT Context,
    _In_ ULONG Offset,
    _Out_ PULONG Value
);

NTSTATUS
GetInputPath(
    _In_ PFILTER_CONTEXT Context,
    _Out_ PPARSED_PATH ParsedPath
);