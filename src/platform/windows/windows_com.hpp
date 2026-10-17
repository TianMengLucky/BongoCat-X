#ifndef BONGO_CAT_WINDOWS_COM_HPP
#define BONGO_CAT_WINDOWS_COM_HPP
#include <windows.h>
#include <objbase.h>
#include <wrl/client.h>
#include <memory>
namespace bongo_cat {
using Microsoft::WRL::ComPtr;
class ComApartment {
    HRESULT status_;
public:
    explicit ComApartment(DWORD mode) noexcept : status_(CoInitializeEx(nullptr, mode)) {}
    ~ComApartment() { if (SUCCEEDED(status_)) CoUninitialize(); }
    HRESULT status() const noexcept { return status_; }
    ComApartment(const ComApartment &) = delete;
    ComApartment &operator=(const ComApartment &) = delete;
};
struct TaskFree { void operator()(void *p) const noexcept { CoTaskMemFree(p); } };
template<class T> using TaskMemory = std::unique_ptr<T, TaskFree>;
}
#endif
