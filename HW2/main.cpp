#define INITGUID

#include <windows.h>
#include <initguid.h>    
#include <setupapi.h>
#include <devguid.h>
#include <cfgmgr32.h>
#include <devpropdef.h>
#include <devpkey.h>
#include <fcntl.h>
#include <io.h>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <iomanip>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

struct DeviceInfo {
    std::wstring instanceId;
    std::wstring friendlyName;
    std::wstring description;
    std::wstring manufacturer;
    std::wstring driverVersion;
    std::wstring driverDate;
    std::wstring deviceClass;
    std::wstring classGuid;
    std::wstring locationInfo;
    std::wstring enumeratorName;
    std::wstring service;
    std::wstring status;
    ULONG        problemCode = 0;
    std::vector<std::wstring> hardwareIds;
    std::vector<std::wstring> compatibleIds;
    std::map<std::wstring, std::wstring> extraProps;
};

static std::wstring ReadStringProp(HDEVINFO hDev,
    SP_DEVINFO_DATA& did,
    const DEVPROPKEY& key)
{
    DEVPROPTYPE propType = 0;
    DWORD       needed = 0;
    SetupDiGetDevicePropertyW(hDev, &did, &key,
        &propType, nullptr, 0, &needed, 0);
    if (needed == 0) return {};
    std::vector<BYTE> buf(needed);
    if (!SetupDiGetDevicePropertyW(hDev, &did, &key,
        &propType, buf.data(), needed, nullptr, 0))
        return {};
    return std::wstring(reinterpret_cast<wchar_t*>(buf.data()));
}

static std::vector<std::wstring> ReadStringListProp(HDEVINFO hDev,
    SP_DEVINFO_DATA& did,
    const DEVPROPKEY& key)
{
    DEVPROPTYPE propType = 0;
    DWORD       needed = 0;
    SetupDiGetDevicePropertyW(hDev, &did, &key,
        &propType, nullptr, 0, &needed, 0);
    if (needed == 0) return {};
    std::vector<BYTE> buf(needed);
    if (!SetupDiGetDevicePropertyW(hDev, &did, &key,
        &propType, buf.data(), needed, nullptr, 0))
        return {};
    std::vector<std::wstring> result;
    const wchar_t* p = reinterpret_cast<wchar_t*>(buf.data());
    while (*p) {
        result.emplace_back(p);
        p += result.back().size() + 1;
    }
    return result;
}

