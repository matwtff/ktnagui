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
        // Resolves the 64-bit IDXGISwapChain VTable via throwaway dummy device
        static void** ResolveSwapchainVTable();

        // Lightweight VMT hook primitive (or pointer swap)
        static bool InstallVMTHook(void** vtable, int index, void* hook_fn, void** original_fn);
        static bool RemoveVMTHook(void** vtable, int index, void* original_fn);

        // Subclasses target window procedure for input capture
        static bool HookWndProc(HWND hWnd, WNDPROC hook_proc, WNDPROC* original_proc);
        static bool UnhookWndProc(HWND hWnd, WNDPROC original_proc);
    };

} // namespace VoidGUI
