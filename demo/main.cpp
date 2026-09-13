#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <chrono>
#include <string>
#include <algorithm>
#include "../include/void_gui.hpp"
#include "../include/void_gui_d3d11.hpp"
#include "../include/void_spotify.hpp"
#include "../include/void_playerbar.hpp"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

static ID3D11Device*           g_pd3dDevice = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*         g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
static bool                    g_Running = true;
static int                     g_Width = 1280;
static int                     g_Height = 720;

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

void SaveBackBufferToBMP(ID3D11Device* device, ID3D11DeviceContext* ctx, IDXGISwapChain* swap, const char* filepath) {
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (FAILED(swap->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) return;

    D3D11_TEXTURE2D_DESC desc;
    pBackBuffer->GetDesc(&desc);

    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;

    ID3D11Texture2D* pStaging = nullptr;
    if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &pStaging))) {
        ctx->CopyResource(pStaging, pBackBuffer);
        D3D11_MAPPED_SUBRESOURCE map;
        if (SUCCEEDED(ctx->Map(pStaging, 0, D3D11_MAP_READ, 0, &map))) {
            BITMAPFILEHEADER bfh = {};
            BITMAPINFOHEADER bih = {};
            bih.biSize = sizeof(BITMAPINFOHEADER);
            bih.biWidth = desc.Width;
            bih.biHeight = desc.Height;
            bih.biPlanes = 1;
            bih.biBitCount = 32;
            bih.biCompression = BI_RGB;
            DWORD imageSize = desc.Width * desc.Height * 4;
            bih.biSizeImage = imageSize;

            bfh.bfType = 0x4D42; // 'BM'
            bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
            bfh.bfSize = bfh.bfOffBits + imageSize;

            FILE* f = nullptr;
            fopen_s(&f, filepath, "wb");
            if (f) {
                fwrite(&bfh, sizeof(bfh), 1, f);
                fwrite(&bih, sizeof(bih), 1, f);
                for (int y = (int)desc.Height - 1; y >= 0; --y) {
                    uint8_t* row = (uint8_t*)map.pData + y * map.RowPitch;
                    uint32_t bgra_row[4096];
                    for (UINT x = 0; x < desc.Width; ++x) {
                        uint8_t r = row[x * 4 + 0];
                        uint8_t g = row[x * 4 + 1];
                        uint8_t b = row[x * 4 + 2];
                        uint8_t a = row[x * 4 + 3];
                        bgra_row[x] = (static_cast<uint32_t>(a) << 24) |
                                      (static_cast<uint32_t>(r) << 16) |
                                      (static_cast<uint32_t>(g) << 8)  |
                                      static_cast<uint32_t>(b);
                    }
                    fwrite(bgra_row, sizeof(uint32_t), desc.Width, f);
                }
                fclose(f);
            }
            ctx->Unmap(pStaging, 0);
        }
        pStaging->Release();
    }
    pBackBuffer->Release();
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);

    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    VoidGUI::Context* ctx = VoidGUI::GetCurrentContext();
    switch (msg) {
    case WM_LBUTTONDOWN:
        SetCapture(hWnd);
        if (ctx) ctx->SetMouseDown(true);
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        if (ctx) ctx->SetMouseDown(false);
        return 0;
    case WM_MOUSEWHEEL:
        if (ctx) ctx->SetMouseWheel(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA);
        return 0;
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_Width = LOWORD(lParam);
            g_Height = HIWORD(lParam);
            g_pSwapChain->ResizeBuffers(0, g_Width, g_Height, DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    SetUnhandledExceptionFilter([](EXCEPTION_POINTERS* p) -> LONG {
        FILE* f = nullptr;
        fopen_s(&f, "crash.log", "w");
        if (f) {
            fprintf(f, "Crash code: 0x%08lX at %p\n", p->ExceptionRecord->ExceptionCode, p->ExceptionRecord->ExceptionAddress);
            fclose(f);
        }
        return EXCEPTION_EXECUTE_HANDLER;
    });

    WNDCLASSEXA wc = {
        sizeof(WNDCLASSEXA), CS_CLASSDC, WndProc, 0L, 0L,
        hInstance, nullptr, nullptr, nullptr, nullptr,
        "ktna_framework", nullptr
    };
    RegisterClassExA(&wc);

    HWND hWnd = CreateWindowA(
        wc.lpszClassName, "ktna.wtf // Standalone C++20 D3D11 Engine",
        WS_OVERLAPPEDWINDOW, 100, 100, g_Width, g_Height,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(hWnd)) {
        CleanupDeviceD3D();
        UnregisterClassA(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    VoidGUI::Context gui;
    VoidGUI::D3D11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    bool liquid_glass_mode = true;
    gui.GetTheme().SetLiquidGlass(liquid_glass_mode);

    // Aimbot state
    bool aimbot_enabled = true;
    bool silent_aim = false;
    int  hitbox_target = 0;
    const char* hitbox_options[] = { "Head", "Neck", "Upper Chest", "Pelvis" };
    float aimbot_fov = 14.5f;
    float aimbot_dist = 250.0f;
    bool triggerbot_enabled = false;
    int  trigger_delay = 35;
    bool rcs_enabled = true;
    float aimbot_smooth = 3.2f;
    float aimbot_jitter = 0.8f;
    int  aimbot_key = 0x02; // Right mouse button
    int  aimbot_mode = 0;
    const char* mode_options[] = { "Hold Key", "Toggle Key", "Always Active" };

    // Visuals Tab
    bool esp_box = true;
    bool esp_skeleton = true;
    bool esp_health = true;
    bool esp_armor = false;
    bool esp_names = true;
    bool esp_snaplines = false;
    bool esp_dropped_weapons = true;
    bool esp_c4_timer = true;
    bool chams_enabled = true;
    int  chams_material = 0;
    const char* chams_options[] = { "Flat Glow", "Metallic", "Wireframe", "Glass" };
    VoidGUI::Color chams_color = VoidGUI::Color(124, 58, 237, 255);
    VoidGUI::Color chams_occluded = VoidGUI::Color(239, 68, 68, 200);
    float esp_distance = 450.0f;

    // Misc Tab
    bool bhop_enabled = true;
    bool strafe_assist = true;
    bool edge_jump = false;
    float speed_hack = 1.0f;
    bool no_flash = true;
    bool no_smoke = false;
    float fov_offset = 12.0f;

    // Config Tab
    int  config_slot = 0;
    const char* config_slots[] = { "Tournament Legitimate", "Semi-Rage Dynamic", "HvH Aggressive", "Stealth Research" };

    // Navigation & Layout
    int selected_tab = 0;
    const char* tabs[] = { "Aimbot", "Visuals", "Misc", "Config" };

    // Movable / Draggable HUD Widgets (Symmetric, non-overlapping default layout)
    VoidGUI::Vec2 hud_pos(18.0f, 55.0f);
    VoidGUI::Vec2 playerbar_pos(18.0f, 260.0f);
    VoidGUI::Vec2 spotify_pos(910.0f, 55.0f);
    VoidGUI::Vec2 preview_pos(640.0f, 410.0f);
    bool hud_open = true;
    bool preview_open = true;
    bool spotify_open = true;
    bool playerbar_open = true;

    VoidGUI::SpotifyPlayer::Initialize();

    float scan_timer = 0.0f;
    bool menu_open = true;
    bool last_insert_state = false;

    auto last_time = std::chrono::high_resolution_clock::now();
    int frame_count = 0;
    float fps = 0.0f;
    float fps_timer = 0.0f;

    // Main Engine Render Loop
    MSG msg = {};
    while (g_Running && msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        // Toggle menu with INSERT key (polled directly, zero hooks)
        bool insert_down = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (insert_down && !last_insert_state) {
            menu_open = !menu_open;
        }
        last_insert_state = insert_down;

        // Delta Time calculation
        auto current_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> delta = current_time - last_time;
        last_time = current_time;
        float dt = delta.count();

        // FPS calculation
        frame_count++;
        fps_timer += dt;
        if (fps_timer >= 0.5f) {
            fps = frame_count / fps_timer;
            frame_count = 0;
            fps_timer = 0.0f;
        }

        scan_timer += dt * 0.45f;
        if (scan_timer > 1.0f) scan_timer = 0.0f;

        // Poll hardware mouse directly
        POINT mouse_pt;
        if (GetCursorPos(&mouse_pt) && ScreenToClient(hWnd, &mouse_pt)) {
            gui.SetMousePos(static_cast<float>(mouse_pt.x), static_cast<float>(mouse_pt.y));
        }
        bool is_focused = (GetForegroundWindow() == hWnd);
        bool is_lbutton = is_focused && ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        gui.SetMouseDown(is_lbutton);

        // Synchronize liquid glass mode if changed via titlebar badge click
        liquid_glass_mode = gui.GetTheme().liquid_glass;

        // Start Frame
        gui.NewFrame(dt);
        VoidGUI::D3D11_NewFrame();
        VoidGUI::DrawList* dl = gui.GetDrawList();

        // Canvas grid
        for (float x = 0; x < g_Width; x += 40.0f) {
            dl->AddLine(VoidGUI::Vec2(x, 0), VoidGUI::Vec2(x, (float)g_Height), VoidGUI::Color(16, 18, 26, 140), 1.0f);
        }
        for (float y = 0; y < g_Height; y += 40.0f) {
            dl->AddLine(VoidGUI::Vec2(0, y), VoidGUI::Vec2((float)g_Width, y), VoidGUI::Color(16, 18, 26, 140), 1.0f);
        }
        // Glowing tactical intersection crosses
        for (float x = 120.0f; x < g_Width; x += 120.0f) {
            for (float y = 120.0f; y < g_Height; y += 120.0f) {
                dl->AddLine(VoidGUI::Vec2(x - 3.0f, y), VoidGUI::Vec2(x + 3.0f, y), VoidGUI::Color(124, 58, 237, 75), 1.0f);
                dl->AddLine(VoidGUI::Vec2(x, y - 3.0f), VoidGUI::Vec2(x, y + 3.0f), VoidGUI::Color(124, 58, 237, 75), 1.0f);
            }
        }

        // Watermark
        {
            char wm_buf[64];
            snprintf(wm_buf, sizeof(wm_buf), "ktna.wtf | mat | %.0f fps | 12ms", fps > 0.0f ? fps : 180.0f);
            VoidGUI::Vec2 wm_sz = VoidGUI::Font::CalcTextSize(wm_buf, 0.95f);
            VoidGUI::Vec2 wm_min(g_Width - wm_sz.x - 36.0f, 14.0f);
            VoidGUI::Vec2 wm_max(g_Width - 14.0f, 38.0f);

            if (gui.GetTheme().liquid_glass) {
                dl->AddLiquidGlassPanel(wm_min, wm_max, 4.0f, gui.GetTheme().liquid_time, VoidGUI::Color(14, 18, 28, 195), VoidGUI::Color(255, 255, 255, 90), gui.GetTheme().accent);
                dl->AddRectFilledGradient(wm_min, VoidGUI::Vec2(wm_max.x, wm_min.y + 2.0f),
                    gui.GetTheme().accent_gradient, gui.GetTheme().accent, gui.GetTheme().accent, gui.GetTheme().accent_gradient);
            } else {
                dl->AddShadow(wm_min, wm_max, 5.0f, 8.0f, VoidGUI::Color(0, 0, 0, 160));
                dl->AddRectFilled(wm_min, wm_max, VoidGUI::Color(14, 16, 24, 250), 4.0f);
                dl->AddRect(wm_min, wm_max, VoidGUI::Color(42, 48, 68, 255), 4.0f, 1.0f);
                dl->AddRectFilledGradient(wm_min, VoidGUI::Vec2(wm_max.x, wm_min.y + 2.0f),
                    VoidGUI::Color(124, 58, 237, 255), VoidGUI::Color(192, 132, 252, 255),
                    VoidGUI::Color(192, 132, 252, 255), VoidGUI::Color(124, 58, 237, 255));
            }

            // Status indicator pip
            dl->AddCircleFilled(wm_min + VoidGUI::Vec2(12.0f, 14.0f), 3.0f, VoidGUI::Color(34, 197, 94, 255));
            dl->AddText(wm_min + VoidGUI::Vec2(22.0f, 5.0f), VoidGUI::Color(240, 245, 255, 255), wm_buf, 0.95f);
        }

        // In-game entity ESP
        if (esp_box) {
            VoidGUI::Vec2 box_min(410, 200);
            VoidGUI::Vec2 box_max(500, 440);
            dl->AddShadow(box_min, box_max, 2.0f, 4.0f, VoidGUI::Color(0, 0, 0, 120));
            dl->AddRect(box_min, box_max, chams_color, 2.0f, 1.5f);

            if (esp_names) {
                dl->AddText(box_min - VoidGUI::Vec2(-6, 15), VoidGUI::Color(255, 255, 255), "TARGET [142m]", 1.0f);
            }

            if (esp_health) {
                // Dynamic health bar on left of box
                VoidGUI::Vec2 h_min(box_min.x - 6.0f, box_min.y);
                VoidGUI::Vec2 h_max(box_min.x - 2.0f, box_max.y);
                dl->AddRectFilled(h_min, h_max, VoidGUI::Color(15, 17, 24, 200), 1.0f);
                float hp_ratio = 0.78f; // 78 HP
                dl->AddRectFilled(VoidGUI::Vec2(h_min.x, box_max.y - (box_max.y - box_min.y) * hp_ratio), h_max, VoidGUI::Color(34, 197, 94, 255), 1.0f);
            }

            if (esp_skeleton) {
                VoidGUI::Vec2 head(455, 225);
                VoidGUI::Vec2 neck(455, 240);
                VoidGUI::Vec2 pelvis(455, 320);
                VoidGUI::Vec2 l_shoulder(430, 255);
                VoidGUI::Vec2 r_shoulder(480, 255);
                VoidGUI::Vec2 l_hand(425, 310);
                VoidGUI::Vec2 r_hand(485, 310);
                VoidGUI::Vec2 left_foot(435, 430);
                VoidGUI::Vec2 right_foot(475, 430);

                dl->AddCircle(head, 11.0f, VoidGUI::Color(255, 255, 255), 18, 1.2f);
                dl->AddLine(head + VoidGUI::Vec2(0, 11), neck, VoidGUI::Color(255, 255, 255), 1.2f);
                dl->AddLine(neck, pelvis, VoidGUI::Color(255, 255, 255), 1.2f);
                dl->AddLine(neck, l_shoulder, VoidGUI::Color(255, 255, 255), 1.2f);
                dl->AddLine(neck, r_shoulder, VoidGUI::Color(255, 255, 255), 1.2f);
                dl->AddLine(l_shoulder, l_hand, VoidGUI::Color(255, 255, 255), 1.2f);
                dl->AddLine(r_shoulder, r_hand, VoidGUI::Color(255, 255, 255), 1.2f);
                dl->AddLine(pelvis, left_foot, VoidGUI::Color(255, 255, 255), 1.2f);
                dl->AddLine(pelvis, right_foot, VoidGUI::Color(255, 255, 255), 1.2f);
            }

            if (esp_snaplines) {
                dl->AddLine(VoidGUI::Vec2(g_Width * 0.5f, (float)g_Height), VoidGUI::Vec2(455, 440), chams_color.WithAlpha(180), 1.2f);
            }
        }

        // Dynamic FOV Circle
        if (aimbot_enabled) {
            dl->AddCircle(VoidGUI::Vec2(g_Width * 0.5f, g_Height * 0.5f), aimbot_fov * 8.0f, VoidGUI::Color(124, 58, 237, 90), 48, 1.2f);
        }

        // HUD: Keybinds
        if (hud_open && gui.BeginDraggableCard("Keybinds", &hud_pos, VoidGUI::Vec2(300, 192), &hud_open)) {
            auto draw_bind_row = [&](const char* name, const char* mode, const char* key_lbl, bool active) {
                VoidGUI::Vec2 row_pos = gui.GetCurrentWindow().cursor;
                float row_w = gui.GetCurrentWindow().content_width;
                float row_h = 22.0f;

                VoidGUI::Color name_col = active ? VoidGUI::Color(240, 243, 255) : VoidGUI::Color(120, 126, 145);
                dl->AddText(row_pos + VoidGUI::Vec2(4.0f, 3.0f), name_col, name, 0.95f);

                char bind_buf[32];
                snprintf(bind_buf, sizeof(bind_buf), "[ %s ]", mode);
                VoidGUI::Vec2 mode_sz = VoidGUI::Font::CalcTextSize(bind_buf, 0.85f);

                VoidGUI::Color mode_col = active ? VoidGUI::Color(192, 132, 252) : VoidGUI::Color(90, 96, 115);
                dl->AddText(row_pos + VoidGUI::Vec2(row_w - mode_sz.x - 2.0f, 4.0f), mode_col, bind_buf, 0.85f);

                gui.GetCurrentWindow().cursor.y += row_h;
            };

            draw_bind_row("Aimbot", "Hold", "[ M2 ]", aimbot_enabled);
            draw_bind_row("Silent Aim", "Always", "[ On ]", silent_aim);
            draw_bind_row("Triggerbot", "Hold", "[ M5 ]", triggerbot_enabled);
            draw_bind_row("Thirdperson", "Toggle", "[ V ]", true);
            draw_bind_row("Doubletap", "Toggle", "[ CAPS ]", true);

            VoidGUI::Vec2 div_pos = gui.GetCurrentWindow().cursor + VoidGUI::Vec2(0, 4.0f);
            float div_w = gui.GetCurrentWindow().content_width;
            dl->AddLine(div_pos, div_pos + VoidGUI::Vec2(div_w, 0), VoidGUI::Color(35, 40, 58, 200), 1.0f);
            gui.GetCurrentWindow().cursor.y += 10.0f;

            VoidGUI::Vec2 net_pos = gui.GetCurrentWindow().cursor;
            char net_buf[64];
            snprintf(net_buf, sizeof(net_buf), "PING: 14ms   |   LOSS: 0%%   |   FPS: %.0f", fps > 0.0f ? fps : 180.0f);
            dl->AddText(net_pos + VoidGUI::Vec2(4.0f, 2.0f), VoidGUI::Color(135, 142, 165), net_buf, 0.85f);

            gui.EndDraggableCard();
        }

        // Spotify player
        VoidGUI::SpotifyPlayer::Update(dt);
        if (spotify_open) {
            VoidGUI::SpotifyPlayer::Render(&gui, &spotify_pos, &spotify_open);
        }

        // Target info / spectator bar
        if (playerbar_open) {
            VoidGUI::PlayerBar::Render(&gui, &playerbar_pos, &playerbar_open);
        }

        // Main menu
        if (menu_open) {
            if (gui.Begin("ktna.wtf", VoidGUI::Vec2(334.0f, 55.0f), VoidGUI::Vec2(560.0f, 580.0f))) {

                gui.TabBar(tabs, 4, &selected_tab);
                gui.Spacing(4.0f);

                if (selected_tab == 0) {
                    gui.Columns(2);

                    gui.BeginCard("AIMBOT");
                    gui.Toggle("Enable Aimbot", &aimbot_enabled);
                    gui.Toggle("Silent Aim", &silent_aim);
                    gui.Combo("Target Hitbox", &hitbox_target, hitbox_options, 4);
                    gui.SliderFloat("Field of View", &aimbot_fov, 1.0f, 30.0f, "%.1f deg");
                    gui.SliderFloat("Max Distance", &aimbot_dist, 20.0f, 500.0f, "%.0fm");
                    gui.EndCard();

                    gui.BeginCard("TRIGGERBOT");
                    gui.Toggle("Enable Triggerbot", &triggerbot_enabled);
                    gui.SliderInt("Reaction Delay", &trigger_delay, 0, 150);
                    gui.EndCard();

                    gui.NextColumn();

                    gui.BeginCard("RECOIL & ACCURACY");
                    gui.Toggle("Recoil Control (RCS)", &rcs_enabled);
                    gui.SliderFloat("RCS Smoothing", &aimbot_smooth, 1.0f, 25.0f, "%.1f");
                    gui.SliderFloat("RCS Variance", &aimbot_jitter, 0.0f, 5.0f, "%.1f");
                    gui.EndCard();

                    gui.BeginCard("KEYBINDS");
                    gui.Keybind("Aimbot Key", &aimbot_key);
                    gui.Combo("Key Mode", &aimbot_mode, mode_options, 3);
                    gui.EndCard();

                    gui.EndColumns();

                } else if (selected_tab == 1) {
                    gui.Columns(2);

                    gui.BeginCard("PLAYER ESP");
                    gui.Toggle("Bounding Box", &esp_box);
                    gui.Toggle("Skeleton", &esp_skeleton);
                    gui.Toggle("Health Bar", &esp_health);
                    gui.Toggle("Armor Bar", &esp_armor);
                    gui.Toggle("Name Tags", &esp_names);
                    gui.Toggle("Snaplines", &esp_snaplines);
                    gui.EndCard();

                    gui.NextColumn();

                    gui.BeginCard("MODELS (CHAMS)");
                    gui.Toggle("Enable Chams", &chams_enabled);
                    gui.Combo("Material", &chams_material, chams_options, 4);
                    gui.ColorEdit("Visible Color", &chams_color);
                    gui.ColorEdit("Invisible Color", &chams_occluded);
                    gui.SliderFloat("Max Distance", &esp_distance, 50.0f, 1000.0f, "%.0fm");
                    gui.EndCard();

                    gui.BeginCard("WORLD ESP");
                    gui.Toggle("Weapon Drops", &esp_dropped_weapons);
                    gui.Toggle("Bomb / C4 Timer", &esp_c4_timer);
                    gui.EndCard();

                    gui.EndColumns();

                } else if (selected_tab == 2) {
                    gui.Columns(2);

                    gui.BeginCard("MOVEMENT");
                    gui.Toggle("Bunnyhop", &bhop_enabled);
                    gui.Toggle("Auto Strafe", &strafe_assist);
                    gui.Toggle("Edge Jump", &edge_jump);
                    gui.SliderFloat("Speed Hack", &speed_hack, 1.0f, 2.5f, "%.1fx");
                    gui.EndCard();

                    gui.BeginCard("APPEARANCE & HUD");
                    if (gui.Toggle("Liquid Glass Mode", &liquid_glass_mode)) {
                        gui.GetTheme().SetLiquidGlass(liquid_glass_mode);
                    }
                    gui.Toggle("Keybinds Window", &hud_open);
                    gui.Toggle("Music Player", &spotify_open);
                    gui.Toggle("Target Info Bar", &playerbar_open);
                    gui.EndCard();

                    gui.NextColumn();

                    gui.BeginCard("REMOVALS");
                    gui.Toggle("No Flash", &no_flash);
                    gui.Toggle("No Smoke", &no_smoke);
                    gui.SliderFloat("Viewmodel FOV", &fov_offset, -20.0f, 30.0f, "%.0f deg");
                    gui.EndCard();

                    gui.BeginCard("SECURITY");
                    gui.Toggle("OBS / Stream Proof", &esp_armor);
                    if (gui.Button("Clean Screenshots")) {}
                    gui.Spacing(4.0f);
                    gui.ProgressBar(1.0f, "ANTI-UNTRUSTED ACTIVE");
                    gui.EndCard();

                    gui.EndColumns();

                } else if (selected_tab == 3) {
                    gui.Columns(2);

                    gui.BeginCard("PRESETS");
                    gui.Combo("Config Slot", &config_slot, config_slots, 4);
                    gui.Spacing(2.0f);
                    if (gui.Button("Save Config")) {}
                    if (gui.Button("Load Config")) {}
                    if (gui.Button("Reset Defaults")) {}
                    gui.EndCard();

                    gui.NextColumn();

                    gui.BeginCard("CLIPBOARD");
                    if (gui.Button("Export to Clipboard")) {}
                    if (gui.Button("Import from Clipboard")) {}
                    gui.Spacing(4.0f);
                    gui.ProgressBar(1.0f, "CLOUD SYNC ACTIVE");
                    gui.EndCard();

                    gui.EndColumns();
                }

                gui.End();
            }
        }

        gui.Render();

        const float clear_color[4] = { 0.05f, 0.06f, 0.08f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);

        VoidGUI::D3D11_Render(gui.GetDrawList(), static_cast<float>(g_Width), static_cast<float>(g_Height));

        // F12 capture snapshot
        static bool last_f12 = false;
        bool f12_down = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
        if (f12_down && !last_f12) {
            SaveBackBufferToBMP(g_pd3dDevice, g_pd3dDeviceContext, g_pSwapChain, "screenshot.bmp");
        }
        last_f12 = f12_down;

        g_pSwapChain->Present(1, 0); // VSync enabled
    }

    VoidGUI::D3D11_Shutdown();
    CleanupDeviceD3D();
    DestroyWindow(hWnd);
    UnregisterClassA(wc.lpszClassName, wc.hInstance);

    return 0;
}
