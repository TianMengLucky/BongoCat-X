#include "windows_com.hpp"
#include <cstdio>

static int failures;
#define CHECK(value) do { if (!(value)) { \
    std::fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #value); \
    ++failures; \
} } while (0)

// Observe reference ownership and apartment lifetime without starting the shell.
class Probe final : public IUnknown {
public:
    ULONG references = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **output) override {
        if (!output) return E_POINTER;
        *output = nullptr;
        if (iid != IID_IUnknown) return E_NOINTERFACE;
        *output = static_cast<IUnknown *>(this);
        AddRef();
        return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++references; }
    ULONG STDMETHODCALLTYPE Release() override {
        ULONG_PTR token = 0;
        CHECK(SUCCEEDED(CoGetContextToken(&token)));
        return --references;
    }
};

static void apartment_and_references() {
    Probe probe;
    {
        bongo_cat::ComApartment first(COINIT_APARTMENTTHREADED);
        CHECK(first.status() == S_OK);
        bongo_cat::ComPtr<IUnknown> owner;
        owner.Attach(&probe);
        {
            bongo_cat::ComApartment nested(COINIT_APARTMENTTHREADED);
            CHECK(nested.status() == S_FALSE);
            bongo_cat::ComPtr<IUnknown> copy = owner;
            CHECK(probe.references == 2);
        }
        CHECK(probe.references == 1);
        {
            bongo_cat::ComApartment incompatible(COINIT_MULTITHREADED);
            CHECK(incompatible.status() == RPC_E_CHANGED_MODE);
        }
        ULONG_PTR token = 0;
        CHECK(SUCCEEDED(CoGetContextToken(&token)));
    }
    CHECK(probe.references == 0);
    bongo_cat::ComApartment next(COINIT_MULTITHREADED);
    CHECK(next.status() == S_OK);
}

int main() {
    apartment_and_references();
    return failures ? 1 : 0;
}
