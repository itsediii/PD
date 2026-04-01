#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

void PrintValue(const std::wstring& name, DWORD type, const BYTE* data, DWORD dataSize) {
    std::wcout << L"  Nume:  " << (name.empty() ? L"(Default)" : name) << L"\n";
    std::wcout << L"  Tip:   ";

    switch (type) {
    case REG_SZ:
        std::wcout << L"REG_SZ\n";
        std::wcout << L"  Date:  " << reinterpret_cast<const wchar_t*>(data) << L"\n";
        break;

    case REG_EXPAND_SZ: {
        std::wcout << L"REG_EXPAND_SZ\n";
        std::wcout << L"  Date:  " << reinterpret_cast<const wchar_t*>(data) << L"\n";

        wchar_t expanded[MAX_PATH * 2];
        ExpandEnvironmentStringsW(reinterpret_cast<const wchar_t*>(data), expanded, MAX_PATH * 2);
        std::wcout << L"  Expandat: " << expanded << L"\n";
        break;
    }

    case REG_DWORD:
        std::wcout << L"REG_DWORD\n";
        std::wcout << L"  Date:  " << *reinterpret_cast<const DWORD*>(data)
            << L" (0x" << std::hex << *reinterpret_cast<const DWORD*>(data)
            << std::dec << L")\n";
        break;

    case REG_QWORD:
        std::wcout << L"REG_QWORD\n";
        std::wcout << L"  Date:  " << *reinterpret_cast<const ULONGLONG*>(data) << L"\n";
        break;

    case REG_MULTI_SZ: {
        std::wcout << L"REG_MULTI_SZ\n";
        std::wcout << L"  Date:\n";
        const wchar_t* ptr = reinterpret_cast<const wchar_t*>(data);
        int idx = 0;
        while (*ptr) {
            std::wcout << L"    [" << idx++ << L"] " << ptr << L"\n";
            ptr += wcslen(ptr) + 1;
        }
        break;
    }

    case REG_BINARY: {
        std::wcout << L"REG_BINARY\n";
        std::wcout << L"  Date:  ";
        for (DWORD i = 0; i < dataSize; ++i) {
            wchar_t hex[4];
            swprintf_s(hex, L"%02X ", data[i]);
            std::wcout << hex;
            if ((i + 1) % 16 == 0) std::wcout << L"\n         ";
        }
        std::wcout << L"\n";
        break;
    }

    case REG_NONE:
        std::wcout << L"REG_NONE\n";
        std::wcout << L"  Date:  (fara date)\n";
        break;

    default:
        std::wcout << L"TIP NECUNOSCUT (" << type << L")\n";
        break;
    }

    std::wcout << L"  Dimensiune: " << dataSize << L" bytes\n";
    std::wcout << L"  " << std::wstring(50, L'-') << L"\n";
}

bool EnumerateSubkeyValues(HKEY hRootKey, const std::wstring& subkeyPath) {
    HKEY hKey = nullptr;

    LONG result = RegOpenKeyExW(
        hRootKey,
        subkeyPath.c_str(),
        0,
        KEY_READ,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        std::wcerr << L"Eroare la deschiderea subcheii: " << subkeyPath << L"\n";
        std::wcerr << L"Cod eroare: " << result << L"\n";
        return false;
    }

    DWORD numValues = 0;
    DWORD maxValueNameLen = 0;
    DWORD maxDataLen = 0;

    result = RegQueryInfoKeyW(
        hKey,
        nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr,
        &numValues, 
        &maxValueNameLen,
        &maxDataLen,     
        nullptr, nullptr
    );

    if (result != ERROR_SUCCESS) {
        std::wcerr << L"Eroare la interogarea cheii: " << result << L"\n";
        RegCloseKey(hKey);
        return false;
    }

    std::wcout << L"  Subcheia: " << subkeyPath << L"\n";
    std::wcout << L"  Numar valori: " << numValues << L"\n";

    if (numValues == 0) {
        std::wcout << L"  (Subcheia nu contine valori)\n";
        RegCloseKey(hKey);
        return true;
    }

    std::vector<wchar_t> valueName(maxValueNameLen + 1);
    std::vector<BYTE>    data(maxDataLen + 2);

    for (DWORD i = 0; i < numValues; ++i) {
        DWORD nameLen = maxValueNameLen + 1;
        DWORD dataLen = maxDataLen + 2;
        DWORD type = 0;

        std::fill(valueName.begin(), valueName.end(), L'\0');
        std::fill(data.begin(), data.end(), 0);

        result = RegEnumValueW(
            hKey,
            i,
            valueName.data(),
            &nameLen,
            nullptr,
            &type,
            data.data(),
            &dataLen
        );

        if (result == ERROR_SUCCESS) {
            std::wstring name(valueName.data(), nameLen);
            PrintValue(name, type, data.data(), dataLen);
        }
        else {
            std::wcerr << L"  Eroare la enumerarea valorii " << i
                << L": " << result << L"\n";
        }
    }

    RegCloseKey(hKey);
    return true;
}

HKEY GetRootKey(const std::wstring& rootName) {
    if (rootName == L"HKLM" || rootName == L"HKEY_LOCAL_MACHINE")
        return HKEY_LOCAL_MACHINE;
    if (rootName == L"HKCU" || rootName == L"HKEY_CURRENT_USER")
        return HKEY_CURRENT_USER;
    if (rootName == L"HKCR" || rootName == L"HKEY_CLASSES_ROOT")
        return HKEY_CLASSES_ROOT;
    if (rootName == L"HKU" || rootName == L"HKEY_USERS")
        return HKEY_USERS;
    if (rootName == L"HKCC" || rootName == L"HKEY_CURRENT_CONFIG")
        return HKEY_CURRENT_CONFIG;
    return nullptr;
}

int wmain(int argc, wchar_t* argv[]) {
    _wsetlocale(LC_ALL, L"");

    HKEY    rootKey = HKEY_LOCAL_MACHINE;
    std::wstring subkeyPath;

    if (argc >= 3) {
        rootKey = GetRootKey(argv[1]);
        subkeyPath = argv[2];

        if (!rootKey) {
            std::wcerr << L"Radacina invalida: " << argv[1] << L"\n";
            std::wcerr << L"Valori valide: HKLM, HKCU, HKCR, HKU, HKCC\n";
            return 1;
        }
    }
    else {
        std::wcout << L"Utilizare: RegistryReader.exe <RADACINA> \"<CALE_SUBCHEIE>\"\n";
        std::wcout << L"Exemplu: RegistryReader.exe HKLM \"SOFTWARE\\\\Microsoft\\\\Windows NT\\\\CurrentVersion\"\n\n";
        std::wcout << L"Rulare in mod demonstrativ cu cheia implicita...\n";

        rootKey = HKEY_LOCAL_MACHINE;
        subkeyPath = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    }

    bool ok = EnumerateSubkeyValues(rootKey, subkeyPath);

    if (!ok) {
        std::wcerr << L"\nEsuat. Verificati:\n";
        std::wcerr << L"  - Calea subcheii este corecta\n";
        std::wcerr << L"  - Aveti permisiuni de citire\n";
        std::wcerr << L"  - Rulati ca Administrator (daca e necesar)\n";
        return 1;
    }

    std::wcout << L"\nEnumerare completa cu succes.\n";
    return 0;
}
