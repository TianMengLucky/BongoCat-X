#include "windows_taskbar.h"
#include "windows_com.hpp"
#include <shobjidl.h>

HRESULT bongo_cat_windows_taskbar_remove(HWND window) {
    bongo_cat::ComApartment apartment(COINIT_APARTMENTTHREADED);
    if (FAILED(apartment.status()) && apartment.status() != RPC_E_CHANGED_MODE)
        return apartment.status();
    bongo_cat::ComPtr<ITaskbarList> taskbar;
    HRESULT result = CoCreateInstance(CLSID_TaskbarList, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(taskbar.GetAddressOf()));
    if (SUCCEEDED(result)) result = taskbar->HrInit();
    if (SUCCEEDED(result)) result = taskbar->DeleteTab(window);
    return result;
}
