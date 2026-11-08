#include "filter.h"

#include "dispatch.h"

#include "qcfs.h"

#include "path.h"
#include "lookup.h"

#include "directory.h"
#include "fileproperty.h"

#include "request.h"

#include <dispatch.tmh>

NTSTATUS
HandleReadFile(_In_ PFILTER_CONTEXT Context)
{
    NTSTATUS Status;
    PARSED_PATH ParsedPath;

    Status = GetInputPath(Context, &ParsedPath);
    if (!NT_SUCCESS(Status))
    {
        return Status;
    }

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "ReadFile '%wZ'", &ParsedPath.Path);

    return HandleFileLookup(Context, &ParsedPath.Path);
}

NTSTATUS
DispatchVirtualIoctl(_In_ PFILTER_CONTEXT Context)
{
    switch (Context->Ioctl)
    {
    case IOCTL_QCFS_READ_FILE:
        return HandleReadFile(Context);
    case IOCTL_QCFS_GET_FILE_PROPERTY:
        return HandleFileProperty(Context);
    case IOCTL_QCFS_LIST_DIRECTORY_FILES:
        return HandleDirectoryListing(Context);
    default:
        Trace(TRACE_LEVEL_WARNING, TRACE_DRIVER, "Unsupported ioctl 0x%08X", Context->Ioctl);
        return STATUS_NOT_SUPPORTED;
    }
}