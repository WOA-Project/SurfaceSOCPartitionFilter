//
// provision.h
//

#pragma once

#include "filter_context.h"

NTSTATUS
HandleBtProvision(
    _In_ PFILTER_CONTEXT Context,
    _In_ PCUNICODE_STRING Path
);

NTSTATUS
HandleWlanProvision(
    _In_ PFILTER_CONTEXT Context,
    _In_ PCUNICODE_STRING Path
);

NTSTATUS
HandleWlanPmicxoProvision(
    _In_ PFILTER_CONTEXT Context,
    _In_ PCUNICODE_STRING Path
);