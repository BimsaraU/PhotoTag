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

    void LoadSourceFolder();

private:
    ID3D11Device* m_pd3dDevice = nullptr;
    ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;

    AppConfig m_Config;

    // File navigation
    std::vector<std::string> m_ImageFiles;
    int m_CurrentImageIdx = -1;

    // Textures
    ID3D11ShaderResourceView* m_MainImageTexture = nullptr;

    int m_MainImageWidth = 0;
    int m_MainImageHeight = 0;

    // GDI+
    ULONG_PTR m_gdiplusToken;

    // Helpers
    void LoadImageTexture(const char* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
    void UnloadCurrentImages();
    void OpenFolderDialog(char* buffer, int maxLen);
};
