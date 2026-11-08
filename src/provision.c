//
// provision.c
//

#include "filter.h"

#include "provision.h"
#include "response.h"

#include "constants.h"
#include "qcomdefs.h"
#include "sfpd.h"

#include <provision.tmh>

static UCHAR
HexNibbleToValue(_In_ UCHAR Character)
{
    if (Character >= '0' && Character <= '9')
    {
        return Character - '0';
    }

    if (Character >= 'A' && Character <= 'F')
    {
        return Character - 'A' + 10;
    }

    if (Character >= 'a' && Character <= 'f')
    {
        return Character - 'a' + 10;
    }

    return 0xFF;
}

static NTSTATUS
DecodeWlanMac(_In_reads_(33) PUCHAR WlanMacBuffer, _Out_writes_(6) PUCHAR DecodedMac)
{
    ULONG i;

    if (WlanMacBuffer == NULL || DecodedMac == NULL)
    {
        return STATUS_INVALID_PARAMETER;
    }

    RtlZeroMemory(DecodedMac, 6);

    //
    // Original Qualcomm format:
    //
    // XXXXXXXX001122334455
    //         ^ starts at +16
    //

    for (i = 0; i < 6; i++)
    {
        UCHAR High;
        UCHAR Low;

        High = HexNibbleToValue(WlanMacBuffer[16 + (i * 2)]);
        Low = HexNibbleToValue(WlanMacBuffer[16 + (i * 2) + 1]);

        if (High == 0xFF || Low == 0xFF)
        {
            return STATUS_DATA_ERROR;
        }

        DecodedMac[i] = (High << 4) | Low;
    }

    return STATUS_SUCCESS;
}

NTSTATUS
HandleBtProvision(_In_ PFILTER_CONTEXT Context, _In_ PCUNICODE_STRING Path)
{
    NTSTATUS status;
    UCHAR BtNv[9];
    BT_PROVISION_DATA Provision;

    UNREFERENCED_PARAMETER(Path);

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Building BT.PROVISION");

    RtlCopyMemory(&Provision, BT_PROVISION, sizeof(Provision));

    status = GetSFPDItem(Context->Device, BT_NV_FILE_PATH, BtNv, sizeof(BtNv));
    if (!NT_SUCCESS(status))
    {
        Trace(TRACE_LEVEL_ERROR, TRACE_DRIVER, "GetSFPDItem(%ws) failed 0x%08X", BT_NV_FILE_PATH, status);
        return status;
    }

    //
    // Original implementation:
    //
    // LocalProvision[2] = BtNv[8];
    // LocalProvision[3] = BtNv[7];
    // ...
    //

    Provision.BDAddress[0] = BtNv[8];
    Provision.BDAddress[1] = BtNv[7];
    Provision.BDAddress[2] = BtNv[6];
    Provision.BDAddress[3] = BtNv[5];
    Provision.BDAddress[4] = BtNv[4];
    Provision.BDAddress[5] = BtNv[3];

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_DRIVER,
        "BT MAC %02X:%02X:%02X:%02X:%02X:%02X",
        Provision.BDAddress[0],
        Provision.BDAddress[1],
        Provision.BDAddress[2],
        Provision.BDAddress[3],
        Provision.BDAddress[4],
        Provision.BDAddress[5]);

    return BuildStaticFileResponse(Context, &Provision, sizeof(Provision));
}

NTSTATUS
HandleWlanProvision(_In_ PFILTER_CONTEXT Context, _In_ PCUNICODE_STRING Path)
{
    NTSTATUS status;
    UCHAR RawMac[33];
    UCHAR DecodedMac[6];
    WLAN_PROVISION_DATA Provision;

    UNREFERENCED_PARAMETER(Path);

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Building WLAN.PROVISION");

    RtlCopyMemory(&Provision, WLAN_PROVISION, sizeof(Provision));

    status = GetSFPDItem(Context->Device, WLAN_MAC_FILE_PATH, RawMac, sizeof(RawMac));

    if (!NT_SUCCESS(status))
    {
        Trace(TRACE_LEVEL_ERROR, TRACE_DRIVER, "GetSFPDItem(%ws) failed 0x%08X", WLAN_MAC_FILE_PATH, status);
        return status;
    }

    status = DecodeWlanMac(RawMac, DecodedMac);

    if (!NT_SUCCESS(status))
    {
        Trace(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Failed to decode WLAN MAC");
        return status;
    }

    RtlCopyMemory(Provision.StationAddress, DecodedMac, sizeof(DecodedMac));

    Trace(
        TRACE_LEVEL_INFORMATION,
        TRACE_DRIVER,
        "WLAN MAC %02X:%02X:%02X:%02X:%02X:%02X",
        DecodedMac[0],
        DecodedMac[1],
        DecodedMac[2],
        DecodedMac[3],
        DecodedMac[4],
        DecodedMac[5]);

    return BuildStaticFileResponse(Context, &Provision, sizeof(Provision));
}

NTSTATUS
HandleWlanPmicxoProvision(_In_ PFILTER_CONTEXT Context, _In_ PCUNICODE_STRING Path)
{
    PQCFS_RESPONSE Response;

    UNREFERENCED_PARAMETER(Path);

    Trace(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "WLAN_PMICXO.PROVISION requested");

    InitializeResponse(Context, STATUS_FILE_NOT_AVAILABLE, 0);

    Response = (PQCFS_RESPONSE)Context->OutputBuffer;
    Response->DataSize = 1;
    Response->Data[0] = 0;

    return STATUS_SUCCESS;
}