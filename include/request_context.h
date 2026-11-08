#pragma once

typedef struct _REQUEST_CONTEXT
{
    ULONG InputBufferLength;
} REQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(
    REQUEST_CONTEXT,
    GetRequestContext);