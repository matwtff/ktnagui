#pragma once

#include "void_gui.hpp"
#include <d3d11.h>

namespace VoidGUI {

    bool D3D11_Init(ID3D11Device* device, ID3D11DeviceContext* context);
    void D3D11_Shutdown();
    void D3D11_NewFrame();
    void D3D11_Render(DrawList* draw_list, float screen_width, float screen_height);

    // Dynamic viewport / display dimension helper
    void D3D11_InvalidateDeviceObjects();
    bool D3D11_CreateDeviceObjects();

    ID3D11Device* D3D11_GetDevice();
    ID3D11DeviceContext* D3D11_GetContext();

} // namespace VoidGUI
