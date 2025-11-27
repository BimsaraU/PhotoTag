#pragma once
#include "../imgui/imgui.h"
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#include <string>
#include <vector>
#include <windows.h>
#include <gdiplus.h>

struct AppConfig {
    char SourceFolder[260] = "";
    char DestFolder[260] = "";
};

class PhotoApp {
public:
    PhotoApp(ID3D11Device* device, ID3D11DeviceContext* context);
    ~PhotoApp();

    void Update();
    void RenderUI();

private:
    ID3D11Device* m_pd3dDevice = nullptr;
    ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;

    AppConfig m_Config;

    // GDI+
    ULONG_PTR m_gdiplusToken;
};
