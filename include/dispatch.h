#pragma once

#include "filter_context.h"

NTSTATUS
DispatchVirtualIoctl(
    _In_ PFILTER_CONTEXT Context
    );

NTSTATUS
HandleReadFile(
    _In_ PFILTER_CONTEXT Context
    );