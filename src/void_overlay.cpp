#include "../include/void_overlay.hpp"
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace VoidGUI {

    static HWND                    s_overlay_hwnd = nullptr;
    static HWND                    s_target_hwnd = nullptr;
    static ID3D11Device*           s_device = nullptr;
    static ID3D11DeviceContext*    s_context = nullptr;
    static IDXGISwapChain*         s_swap_chain = nullptr;
    static ID3D11RenderTargetView* s_rtv = nullptr;
    static bool                    s_running = false;
    static int                     s_width = 1920;
    static int                     s_height = 1080;
    static bool                    s_click_through = false;

    static void CreateOverlayRTV() {
        ID3D11Texture2D* back_buffer = nullptr;
        s_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
        if (back_buffer) {
            s_device->CreateRenderTargetView(back_buffer, nullptr, &s_rtv);
            back_buffer->Release();
        }
    }

    static void CleanupOverlayRTV() {
        if (s_rtv) {
            s_rtv->Release();
            s_rtv = nullptr;
        }
    }

    static LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }

    bool ExternalOverlay::Initialize(const char* target_window_title, int fallback_w, int fallback_h) {
        s_width = fallback_w;
        s_height = fallback_h;

        if (target_window_title && *target_window_title) {
            s_target_hwnd = FindWindowA(nullptr, target_window_title);
            if (s_target_hwnd) {
                RECT rect;
                if (GetClientRect(s_target_hwnd, &rect)) {
                    s_width = rect.right - rect.left;
                    s_height = rect.bottom - rect.top;
                }
            }
        }

        HINSTANCE hInst = GetModuleHandle(nullptr);
        WNDCLASSEXA wc = {
            sizeof(WNDCLASSEXA), CS_HREDRAW | CS_VREDRAW, OverlayWndProc,
            0, 0, hInst, nullptr, nullptr, nullptr, nullptr,
            "VoidGUI_ExternalOverlay", nullptr
        };
        RegisterClassExA(&wc);

        s_overlay_hwnd = CreateWindowExA(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
            wc.lpszClassName, "VoidGUI Overlay",
            WS_POPUP, 0, 0, s_width, s_height,
            nullptr, nullptr, hInst, nullptr
        );

        if (!s_overlay_hwnd) return false;

        SetLayeredWindowAttributes(s_overlay_hwnd, RGB(0, 0, 0), 255, LWA_ALPHA);
        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(s_overlay_hwnd, &margins);

        DXGI_SWAP_CHAIN_DESC sd = {};
        sd.BufferCount = 2;
        sd.BufferDesc.Width = s_width;
        sd.BufferDesc.Height = s_height;
        sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.BufferDesc.RefreshRate.Numerator = 60;
        sd.BufferDesc.RefreshRate.Denominator = 1;
        sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.OutputWindow = s_overlay_hwnd;
        sd.SampleDesc.Count = 1;
        sd.SampleDesc.Quality = 0;
        sd.Windowed = TRUE;
        sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        D3D_FEATURE_LEVEL fl;
        const D3D_FEATURE_LEVEL fl_array[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
        HRESULT hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            fl_array, 2, D3D11_SDK_VERSION, &sd,
            &s_swap_chain, &s_device, &fl, &s_context
        );

        if (hr != S_OK) return false;

        CreateOverlayRTV();
        ShowWindow(s_overlay_hwnd, SW_SHOW);
        UpdateWindow(s_overlay_hwnd);

        s_running = true;
        s_click_through = true;
        return true;
    }

    void ExternalOverlay::Shutdown() {
        CleanupOverlayRTV();
        if (s_swap_chain) { s_swap_chain->Release(); s_swap_chain = nullptr; }
        if (s_context) { s_context->Release(); s_context = nullptr; }
        if (s_device) { s_device->Release(); s_device = nullptr; }
        if (s_overlay_hwnd) {
            DestroyWindow(s_overlay_hwnd);
            UnregisterClassA("VoidGUI_ExternalOverlay", GetModuleHandle(nullptr));
            s_overlay_hwnd = nullptr;
        }
        s_running = false;
    }

    bool ExternalOverlay::ProcessMessages() {
        MSG msg;
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
            if (msg.message == WM_QUIT) {
                s_running = false;
                return false;
            }
        }
        return s_running;
    }

    void ExternalOverlay::SyncWithTarget() {
        if (!s_target_hwnd) return;
        if (!IsWindow(s_target_hwnd)) return;

        POINT pt = { 0, 0 };
        ClientToScreen(s_target_hwnd, &pt);

        RECT rect;
        GetClientRect(s_target_hwnd, &rect);
        int w = rect.right - rect.left;
        int h = rect.bottom - rect.top;

        if (w != s_width || h != s_height) {
            s_width = w;
            s_height = h;
            CleanupOverlayRTV();
            s_swap_chain->ResizeBuffers(0, s_width, s_height, DXGI_FORMAT_UNKNOWN, 0);
            CreateOverlayRTV();
        }

        SetWindowPos(s_overlay_hwnd, HWND_TOPMOST, pt.x, pt.y, s_width, s_height, SWP_NOACTIVATE);
    }

    void ExternalOverlay::SetClickThrough(bool click_through) {
        if (s_click_through == click_through) return;
        s_click_through = click_through;

        LONG_PTR ex_style = GetWindowLongPtrA(s_overlay_hwnd, GWL_EXSTYLE);
        if (click_through) {
            SetWindowLongPtrA(s_overlay_hwnd, GWL_EXSTYLE, ex_style | WS_EX_TRANSPARENT);
        } else {
            SetWindowLongPtrA(s_overlay_hwnd, GWL_EXSTYLE, ex_style & ~WS_EX_TRANSPARENT);
            SetForegroundWindow(s_overlay_hwnd);
        }
    }

    HWND ExternalOverlay::GetOverlayHWND() { return s_overlay_hwnd; }
    HWND ExternalOverlay::GetTargetHWND() { return s_target_hwnd; }
    ID3D11Device* ExternalOverlay::GetDevice() { return s_device; }
    ID3D11DeviceContext* ExternalOverlay::GetContext() { return s_context; }
    IDXGISwapChain* ExternalOverlay::GetSwapChain() { return s_swap_chain; }
    ID3D11RenderTargetView* ExternalOverlay::GetRenderTarget() { return s_rtv; }

    void ExternalOverlay::BeginFrame() {
        const float transparent[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        s_context->OMSetRenderTargets(1, &s_rtv, nullptr);
        s_context->ClearRenderTargetView(s_rtv, transparent);
    }

    void ExternalOverlay::EndFrame(bool vsync) {
        s_swap_chain->Present(vsync ? 1 : 0, 0);
    }

    bool ExternalOverlay::IsRunning() { return s_running; }
    Vec2 ExternalOverlay::GetScreenSize() { return Vec2(static_cast<float>(s_width), static_cast<float>(s_height)); }

}
