#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>

namespace VoidGUI {

    typedef HRESULT(__stdcall* PresentFn)(IDXGISwapChain*, UINT, UINT);
    typedef HRESULT(__stdcall* ResizeBuffersFn)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT);

    class InternalHook {
    public:
        static void** ResolveSwapchainVTable();

        static bool InstallVMTHook(void** vtable, int index, void* hook_fn, void** original_fn);
        static bool RemoveVMTHook(void** vtable, int index, void* original_fn);

        static bool HookWndProc(HWND hWnd, WNDPROC hook_proc, WNDPROC* original_proc);
        static bool UnhookWndProc(HWND hWnd, WNDPROC original_proc);
    };

}
