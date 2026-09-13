#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include "void_types.hpp"

namespace VoidGUI {

    class ExternalOverlay {
    public:
        static bool Initialize(const char* target_window_title, int fallback_w = 1920, int fallback_h = 1080);
        static void Shutdown();
        static bool ProcessMessages();
        static void SyncWithTarget();
        static void SetClickThrough(bool click_through);

        static HWND GetOverlayHWND();
        static HWND GetTargetHWND();
        static ID3D11Device* GetDevice();
        static ID3D11DeviceContext* GetContext();
        static IDXGISwapChain* GetSwapChain();
        static ID3D11RenderTargetView* GetRenderTarget();

        static void BeginFrame();
        static void EndFrame(bool vsync = true);

        static bool IsRunning();
        static Vec2 GetScreenSize();
    };

}
