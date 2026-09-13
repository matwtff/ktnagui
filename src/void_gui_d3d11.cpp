#include "../include/void_gui_d3d11.hpp"
#include "../include/void_font.hpp"
#include <d3dcompiler.h>
#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace VoidGUI {

    static ID3D11Device*            g_pd3dDevice = nullptr;
    static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
    static ID3D11Buffer*            g_pVB = nullptr;
    static ID3D11Buffer*            g_pIB = nullptr;
    static ID3D11VertexShader*      g_pVertexShader = nullptr;
    static ID3D11InputLayout*       g_pInputLayout = nullptr;
    static ID3D11Buffer*            g_pConstantBuffer = nullptr;
    static ID3D11PixelShader*       g_pPixelShader = nullptr;
    static ID3D11SamplerState*      g_pFontSampler = nullptr;
    static ID3D11RasterizerState*   g_pRasterizerState = nullptr;
    static ID3D11BlendState*        g_pBlendState = nullptr;
    static ID3D11DepthStencilState* g_pDepthStencilState = nullptr;

    static int g_VertexBufferSize = 5000;
    static int g_IndexBufferSize  = 10000;

    struct CONSTANT_BUFFER {
        float mvp[4][4];
    };

    // In-memory HLSL runtime shader source
    static const char* s_shader_hlsl =
        "Texture2D u_Texture : register(t0);\n"
        "SamplerState u_Sampler : register(s0);\n"
        "cbuffer TransformBuffer : register(b0) {\n"
        "    float4x4 u_ProjMatrix;\n"
        "};\n"
        "struct VS_IN {\n"
        "    float2 pos   : POSITION;\n"
        "    float2 uv    : TEXCOORD0;\n"
        "    float4 col   : COLOR0;\n"
        "    float  flags : PADDING;\n"
        "};\n"
        "struct PS_IN {\n"
        "    float4 pos   : SV_POSITION;\n"
        "    float4 col   : COLOR0;\n"
        "    float2 uv    : TEXCOORD0;\n"
        "    float  flags : PADDING;\n"
        "};\n"
        "PS_IN VSMain(VS_IN input) {\n"
        "    PS_IN output;\n"
        "    output.pos = mul(u_ProjMatrix, float4(input.pos.xy, 0.0f, 1.0f));\n"
        "    output.col = input.col;\n"
        "    output.uv  = input.uv;\n"
        "    output.flags = input.flags;\n"
        "    return output;\n"
        "}\n"
        "float4 PSMain(PS_IN input) : SV_Target {\n"
        "    if (input.flags > 1.5f) {\n"
        "        return input.col * u_Texture.Sample(u_Sampler, input.uv);\n"
        "    }\n"
        "    return input.col;\n"
        "}\n";

    bool D3D11_CreateDeviceObjects() {
        if (!g_pd3dDevice) return false;

        if (g_pVB) D3D11_InvalidateDeviceObjects();

        // Compile Vertex Shader
        ID3DBlob* vsBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;
        HRESULT hr = D3DCompile(s_shader_hlsl, strlen(s_shader_hlsl), nullptr, nullptr, nullptr,
                                "VSMain", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) errorBlob->Release();
            return false;
        }

        hr = g_pd3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_pVertexShader);
        if (FAILED(hr)) {
            vsBlob->Release();
            return false;
        }

        // Custom 24-byte input layout (UD: differs from standard 20-byte ImDrawVert)
        D3D11_INPUT_ELEMENT_DESC local_layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "PADDING",  0, DXGI_FORMAT_R32_FLOAT,      0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = g_pd3dDevice->CreateInputLayout(local_layout, 4, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &g_pInputLayout);
        vsBlob->Release();
        if (FAILED(hr)) return false;

        // Compile Pixel Shader
        ID3DBlob* psBlob = nullptr;
        hr = D3DCompile(s_shader_hlsl, strlen(s_shader_hlsl), nullptr, nullptr, nullptr,
                                "PSMain", "ps_4_0", 0, 0, &psBlob, &errorBlob);
        if (FAILED(hr)) {
            if (errorBlob) errorBlob->Release();
            return false;
        }

        hr = g_pd3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pPixelShader);
        psBlob->Release();
        if (FAILED(hr)) return false;

        // Constant Buffer
        D3D11_BUFFER_DESC cbDesc = {};
        cbDesc.ByteWidth = sizeof(CONSTANT_BUFFER);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = g_pd3dDevice->CreateBuffer(&cbDesc, nullptr, &g_pConstantBuffer);
        if (FAILED(hr)) return false;

        // Blend State (Alpha Blending)
        D3D11_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = false;
        blendDesc.RenderTarget[0].BlendEnable = true;
        blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        g_pd3dDevice->CreateBlendState(&blendDesc, &g_pBlendState);

        // Rasterizer State (Scissor Test enabled, no culling)
        D3D11_RASTERIZER_DESC rastDesc = {};
        rastDesc.FillMode = D3D11_FILL_SOLID;
        rastDesc.CullMode = D3D11_CULL_NONE;
        rastDesc.ScissorEnable = true;
        rastDesc.DepthClipEnable = true;
        g_pd3dDevice->CreateRasterizerState(&rastDesc, &g_pRasterizerState);

        // Depth Stencil State (Disabled)
        D3D11_DEPTH_STENCIL_DESC depthDesc = {};
        depthDesc.DepthEnable = false;
        depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
        g_pd3dDevice->CreateDepthStencilState(&depthDesc, &g_pDepthStencilState);

        // Linear Sampler State for Textures
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        samplerDesc.MinLOD = 0;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        g_pd3dDevice->CreateSamplerState(&samplerDesc, &g_pFontSampler);

        Font::InitializeAtlas(g_pd3dDevice);
        return true;
    }

    void D3D11_InvalidateDeviceObjects() {
        Font::ShutdownAtlas();
        if (g_pVB) { g_pVB->Release(); g_pVB = nullptr; }
        if (g_pIB) { g_pIB->Release(); g_pIB = nullptr; }
        if (g_pVertexShader) { g_pVertexShader->Release(); g_pVertexShader = nullptr; }
        if (g_pInputLayout) { g_pInputLayout->Release(); g_pInputLayout = nullptr; }
        if (g_pConstantBuffer) { g_pConstantBuffer->Release(); g_pConstantBuffer = nullptr; }
        if (g_pPixelShader) { g_pPixelShader->Release(); g_pPixelShader = nullptr; }
        if (g_pFontSampler) { g_pFontSampler->Release(); g_pFontSampler = nullptr; }
        if (g_pRasterizerState) { g_pRasterizerState->Release(); g_pRasterizerState = nullptr; }
        if (g_pBlendState) { g_pBlendState->Release(); g_pBlendState = nullptr; }
        if (g_pDepthStencilState) { g_pDepthStencilState->Release(); g_pDepthStencilState = nullptr; }
    }

    bool D3D11_Init(ID3D11Device* device, ID3D11DeviceContext* context) {
        g_pd3dDevice = device;
        g_pd3dDeviceContext = context;
        return D3D11_CreateDeviceObjects();
    }

    void D3D11_Shutdown() {
        D3D11_InvalidateDeviceObjects();
        g_pd3dDevice = nullptr;
        g_pd3dDeviceContext = nullptr;
    }

    void D3D11_NewFrame() {
        if (!g_pVB) D3D11_CreateDeviceObjects();
    }

    // D3D11 pipeline state snapshot (14-point restoration prevents host state corruption)
    struct BACKUP_DX11_STATE {
        UINT                        ScissorRectsCount, ViewportsCount;
        D3D11_RECT                  ScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        D3D11_VIEWPORT              Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        ID3D11RasterizerState*      RS;
        ID3D11BlendState*           BlendState;
        FLOAT                       BlendFactor[4];
        UINT                        SampleMask;
        UINT                        StencilRef;
        ID3D11DepthStencilState*    DepthStencilState;
        ID3D11ShaderResourceView*   PSShaderResource;
        ID3D11SamplerState*         PSSampler;
        ID3D11PixelShader*          PS;
        ID3D11VertexShader*         VS;
        ID3D11GeometryShader*       GS;
        UINT                        PSInstancesCount, VSInstancesCount, GSInstancesCount;
        ID3D11ClassInstance         *PSInstances[256], *VSInstances[256], *GSInstances[256];
        D3D11_PRIMITIVE_TOPOLOGY    Topology;
        ID3D11Buffer*               IndexBuffer;
        ID3D11Buffer*               VertexBuffer;
        ID3D11Buffer*               VSConstantBuffer;
        UINT                        IndexBufferOffset, VertexBufferStride, VertexBufferOffset;
        DXGI_FORMAT                 IndexBufferFormat;
        ID3D11InputLayout*          InputLayout;
    };

    void D3D11_Render(DrawList* draw_list, float screen_width, float screen_height) {
        if (!draw_list || draw_list->vertices.empty() || draw_list->indices.empty()) return;

        // Allocate / grow dynamic vertex buffer
        if (!g_pVB || g_VertexBufferSize < static_cast<int>(draw_list->vertices.size())) {
            if (g_pVB) { g_pVB->Release(); g_pVB = nullptr; }
            g_VertexBufferSize = static_cast<int>(draw_list->vertices.size()) + 5000;
            D3D11_BUFFER_DESC desc = {};
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.ByteWidth = g_VertexBufferSize * sizeof(Vertex);
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if (FAILED(g_pd3dDevice->CreateBuffer(&desc, nullptr, &g_pVB))) return;
        }

        // Allocate / grow dynamic index buffer
        if (!g_pIB || g_IndexBufferSize < static_cast<int>(draw_list->indices.size())) {
            if (g_pIB) { g_pIB->Release(); g_pIB = nullptr; }
            g_IndexBufferSize = static_cast<int>(draw_list->indices.size()) + 10000;
            D3D11_BUFFER_DESC desc = {};
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.ByteWidth = g_IndexBufferSize * sizeof(uint32_t);
            desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if (FAILED(g_pd3dDevice->CreateBuffer(&desc, nullptr, &g_pIB))) return;
        }

        // Copy vertex & index data
        D3D11_MAPPED_SUBRESOURCE vtx_resource, idx_resource;
        if (SUCCEEDED(g_pd3dDeviceContext->Map(g_pVB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vtx_resource))) {
            memcpy(vtx_resource.pData, draw_list->vertices.data(), draw_list->vertices.size() * sizeof(Vertex));
            g_pd3dDeviceContext->Unmap(g_pVB, 0);
        }
        if (SUCCEEDED(g_pd3dDeviceContext->Map(g_pIB, 0, D3D11_MAP_WRITE_DISCARD, 0, &idx_resource))) {
            memcpy(idx_resource.pData, draw_list->indices.data(), draw_list->indices.size() * sizeof(uint32_t));
            g_pd3dDeviceContext->Unmap(g_pIB, 0);
        }

        // Setup orthographic projection matrix
        D3D11_MAPPED_SUBRESOURCE mapped_res;
        if (SUCCEEDED(g_pd3dDeviceContext->Map(g_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res))) {
            CONSTANT_BUFFER* cb = reinterpret_cast<CONSTANT_BUFFER*>(mapped_res.pData);
            float L = 0.0f;
            float R = screen_width;
            float T = 0.0f;
            float B = screen_height;
            float mvp[4][4] = {
                { 2.0f / (R - L),   0.0f,            0.0f,  0.0f },
                { 0.0f,            2.0f / (T - B),   0.0f,  0.0f },
                { 0.0f,            0.0f,            0.5f,  0.0f },
                { (R + L) / (L - R), (T + B) / (B - T), 0.5f,  1.0f },
            };
            memcpy(&cb->mvp, mvp, sizeof(mvp));
            g_pd3dDeviceContext->Unmap(g_pConstantBuffer, 0);
        }

        // Snapshot pipeline state
        BACKUP_DX11_STATE old = {};
        old.ScissorRectsCount = old.ViewportsCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        g_pd3dDeviceContext->RSGetScissorRects(&old.ScissorRectsCount, old.ScissorRects);
        g_pd3dDeviceContext->RSGetViewports(&old.ViewportsCount, old.Viewports);
        g_pd3dDeviceContext->RSGetState(&old.RS);
        g_pd3dDeviceContext->OMGetBlendState(&old.BlendState, old.BlendFactor, &old.SampleMask);
        g_pd3dDeviceContext->OMGetDepthStencilState(&old.DepthStencilState, &old.StencilRef);
        old.PSInstancesCount = old.VSInstancesCount = old.GSInstancesCount = 256;
        g_pd3dDeviceContext->PSGetShader(&old.PS, old.PSInstances, &old.PSInstancesCount);
        g_pd3dDeviceContext->VSGetShader(&old.VS, old.VSInstances, &old.VSInstancesCount);
        g_pd3dDeviceContext->VSGetConstantBuffers(0, 1, &old.VSConstantBuffer);
        g_pd3dDeviceContext->GSGetShader(&old.GS, old.GSInstances, &old.GSInstancesCount);
        g_pd3dDeviceContext->IAGetPrimitiveTopology(&old.Topology);
        g_pd3dDeviceContext->IAGetIndexBuffer(&old.IndexBuffer, &old.IndexBufferFormat, &old.IndexBufferOffset);
        g_pd3dDeviceContext->IAGetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset);
        g_pd3dDeviceContext->IAGetInputLayout(&old.InputLayout);

        // Bind engine pipeline state
        D3D11_VIEWPORT vp = {};
        vp.Width = screen_width;
        vp.Height = screen_height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        g_pd3dDeviceContext->RSSetViewports(1, &vp);

        UINT stride = sizeof(Vertex); // 24 bytes
        UINT offset = 0;
        g_pd3dDeviceContext->IASetInputLayout(g_pInputLayout);
        g_pd3dDeviceContext->IASetVertexBuffers(0, 1, &g_pVB, &stride, &offset);
        g_pd3dDeviceContext->IASetIndexBuffer(g_pIB, DXGI_FORMAT_R32_UINT, 0);
        g_pd3dDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_pd3dDeviceContext->VSSetShader(g_pVertexShader, nullptr, 0);
        g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, &g_pConstantBuffer);
        g_pd3dDeviceContext->PSSetShader(g_pPixelShader, nullptr, 0);
        g_pd3dDeviceContext->GSSetShader(nullptr, nullptr, 0);

        const float blend_factor[4] = { 0.f, 0.f, 0.f, 0.f };
        g_pd3dDeviceContext->OMSetBlendState(g_pBlendState, blend_factor, 0xffffffff);
        g_pd3dDeviceContext->OMSetDepthStencilState(g_pDepthStencilState, 0);
        g_pd3dDeviceContext->RSSetState(g_pRasterizerState);

        for (const auto& cmd : draw_list->commands) {
            if (cmd.index_count == 0) continue;

            if (cmd.texture_id) {
                ID3D11ShaderResourceView* srv = reinterpret_cast<ID3D11ShaderResourceView*>(cmd.texture_id);
                g_pd3dDeviceContext->PSSetShaderResources(0, 1, &srv);
                if (g_pFontSampler) {
                    g_pd3dDeviceContext->PSSetSamplers(0, 1, &g_pFontSampler);
                }
            }

            D3D11_RECT r = {
                static_cast<LONG>(cmd.clip_rect.min.x),
                static_cast<LONG>(cmd.clip_rect.min.y),
                static_cast<LONG>(cmd.clip_rect.max.x),
                static_cast<LONG>(cmd.clip_rect.max.y)
            };
            g_pd3dDeviceContext->RSSetScissorRects(1, &r);
            g_pd3dDeviceContext->DrawIndexed(cmd.index_count, cmd.index_offset, 0);
        }

        // Restore pipeline state
        g_pd3dDeviceContext->RSSetScissorRects(old.ScissorRectsCount, old.ScissorRects);
        g_pd3dDeviceContext->RSSetViewports(old.ViewportsCount, old.Viewports);
        g_pd3dDeviceContext->RSSetState(old.RS); if (old.RS) old.RS->Release();
        g_pd3dDeviceContext->OMSetBlendState(old.BlendState, old.BlendFactor, old.SampleMask); if (old.BlendState) old.BlendState->Release();
        g_pd3dDeviceContext->OMSetDepthStencilState(old.DepthStencilState, old.StencilRef); if (old.DepthStencilState) old.DepthStencilState->Release();
        g_pd3dDeviceContext->PSSetShader(old.PS, old.PSInstances, old.PSInstancesCount); if (old.PS) old.PS->Release();
        for (UINT i = 0; i < old.PSInstancesCount; i++) if (old.PSInstances[i]) old.PSInstances[i]->Release();
        g_pd3dDeviceContext->VSSetShader(old.VS, old.VSInstances, old.VSInstancesCount); if (old.VS) old.VS->Release();
        g_pd3dDeviceContext->VSSetConstantBuffers(0, 1, &old.VSConstantBuffer); if (old.VSConstantBuffer) old.VSConstantBuffer->Release();
        for (UINT i = 0; i < old.VSInstancesCount; i++) if (old.VSInstances[i]) old.VSInstances[i]->Release();
        g_pd3dDeviceContext->GSSetShader(old.GS, old.GSInstances, old.GSInstancesCount); if (old.GS) old.GS->Release();
        for (UINT i = 0; i < old.GSInstancesCount; i++) if (old.GSInstances[i]) old.GSInstances[i]->Release();
        g_pd3dDeviceContext->IASetPrimitiveTopology(old.Topology);
        g_pd3dDeviceContext->IASetIndexBuffer(old.IndexBuffer, old.IndexBufferFormat, old.IndexBufferOffset); if (old.IndexBuffer) old.IndexBuffer->Release();
        g_pd3dDeviceContext->IASetVertexBuffers(0, 1, &old.VertexBuffer, &old.VertexBufferStride, &old.VertexBufferOffset); if (old.VertexBuffer) old.VertexBuffer->Release();
        g_pd3dDeviceContext->IASetInputLayout(old.InputLayout); if (old.InputLayout) old.InputLayout->Release();
    }

    ID3D11Device* D3D11_GetDevice() { return g_pd3dDevice; }
    ID3D11DeviceContext* D3D11_GetContext() { return g_pd3dDeviceContext; }

} // namespace VoidGUI
