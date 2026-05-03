#include <windows.h>
#include <tchar.h>

SERVICE_STATUS          g_Status = {};
SERVICE_STATUS_HANDLE   g_Handle = NULL;
HANDLE                  g_StopEvent = NULL;

VOID WINAPI ServiceCtrlHandler(DWORD dwCtrl)
{
    if (dwCtrl == SERVICE_CONTROL_STOP)
    {
        g_Status.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_Handle, &g_Status);
        SetEvent(g_StopEvent);
    }
}

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
    g_Handle = RegisterServiceCtrlHandler(L"HelloWorldService", ServiceCtrlHandler);

    g_Status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_Status.dwCurrentState = SERVICE_RUNNING;
    g_Status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    SetServiceStatus(g_Handle, &g_Status);

    MessageBox(NULL, L"Hello World!", L"HelloWorldService", MB_OK);

    g_StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    WaitForSingleObject(g_StopEvent, INFINITE);
    CloseHandle(g_StopEvent);

    g_Status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_Handle, &g_Status);
}

int main()
{
    SERVICE_TABLE_ENTRY table[] =
    {
        { (LPWSTR)L"HelloWorldService", ServiceMain },
        { NULL, NULL }
    };

    StartServiceCtrlDispatcher(table);
    return 0;
}
