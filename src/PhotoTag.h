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
    char PortraitTagPath[260] = "";
    char LandscapeTagPath[260] = "";
};

struct TagSettings {
    float TagScale = 1.0f; // Replaced WidthPct/HeightPct with Scale
    float PosXPct = 0.5f;
    float PosYPct = 0.5f;
    bool  SnapGrid = false;
    int   GridLines = 6;
    float SafeMargin = 0.0f;
    bool  EnableBlur = false;
    int   BlurDownscale = 16; // Adjustable blur amount
    float TagOpacity = 1.0f;
    float BackgroundOpacity = 1.0f;
};

class PhotoApp {
public:
    PhotoApp(ID3D11Device* device, ID3D11DeviceContext* context);
    ~PhotoApp();

    void Update();
    void RenderUI();

    void LoadSourceFolder();
    void ProcessAllImages();

private:
    ID3D11Device* m_pd3dDevice = nullptr;
    ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;

    AppConfig m_Config;
    TagSettings m_TagSettings;

    // File navigation
    std::vector<std::string> m_ImageFiles;
    int m_CurrentImageIdx = -1;

    // Textures
    ID3D11ShaderResourceView* m_MainImageTexture = nullptr;
    ID3D11ShaderResourceView* m_BlurredImageTexture = nullptr;
    ID3D11ShaderResourceView* m_PortraitTagTexture = nullptr;
    ID3D11ShaderResourceView* m_LandscapeTagTexture = nullptr;
    
    int m_MainImageWidth = 0;
    int m_MainImageHeight = 0;
    int m_PortraitTagWidth = 0;
    int m_PortraitTagHeight = 0;
    int m_LandscapeTagWidth = 0;
    int m_LandscapeTagHeight = 0;

    // GDI+
    ULONG_PTR m_gdiplusToken;

    // Helpers
    void LoadImageTexture(const char* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height, ID3D11ShaderResourceView** out_blur_srv = nullptr, int blur_downscale = 16);
    void UnloadCurrentImages();
    void OpenFolderDialog(char* buffer, int maxLen);
    void OpenFileDialog(char* buffer, int maxLen);
    
    // Logic
    void RenderPreview();
    void DrawGrid(ImVec2 imageStart, ImVec2 imageSize);
    
    bool m_FullscreenPreview = false;
    void GenerateGaussianBlur(Gdiplus::Bitmap* bmp, int radius);
};