static std::wstring ReadFiletimeProp(HDEVINFO hDev,
    SP_DEVINFO_DATA& did,
    const DEVPROPKEY& key)
{
    DEVPROPTYPE propType = 0;
    FILETIME    ft{};
    DWORD       needed = sizeof(ft);
    if (!SetupDiGetDevicePropertyW(hDev, &did, &key,
        &propType,
        reinterpret_cast<PBYTE>(&ft),
        needed, nullptr, 0))
        return {};
    SYSTEMTIME st{};
    FileTimeToSystemTime(&ft, &st);
    wchar_t buf[64];
    swprintf_s(buf, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    return buf;
}

static std::wstring GetDeviceStatus(DEVINST devInst, ULONG& problemCode)
{
    ULONG status = 0;
    ULONG problem = 0;
    CONFIGRET cr = CM_Get_DevNode_Status(&status, &problem, devInst, 0);
    problemCode = problem;
    if (cr != CR_SUCCESS)   return L"Unknown";
    if (status & DN_STARTED)  return L"Running";
    if (problem != 0) {
        wchar_t buf[32];
        swprintf_s(buf, L"Error (code %lu)", problem);
        return buf;
    }
    return L"Stopped";
}


static std::vector<DeviceInfo> EnumerateAllDevices()
{
    std::vector<DeviceInfo> devices;

    HDEVINFO hDevInfo = SetupDiGetClassDevsW(
        nullptr, nullptr, nullptr,
        DIGCF_ALLCLASSES | DIGCF_PRESENT);

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        std::wcerr << L"SetupDiGetClassDevs failed: " << GetLastError() << L"\n";
        return devices;
    }

    SP_DEVINFO_DATA devInfoData{};
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD idx = 0;
        SetupDiEnumDeviceInfo(hDevInfo, idx, &devInfoData);
        ++idx)
    {
        DeviceInfo di;

        di.friendlyName = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_FriendlyName);
        di.description = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_DeviceDesc);
        di.manufacturer = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_Manufacturer);
        di.instanceId = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_InstanceId);
        di.locationInfo = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_LocationInfo);
        di.enumeratorName = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_EnumeratorName);
        di.service = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_Service);
        di.deviceClass = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_Class);
        di.driverVersion = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_DriverVersion);
        di.driverDate = ReadFiletimeProp(hDevInfo, devInfoData, DEVPKEY_Device_DriverDate);
        di.hardwareIds = ReadStringListProp(hDevInfo, devInfoData, DEVPKEY_Device_HardwareIds);
        di.compatibleIds = ReadStringListProp(hDevInfo, devInfoData, DEVPKEY_Device_CompatibleIds);

        wchar_t guidStr[64]{};
        StringFromGUID2(devInfoData.ClassGuid, guidStr, 64);
        di.classGuid = guidStr;

        di.extraProps[L"BusReportedDeviceDesc"] =
            ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_BusReportedDeviceDesc);

        auto locPaths = ReadStringListProp(hDevInfo, devInfoData, DEVPKEY_Device_LocationPaths);
        di.extraProps[L"LocationPaths"] = locPaths.empty() ? L"" : locPaths[0];
        di.extraProps[L"PDOName"] = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_PDOName);
        di.extraProps[L"DriverInfPath"] = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_DriverInfPath);
        di.extraProps[L"DriverInfSection"] = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_DriverInfSection);
        di.extraProps[L"DriverProvider"] = ReadStringProp(hDevInfo, devInfoData, DEVPKEY_Device_DriverProvider);

        {
            DEVPROPTYPE t = 0;
            GUID g{};
            if (SetupDiGetDevicePropertyW(hDevInfo, &devInfoData,
                &DEVPKEY_Device_ContainerId, &t,
                reinterpret_cast<PBYTE>(&g), sizeof(g), nullptr, 0))
            {
                wchar_t buf[64];
                StringFromGUID2(g, buf, 64);
                di.extraProps[L"ContainerId"] = buf;
            }
        }

        di.status = GetDeviceStatus(devInfoData.DevInst, di.problemCode);

        devices.push_back(std::move(di));
    }

    if (GetLastError() != ERROR_NO_MORE_ITEMS)
        std::wcerr << L"Enum stopped early, error: " << GetLastError() << L"\n";

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return devices;
}


static void PrintSep() {
    std::wcout << L"\n" << std::wstring(72, L'-') << L"\n";
}

static void PrintField(const wchar_t* label, const std::wstring& val) {
    if (val.empty()) return;
    std::wcout << L"  " << std::left << std::setw(26) << label
        << L": " << val << L"\n";
}

static void PrintDevices(const std::vector<DeviceInfo>& devices)
{
    int n = 1;
    for (const auto& d : devices)
    {
        PrintSep();
        std::wcout << L"[" << n++ << L"] "
            << (!d.friendlyName.empty() ? d.friendlyName : d.description)
            << L"\n";

        PrintField(L"Instance ID", d.instanceId);
        PrintField(L"Friendly Name", d.friendlyName);
        PrintField(L"Description", d.description);
        PrintField(L"Manufacturer", d.manufacturer);
        PrintField(L"Class", d.deviceClass);
        PrintField(L"Class GUID", d.classGuid);
        PrintField(L"Enumerator", d.enumeratorName);
        PrintField(L"Service", d.service);
        PrintField(L"Status", d.status);
        if (d.problemCode)
            std::wcout << L"  " << std::left << std::setw(26)
            << L"Problem Code" << L": " << d.problemCode << L"\n";
        PrintField(L"Driver Version", d.driverVersion);
        PrintField(L"Driver Date", d.driverDate);
        PrintField(L"Location", d.locationInfo);

        if (!d.hardwareIds.empty()) {
            std::wcout << L"  Hardware IDs:\n";
            for (const auto& id : d.hardwareIds)
                std::wcout << L"      " << id << L"\n";
        }
        if (!d.compatibleIds.empty()) {
            std::wcout << L"  Compatible IDs:\n";
            for (const auto& id : d.compatibleIds)
                std::wcout << L"      " << id << L"\n";
        }
        for (const auto& [k, v] : d.extraProps)
            PrintField(k.c_str(), v);
    }
    PrintSep();
    std::wcout << L"\nTotal: " << devices.size() << L" device(s) found.\n";
}


int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    _setmode(_fileno(stdout), _O_U8TEXT);

    auto devices = EnumerateAllDevices();

    PrintDevices(devices);

    return 0;
}
