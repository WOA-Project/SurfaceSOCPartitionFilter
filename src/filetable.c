//
// filetable.c
//

#include "filter.h"

#include "filetable.h"

#include "constants.h"
#include "qcomdefs.h"

#include "provision.h"
#include "sensor.h"

#include <filetable.tmh>

const FILE_ENTRY g_FileTable[] =
{
    //
    // Static files
    //
    {FileMatchExact, L"QCOM\\BT_NVMTAG36.PROVISION",  FileEntryStatic, {.StaticFile = {BT_NVMTAG36_PROVISION,  sizeof(BT_NVMTAG36_PROVISION_DATA)}}},
    {FileMatchExact, L"QCOM\\BT_NVMTAG83.PROVISION",  FileEntryStatic, {.StaticFile = {BT_NVMTAG83_PROVISION,  sizeof(BT_NVMTAG83_PROVISION_DATA)}}},
    {FileMatchExact, L"QCOM\\WLAN_CLPC.PROVISION",    FileEntryStatic, {.StaticFile = {WLAN_CLPC_PROVISION,    sizeof(WLAN_CLPC_PROVISION)}}},
    {FileMatchExact, L"QCOM\\WLAN_SAR2CFG.PROVISION", FileEntryStatic, {.StaticFile = {WLAN_SAR2CFG_PROVISION, sizeof(WLAN_SAR2CFG_PROVISION_DATA)}}},

    //
    // Dynamic provision files
    //
    {FileMatchExact, L"QCOM\\BT.PROVISION",          FileEntryHandler, {.Handler = HandleBtProvision}},
    {FileMatchExact, L"QCOM\\WLAN.PROVISION",        FileEntryHandler, {.Handler = HandleWlanProvision}},
    {FileMatchExact, L"QCOM\\WLAN_PMICXO.PROVISION", FileEntryHandler, {.Handler = HandleWlanPmicxoProvision}},

    //
    // Sensor JSON files
    //
    {FileMatchPrefix, L"JSON\\", FileEntryHandler, {.Handler = HandleSensorJson}}
};

const ULONG g_FileTableCount = RTL_NUMBER_OF(g_FileTable);
