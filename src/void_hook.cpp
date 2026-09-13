#include "../include/void_hook.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace VoidGUI {

    static void** s_swapchain_vtable = nullptr;

    void** InternalHook::ResolveSwapchainVTable() {
        if (s_swapchain_vtable) return s_swapchain_vtable;

        WNDCLASSEXA wc = {};
        wc.cbSize = sizeof(WNDCLASSEXA);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = DefWindowProcA;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpszClassName = "VoidDummyClass";
        RegisterClassExA(&wc);

        HWND hWnd = CreateWindowA(
            wc.lpszClassName, "VoidDummyWindow",
            WS_OVERLAPPEDWINDOW, 0, 0, 100, 100,
            nullptr, nullptr, wc.hInstance, nullptr
        );

        if (!hWnd) {
            UnregisterClassA(wc.lpszClassName, wc.hInstance);
            return nullptr;
        }

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 1;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = hWnd;
        sd.SampleDesc.Count = 1;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        IDXGISwapChain* swap_chain = nullptr;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        D3D_FEATURE_LEVEL feature_level;
        const D3D_FEATURE_LEVEL fl_array[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            fl_array, 2, D3D11_SDK_VERSION, &sd,
            &swap_chain, &device, &feature_level, &context
        );

        if (hr == S_OK && swap_chain) {
            void** vtable = *reinterpret_cast<void***>(swap_chain);
            s_swapchain_vtable = vtable;

            swap_chain->Release();
            context->Release();
            device->Release();
        }

        DestroyWindow(hWnd);
        UnregisterClassA(wc.lpszClassName, wc.hInstance);

        return s_swapchain_vtable;
    }

    bool InternalHook::InstallVMTHook(void** vtable, int index, void* hook_fn, void** original_fn) {
        if (!vtable || index < 0 || !hook_fn) return false;

        DWORD old_protect;
        if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protect)) {
            return false;
        }

        if (original_fn) {
            *original_fn = vtable[index];
        }

        vtable[index] = hook_fn;
        VirtualProtect(&vtable[index], sizeof(void*), old_protect, &old_protect);
        return true;
    }

    bool InternalHook::RemoveVMTHook(void** vtable, int index, void* original_fn) {
        if (!vtable || index < 0 || !original_fn) return false;

        DWORD old_protect;
        if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &old_protect)) {
            return false;
        }

        vtable[index] = original_fn;
        VirtualProtect(&vtable[index], sizeof(void*), old_protect, &old_protect);
        return true;
    }

    bool InternalHook::HookWndProc(HWND hWnd, WNDPROC hook_proc, WNDPROC* original_proc) {
        if (!hWnd || !hook_proc || !original_proc) return false;
        *original_proc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(hook_proc))
        );
        return (*original_proc != nullptr);
    }

    bool InternalHook::UnhookWndProc(HWND hWnd, WNDPROC original_proc) {
        if (!hWnd || !original_proc) return false;
        SetWindowLongPtrA(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original_proc));
        return true;
    }

}
