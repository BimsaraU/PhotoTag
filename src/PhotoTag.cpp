#include "PhotoTag.h"
#define STB_IMAGE_IMPLEMENTATION // If we had it, but we use GDI+
#include <tchar.h>
#include <shobjidl.h> 
#include <algorithm>
#include <cmath>
#include <vector>

#pragma comment (lib,"Gdiplus.lib")

// Helper to convert wstring to string
std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

PhotoApp::PhotoApp(ID3D11Device* device, ID3D11DeviceContext* context) 
    : m_pd3dDevice(device), m_pd3dDeviceContext(context)
{
    m_IsProcessing = false;
    m_ProcessingProgress = 0.0f;
    m_UnsnappedPosX = 0.5f;
    m_UnsnappedPosY = 0.5f;

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

    ImGuiStyle& style = ImGui::GetStyle();

    // Topaz-inspired dark theme
    style.WindowRounding    = 4.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 3.0f;
    style.GrabRounding      = 3.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding       = 3.0f;
    style.WindowPadding     = ImVec2(10, 10);
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(8, 6);
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 8.0f;
    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 0.0f;

    ImVec4* c = style.Colors;
    // #1a252d based backgrounds
    c[ImGuiCol_WindowBg]             = ImVec4(0.102f, 0.145f, 0.176f, 1.00f);
    c[ImGuiCol_ChildBg]              = ImVec4(0.110f, 0.155f, 0.190f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.110f, 0.155f, 0.190f, 0.96f);
    c[ImGuiCol_Border]               = ImVec4(0.16f, 0.22f, 0.27f, 1.00f);
    c[ImGuiCol_Text]                 = ImVec4(0.86f, 0.90f, 0.94f, 1.00f);
    c[ImGuiCol_TextDisabled]         = ImVec4(0.36f, 0.44f, 0.52f, 1.00f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.13f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.16f, 0.22f, 0.27f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.19f, 0.26f, 0.32f, 1.00f);
    c[ImGuiCol_TitleBg]              = ImVec4(0.08f, 0.11f, 0.14f, 1.00f);
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.10f, 0.14f, 0.17f, 1.00f);
    // #11c2ed based interactive elements
    c[ImGuiCol_Button]               = ImVec4(0.13f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.067f, 0.761f, 0.929f, 0.65f);
    c[ImGuiCol_ButtonActive]         = ImVec4(0.067f, 0.761f, 0.929f, 0.85f);
    c[ImGuiCol_Header]               = ImVec4(0.13f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.067f, 0.761f, 0.929f, 0.45f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.067f, 0.761f, 0.929f, 0.65f);
    c[ImGuiCol_Separator]            = ImVec4(0.16f, 0.22f, 0.27f, 1.00f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.067f, 0.761f, 0.929f, 1.00f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.20f, 0.84f, 0.97f, 1.00f);
    c[ImGuiCol_CheckMark]            = ImVec4(0.067f, 0.761f, 0.929f, 1.00f);
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.08f, 0.11f, 0.14f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.16f, 0.22f, 0.27f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.22f, 0.30f, 0.36f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.067f, 0.761f, 0.929f, 1.00f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.067f, 0.761f, 0.929f, 1.00f);
    c[ImGuiCol_Tab]                  = ImVec4(0.11f, 0.16f, 0.19f, 1.00f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.067f, 0.761f, 0.929f, 0.65f);
    c[ImGuiCol_TabActive]            = ImVec4(0.067f, 0.761f, 0.929f, 0.45f);
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.067f, 0.761f, 0.929f, 0.25f);
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.067f, 0.761f, 0.929f, 0.60f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.067f, 0.761f, 0.929f, 0.90f);

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f;
}

PhotoApp::~PhotoApp() {
    m_IsProcessing = false; // Signal thread to stop
    if (m_WorkerThread.joinable()) {
        m_WorkerThread.join();
    }
    UnloadCurrentImages();
    if (m_PortraitTagTexture) m_PortraitTagTexture->Release();
    if (m_LandscapeTagTexture) m_LandscapeTagTexture->Release();
    
    Gdiplus::GdiplusShutdown(m_gdiplusToken);
}

// Separable Gaussian Blur (approximated by 3 passes of Box Blur for speed)
// Using standard deviation approx. radius/2
void PhotoApp::GenerateGaussianBlur(Gdiplus::Bitmap* bmp, int radius) {
    if (radius < 1) return;

    int w = bmp->GetWidth();
    int h = bmp->GetHeight();
    
    Gdiplus::Rect rect(0, 0, w, h);
    Gdiplus::BitmapData data;
    if (bmp->LockBits(&rect, Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &data) != Gdiplus::Ok) return;

    // We only care about RGB, A is kept 255 usually, or blur A too? Let's assume opaque for now or blur everything. 
    // Format is BGRA (Order in byte array: B, G, R, A)
    int stride = data.Stride;
    unsigned char* pixels = (unsigned char*)data.Scan0;
    
    std::vector<unsigned char> buffer(h * stride);
    memcpy(buffer.data(), pixels, h * stride); // Copy source

    // Helper: Horizontal Pass
    auto HorizontalBlur = [&](std::vector<unsigned char>& src, std::vector<unsigned char>& dst, int w, int h, int r) {
        float iarr = 1.0f / (r + r + 1);
        for (int i = 0; i < h; i++) {
            int ti = i * stride; // Target Index Row Start
            int li = ti;
            int ri = ti + r * 4;
            
            // Initial sum
            int fv_b = src[ti], fv_g = src[ti+1], fv_r = src[ti+2];
            int lv_b = src[ti + (w-1)*4], lv_g = src[ti + (w-1)*4 + 1], lv_r = src[ti + (w-1)*4 + 2];
            
            int val_b = (r+1)*fv_b, val_g = (r+1)*fv_g, val_r = (r+1)*fv_r;
            
            for(int j=0; j<r; j++) { 
                val_b += src[ti + j*4]; val_g += src[ti + j*4 + 1]; val_r += src[ti + j*4 + 2]; 
            }
            
            for(int j=0; j<=r; j++) {
                int p = ti + (j+r)*4;
                if (j+r >= w) p = ti + (w-1)*4; // Clamp
                val_b += src[p]; val_g += src[p+1]; val_r += src[p+2];
                
                dst[ti + j*4]     = val_b * iarr;
                dst[ti + j*4 + 1] = val_g * iarr;
                dst[ti + j*4 + 2] = val_r * iarr;
                dst[ti + j*4 + 3] = 255;
                
                val_b -= fv_b; val_g -= fv_g; val_r -= fv_r;
            }
            // Middle
            for(int j=r+1; j<w-r; j++) {
                 int p1 = ti + (j+r)*4;
                 int p2 = ti + (j-r-1)*4;
                 
                 val_b += src[p1] - src[p2];
                 val_g += src[p1+1] - src[p2+1];
                 val_r += src[p1+2] - src[p2+2];
                 
                 dst[ti + j*4]     = val_b * iarr;
                 dst[ti + j*4 + 1] = val_g * iarr;
                 dst[ti + j*4 + 2] = val_r * iarr;
            }
            // End
            for(int j=w-r; j<w; j++) {
                 int p = ti + (j-r-1)*4;
                 val_b += lv_b - src[p];
                 val_g += lv_g - src[p+1];
                 val_r += lv_r - src[p+2];
                 
                 dst[ti + j*4]     = val_b * iarr;
                 dst[ti + j*4 + 1] = val_g * iarr;
                 dst[ti + j*4 + 2] = val_r * iarr;
            }
        }
    };
    
    // Helper: Vertical Pass (Need to jump strides)
    auto VerticalBlur = [&](std::vector<unsigned char>& src, std::vector<unsigned char>& dst, int w, int h, int r) {
        float iarr = 1.0f / (r + r + 1);
        for (int i = 0; i < w; i++) {
            int ti = i * 4; // Target Column Start
            
            int fv_b = src[ti], fv_g = src[ti+1], fv_r = src[ti+2];
            int lv_b = src[ti + (h-1)*stride], lv_g = src[ti + (h-1)*stride + 1], lv_r = src[ti + (h-1)*stride + 2];
            
            int val_b = (r+1)*fv_b, val_g = (r+1)*fv_g, val_r = (r+1)*fv_r;
            
            for(int j=0; j<r; j++) { 
                val_b += src[ti + j*stride]; val_g += src[ti + j*stride + 1]; val_r += src[ti + j*stride + 2]; 
            }
            
            for(int j=0; j<=r; j++) {
                int p = ti + (j+r)*stride;
                if (j+r >= h) p = ti + (h-1)*stride;
                val_b += src[p]; val_g += src[p+1]; val_r += src[p+2];
                
                dst[ti + j*stride]     = val_b * iarr;
                dst[ti + j*stride + 1] = val_g * iarr;
                dst[ti + j*stride + 2] = val_r * iarr;
                dst[ti + j*stride + 3] = 255;
                
                val_b -= fv_b; val_g -= fv_g; val_r -= fv_r;
            }
            
             for(int j=r+1; j<h-r; j++) {
                 int p1 = ti + (j+r)*stride;
                 int p2 = ti + (j-r-1)*stride;
                 
                 val_b += src[p1] - src[p2];
                 val_g += src[p1+1] - src[p2+1];
                 val_r += src[p1+2] - src[p2+2];
                 
                 dst[ti + j*stride]     = val_b * iarr;
                 dst[ti + j*stride + 1] = val_g * iarr;
                 dst[ti + j*stride + 2] = val_r * iarr;
            }
            
             for(int j=h-r; j<h; j++) {
                 int p = ti + (j-r-1)*stride;
                 val_b += lv_b - src[p];
                 val_g += lv_g - src[p+1];
                 val_r += lv_r - src[p+2];
                 
                 dst[ti + j*stride]     = val_b * iarr;
                 dst[ti + j*stride + 1] = val_g * iarr;
                 dst[ti + j*stride + 2] = val_r * iarr;
            }
        }
    };

    // 3 passes of box blur approximates Gaussian
    std::vector<unsigned char> temp(h * stride);
    
    // Pass 1
    HorizontalBlur(buffer, temp, w, h, radius);
    VerticalBlur(temp, buffer, w, h, radius);
    // Pass 2
    HorizontalBlur(buffer, temp, w, h, radius);
    VerticalBlur(temp, buffer, w, h, radius);
    // Pass 3 (Optional, skip for perf if needed, but 2 is minimal)
    
    // Copy back
    memcpy(pixels, buffer.data(), h * stride);
    
    bmp->UnlockBits(&data);
}

void PhotoApp::UnloadCurrentImages() {
    if (m_MainImageTexture) { m_MainImageTexture->Release(); m_MainImageTexture = nullptr; }
    if (m_BlurredImageTexture) { m_BlurredImageTexture->Release(); m_BlurredImageTexture = nullptr; }
}

// Simple GDI+ to DX11 loader
void PhotoApp::LoadImageTexture(const char* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height, ID3D11ShaderResourceView** out_blur_srv, int blur_downscale) {
    if (!filename || !*filename) return;

    std::wstring wFilename = Utf8ToWide(filename);
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(wFilename.c_str());
    
    if (bitmap->GetLastStatus() != Gdiplus::Ok) {
        delete bitmap;
        return; 
    }

    *out_width = bitmap->GetWidth();
    *out_height = bitmap->GetHeight();

    // PROXY LOGIC: Downscale if too big to save RAM/VRAM
    int originalW = *out_width;
    int originalH = *out_height;
    const int MAX_PROXY_DIM = 1920; 

    Gdiplus::Bitmap* bitampToUpload = bitmap;
    bool usingProxy = false;

    if (originalW > MAX_PROXY_DIM || originalH > MAX_PROXY_DIM) {
        float ratio = (float)originalW / (float)originalH;
        int newW = originalW;
        int newH = originalH;
        if (newW > MAX_PROXY_DIM) {
            newW = MAX_PROXY_DIM;
            newH = (int)(newW / ratio);
        }
        if (newH > MAX_PROXY_DIM) {
            newH = MAX_PROXY_DIM;
            newW = (int)(newH * ratio);
        }

        Gdiplus::Bitmap* proxyBmp = new Gdiplus::Bitmap(newW, newH, PixelFormat32bppARGB);
        Gdiplus::Graphics* gr = Gdiplus::Graphics::FromImage(proxyBmp);
        gr->SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
        gr->DrawImage(bitmap, 0, 0, newW, newH);
        delete gr;

        bitampToUpload = proxyBmp;
        usingProxy = true;
    }

    // Helper lambda to upload bitmap to DX11
    auto UploadBitmap = [&](Gdiplus::Bitmap* bmp, ID3D11ShaderResourceView** target_srv) {
        Gdiplus::BitmapData data;
        Gdiplus::Rect rect(0, 0, bmp->GetWidth(), bmp->GetHeight());
        
        // Lock bits in Format32bppArgb
        bmp->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data);

        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = bmp->GetWidth();
        desc.Height = bmp->GetHeight();
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // GDI+ uses BGRA
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA subResource;
        subResource.pSysMem = data.Scan0;
        subResource.SysMemPitch = data.Stride;
        subResource.SysMemSlicePitch = 0;

        ID3D11Texture2D* pTexture = NULL;
        if (SUCCEEDED(m_pd3dDevice->CreateTexture2D(&desc, &subResource, &pTexture))) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            ZeroMemory(&srvDesc, sizeof(srvDesc));
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
            srvDesc.Texture2D.MostDetailedMip = 0;
            m_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, target_srv);
            pTexture->Release();
        }
        bmp->UnlockBits(&data);
    };

    // Release the full-resolution original as early as possible to free memory.
    // The proxy (if created) is all we need from here on.
    if (usingProxy) {
        delete bitmap;
        bitmap = nullptr;
    }

    UploadBitmap(bitampToUpload, out_srv);

    if (out_blur_srv) {
        int w = bitampToUpload->GetWidth();
        int h = bitampToUpload->GetHeight();
        
        Gdiplus::Bitmap* blurBmp = bitampToUpload->Clone(0, 0, w, h, PixelFormat32bppARGB);
        GenerateGaussianBlur(blurBmp, blur_downscale);
        UploadBitmap(blurBmp, out_blur_srv);
        delete blurBmp;
    }

    if (usingProxy) delete bitampToUpload;
    if (bitmap) delete bitmap;
}

void PhotoApp::LoadSourceFolder() {
    m_ImageFiles.clear();
    std::string search_path = std::string(m_Config.SourceFolder) + "\\*.*";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = ::FindFirstFileA(search_path.c_str(), &fd);
    if(hFind != INVALID_HANDLE_VALUE) {
        do {
            if(!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string name = fd.cFileName;
                std::string ext = name.substr(name.find_last_of(".") + 1);
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "bmp") {
                    m_ImageFiles.push_back(std::string(m_Config.SourceFolder) + "\\" + name);
                }
            }
        } while(::FindNextFileA(hFind, &fd));
        ::FindClose(hFind);
    }

    if (!m_ImageFiles.empty()) {
        m_CurrentImageIdx = 0;
        UnloadCurrentImages();
        LoadImageTexture(m_ImageFiles[0].c_str(), &m_MainImageTexture, &m_MainImageWidth, &m_MainImageHeight, &m_BlurredImageTexture, m_TagSettings.BlurDownscale);
    }
}

void PhotoApp::Update() {
   // Logic updates map go here
}

static void PathLabel(const char* text) {
    const char* display = (text && text[0]) ? text : "(none)";
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.62f, 1.0f));
    ImGui::TextWrapped("%s", display);
    ImGui::PopStyleColor();
}

void PhotoApp::RenderUI() {
    // Single window mode
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    bool isFullscreen = m_FullscreenPreview;

    ImGui::Begin("Photo Tagger", NULL, window_flags);
    ImGui::PopStyleVar(2);

    bool processing = m_IsProcessing.load();

    // Left Panel (Hidden if Fullscreen)
    if (!isFullscreen) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
        ImGui::BeginChild("LeftPanel", ImVec2(340, 0), true);
        ImGui::PopStyleVar();

        ImGui::TextColored(ImVec4(0.067f, 0.761f, 0.929f, 1.0f), "FOLDERS");
        ImGui::Spacing();

        if (ImGui::Button("Source Folder", ImVec2(-1, 0))) OpenFolderDialog(m_Config.SourceFolder, 260);
        PathLabel(m_Config.SourceFolder);
        ImGui::Spacing();

        if (ImGui::Button("Output Folder", ImVec2(-1, 0))) OpenFolderDialog(m_Config.DestFolder, 260);
        PathLabel(m_Config.DestFolder);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.067f, 0.761f, 0.929f, 1.0f), "WATERMARKS");
        ImGui::Spacing();

        if (ImGui::Button("Portrait Tag", ImVec2(-1, 0))) {
            OpenFileDialog(m_Config.PortraitTagPath, 260);
            if (m_PortraitTagTexture) m_PortraitTagTexture->Release();
            m_PortraitTagTexture = nullptr;
            LoadImageTexture(m_Config.PortraitTagPath, &m_PortraitTagTexture, &m_PortraitTagWidth, &m_PortraitTagHeight);
        }
        PathLabel(m_Config.PortraitTagPath);
        ImGui::Spacing();

        if (ImGui::Button("Landscape Tag", ImVec2(-1, 0))) {
            OpenFileDialog(m_Config.LandscapeTagPath, 260);
            if (m_LandscapeTagTexture) m_LandscapeTagTexture->Release();
            m_LandscapeTagTexture = nullptr;
            LoadImageTexture(m_Config.LandscapeTagPath, &m_LandscapeTagTexture, &m_LandscapeTagWidth, &m_LandscapeTagHeight);
        }
        PathLabel(m_Config.LandscapeTagPath);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.067f, 0.761f, 0.929f, 1.0f), "IMAGES");
        ImGui::Spacing();

        if (ImGui::Button("Load Images", ImVec2(-1, 0))) {
            LoadSourceFolder();
        }
        ImGui::TextColored(ImVec4(0.55f, 0.58f, 0.62f, 1.0f), "%d images loaded", (int)m_ImageFiles.size());

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.067f, 0.761f, 0.929f, 1.0f), "TAG SETTINGS");
        ImGui::Spacing();

        ImGui::DragFloat("Scale", &m_TagSettings.TagScale, 0.005f, 0.01f, 1.0f, "%.2f");
        if (m_TagSettings.TagScale < 0.01f) m_TagSettings.TagScale = 0.01f;
        if (m_TagSettings.TagScale > 1.0f) m_TagSettings.TagScale = 1.0f;

        ImGui::DragFloat("X", &m_TagSettings.PosXPct, 0.005f, 0.0f, 1.0f, "%.2f");
        if (m_TagSettings.PosXPct < 0.0f) m_TagSettings.PosXPct = 0.0f;
        if (m_TagSettings.PosXPct > 1.0f) m_TagSettings.PosXPct = 1.0f;

        float yDisplay = 1.0f - m_TagSettings.PosYPct;
        if (ImGui::DragFloat("Y", &yDisplay, 0.005f, 0.0f, 1.0f, "%.2f")) {
            m_TagSettings.PosYPct = 1.0f - yDisplay;
        }
        if (m_TagSettings.PosYPct < 0.0f) m_TagSettings.PosYPct = 0.0f;
        if (m_TagSettings.PosYPct > 1.0f) m_TagSettings.PosYPct = 1.0f;

        ImGui::DragFloat("Tag Opacity", &m_TagSettings.TagOpacity, 0.005f, 0.0f, 1.0f, "%.2f");
        if (m_TagSettings.TagOpacity < 0.0f) m_TagSettings.TagOpacity = 0.0f;
        if (m_TagSettings.TagOpacity > 1.0f) m_TagSettings.TagOpacity = 1.0f;

        ImGui::Spacing();
        ImGui::Checkbox("Snap Grid", &m_TagSettings.SnapGrid);
        if (m_TagSettings.SnapGrid) {
            ImGui::DragInt("Grid Lines", &m_TagSettings.GridLines, 0.2f, 2, 20);
            ImGui::DragFloat("Safe Margin", &m_TagSettings.SafeMargin, 0.005f, 0.0f, 0.5f, "%.2f");
        }

        ImGui::Spacing();
        ImGui::Checkbox("Enable Blur", &m_TagSettings.EnableBlur);
        if (m_TagSettings.EnableBlur) {
            ImGui::DragInt("Blur Strength", &m_TagSettings.BlurDownscale, 0.5f, 2, 64);
            if (ImGui::IsItemDeactivatedAfterEdit() && !m_ImageFiles.empty()) {
                UnloadCurrentImages();
                LoadImageTexture(m_ImageFiles[m_CurrentImageIdx].c_str(), &m_MainImageTexture, &m_MainImageWidth, &m_MainImageHeight, &m_BlurredImageTexture, m_TagSettings.BlurDownscale);
            }
            ImGui::DragFloat("Blur Opacity", &m_TagSettings.BackgroundOpacity, 0.005f, 0.0f, 1.0f, "%.2f");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (processing) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.067f, 0.761f, 0.929f, 0.80f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.067f, 0.761f, 0.929f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.84f, 0.97f, 1.00f));
        if (ImGui::Button("Export All", ImVec2(-1, 36))) {
            ProcessAllImages(); 
        }
        ImGui::PopStyleColor(3);
        if (processing) ImGui::EndDisabled();

        if (processing) {
            float progress = m_ProcessingProgress;
            int current = (int)(progress * m_ImageFiles.size()) + 1;
            int total = (int)m_ImageFiles.size();
            if (current > total) current = total;

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Spinner + text side by side
            ImVec2 cPos = ImGui::GetCursorScreenPos();
            float spinRadius = 16.0f;
            float spinCX = cPos.x + spinRadius + 4.0f;
            float spinCY = cPos.y + spinRadius + 2.0f;
            ImDrawList* dlp = ImGui::GetWindowDrawList();
            float t = (float)ImGui::GetTime();
            int segCount = 12;
            for (int si = 0; si < segCount; si++) {
                float angle = t * 5.0f + (float)si * (2.0f * 3.14159f / (float)segCount);
                float alpha = 0.1f + 0.9f * ((float)si / (float)segCount);
                float x1 = spinCX + cosf(angle) * (spinRadius - 3.0f);
                float y1 = spinCY + sinf(angle) * (spinRadius - 3.0f);
                float x2 = spinCX + cosf(angle) * spinRadius;
                float y2 = spinCY + sinf(angle) * spinRadius;
                dlp->AddLine(ImVec2(x1, y1), ImVec2(x2, y2),
                    IM_COL32(17, 194, 237, (int)(255 * alpha)), 2.5f);
            }
            // Text next to spinner
            ImGui::SetCursorScreenPos(ImVec2(cPos.x + spinRadius * 2.0f + 14.0f, cPos.y + 10.0f));
            ImGui::TextColored(ImVec4(0.067f, 0.761f, 0.929f, 1.0f),
                "Exporting %d / %d", current, total);

            ImGui::SetCursorScreenPos(ImVec2(cPos.x, cPos.y + spinRadius * 2.0f + 10.0f));
            ImGui::ProgressBar(progress, ImVec2(-1, 8));
            ImGui::Spacing();
        }

        ImGui::EndChild(); // LeftPanel

        ImGui::SameLine();
    }

    // Right Panel - Preview
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
    
    // Top Bar
    if (m_MainImageTexture) {
        if (ImGui::Button(isFullscreen ? "Exit Fullscreen" : "Fullscreen")) {
            m_FullscreenPreview = !m_FullscreenPreview;
        }
        ImGui::SameLine();
        if (ImGui::Button("<< Prev") && m_CurrentImageIdx > 0) {
            m_CurrentImageIdx--;
            UnloadCurrentImages();
            LoadImageTexture(m_ImageFiles[m_CurrentImageIdx].c_str(), &m_MainImageTexture, &m_MainImageWidth, &m_MainImageHeight, &m_BlurredImageTexture, m_TagSettings.BlurDownscale);
        }
        ImGui::SameLine();
        ImGui::Text("%d / %d: %s", m_CurrentImageIdx + 1, (int)m_ImageFiles.size(), m_ImageFiles[m_CurrentImageIdx].c_str());
        ImGui::SameLine();
        if (ImGui::Button("Next >>") && m_CurrentImageIdx < m_ImageFiles.size() - 1) {
            m_CurrentImageIdx++;
            UnloadCurrentImages();
            LoadImageTexture(m_ImageFiles[m_CurrentImageIdx].c_str(), &m_MainImageTexture, &m_MainImageWidth, &m_MainImageHeight, &m_BlurredImageTexture, m_TagSettings.BlurDownscale);
        }

        // Draw Image using available space
        ImVec2 avail = ImGui::GetContentRegionAvail();
        
        // Center Image Logic
        float aspect = (float)m_MainImageWidth / (float)m_MainImageHeight;
        ImVec2 drawSize = avail;
        if (avail.x / avail.y > aspect) {
            drawSize.x = avail.y * aspect;
        } else {
            drawSize.y = avail.x / aspect;
        }
        
        // Centering Offset
        float offsetX = (avail.x - drawSize.x) * 0.5f;
        float offsetY = (avail.y - drawSize.y) * 0.5f;

        ImVec2 drawPos = ImGui::GetCursorScreenPos();
        drawPos.x += offsetX;
        drawPos.y += offsetY;

        // Background/Blur Base
        ImGui::GetWindowDrawList()->AddImage((void*)m_MainImageTexture, drawPos, ImVec2(drawPos.x + drawSize.x, drawPos.y + drawSize.y));
        
        // Axis Lines (Dashed)
        // Center vertical
        float cx = drawPos.x + drawSize.x * 0.5f;
        float cy = drawPos.y + drawSize.y * 0.5f;
        ImDrawList* dl = ImGui::GetWindowDrawList();
        
        // Draw Dashed Line Helper
        auto DrawDashedLine = [&](ImVec2 p1, ImVec2 p2, ImU32 col, float thickness) {
            float len = sqrtf((p2.x - p1.x)*(p2.x - p1.x) + (p2.y - p1.y)*(p2.y - p1.y));
            if (len <= 0.0f) return;
            float segmentLen = 10.0f;
            int segments = (int)(len / segmentLen);
            ImVec2 dir = ImVec2((p2.x - p1.x)/len, (p2.y - p1.y)/len);
            for(int i=0; i<segments; i+=2) {
                ImVec2 s = ImVec2(p1.x + dir.x * i * segmentLen, p1.y + dir.y * i * segmentLen);
                ImVec2 e = ImVec2(p1.x + dir.x * (i+1) * segmentLen, p1.y + dir.y * (i+1) * segmentLen);
                dl->AddLine(s, e, col, thickness);
            }
        };

        ImU32 axisCol = IM_COL32(255, 255, 255, 100);
        DrawDashedLine(ImVec2(cx, drawPos.y), ImVec2(cx, drawPos.y + drawSize.y), axisCol, 1.0f);
        DrawDashedLine(ImVec2(drawPos.x, cy), ImVec2(drawPos.x + drawSize.x, cy), axisCol, 1.0f);


        // Tag Calculation & Selection
        ID3D11ShaderResourceView* tagTex = nullptr;
        int tW = 0, tH = 0;
        
        bool isPortrait = aspect < 1.0f;
        // Selection logic...
        if (isPortrait) {
            tagTex = m_PortraitTagTexture ? m_PortraitTagTexture : m_LandscapeTagTexture;
            tW = m_PortraitTagTexture ? m_PortraitTagWidth : m_LandscapeTagWidth;
            tH = m_PortraitTagTexture ? m_PortraitTagHeight : m_LandscapeTagHeight;
        } else {
            tagTex = m_LandscapeTagTexture ? m_LandscapeTagTexture : m_PortraitTagTexture;
            tW = m_LandscapeTagTexture ? m_LandscapeTagWidth : m_PortraitTagWidth;
            tH = m_LandscapeTagTexture ? m_LandscapeTagHeight : m_PortraitTagHeight;
        }

        // Auto-Scale logic (100% width fit, clamped)
        // TagScale default is 1.0f now (fit width).
        
        float tagW = 0, tagH = 0;
        if (tagTex && tW > 0 && tH > 0) {
             float tagAspect = (float)tW / (float)tH;
             tagW = drawSize.x * m_TagSettings.TagScale; 
             tagH = tagW / tagAspect;
        } else {
             tagW = drawSize.x * m_TagSettings.TagScale;
             tagH = tagW;
        }

        // Map PosXPct/PosYPct [0,1] to the valid center range so 0=left edge, 1=right edge
        float halfTagW = tagW * 0.5f;
        float halfTagH = tagH * 0.5f;
        
        float minCX = halfTagW;
        float maxCX = drawSize.x - halfTagW;
        if (minCX > maxCX) { minCX = maxCX = drawSize.x * 0.5f; }
        float currentCX = minCX + m_TagSettings.PosXPct * (maxCX - minCX);

        float minCY = halfTagH;
        float maxCY = drawSize.y - halfTagH;
        if (minCY > maxCY) { minCY = maxCY = drawSize.y * 0.5f; }
        float currentCY = minCY + m_TagSettings.PosYPct * (maxCY - minCY);

        float tagX = drawPos.x + currentCX - halfTagW;
        float tagY = drawPos.y + currentCY - halfTagH;
        
        // Render Blur Clip
        if (m_TagSettings.EnableBlur && m_BlurredImageTexture) {
            ImGui::PushClipRect(ImVec2(tagX, tagY), ImVec2(tagX + tagW, tagY + tagH), true);
            ImGui::GetWindowDrawList()->AddImage((void*)m_BlurredImageTexture, drawPos, ImVec2(drawPos.x + drawSize.x, drawPos.y + drawSize.y), ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255, (int)(255 * m_TagSettings.BackgroundOpacity)));
            ImGui::PopClipRect();
        }

        // Render Tag Overlay
        if (tagTex) {
            ImGui::GetWindowDrawList()->AddImage((void*)tagTex, ImVec2(tagX, tagY), ImVec2(tagX + tagW, tagY + tagH), ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255, (int)(255 * m_TagSettings.TagOpacity)));
        } 
         
        // Visuals: Highlight Border
        ImGui::GetWindowDrawList()->AddRect(ImVec2(tagX, tagY), ImVec2(tagX + tagW, tagY + tagH), IM_COL32(255, 255, 0, 200), 0.0f, 0, 2.0f);
        
        // Visuals: Tag Center Axes (Dashed)
        {
            float tcx = tagX + tagW * 0.5f;
            float tcy = tagY + tagH * 0.5f;
            float ext = 10.0f;
            ImU32 dashedCol = IM_COL32(255, 255, 0, 150); 
            DrawDashedLine(ImVec2(tcx, tagY - ext), ImVec2(tcx, tagY + tagH + ext), dashedCol, 1.0f);
            DrawDashedLine(ImVec2(tagX - ext, tcy), ImVec2(tagX + tagW + ext, tcy), dashedCol, 1.0f);
        }

        // Grid (Front most)
        if (m_TagSettings.SnapGrid) {
            DrawGrid(drawPos, drawSize); 
        }

        // Resizing Handle (Bottom Right) — rendered first so it gets input priority
        float handleSize = 14.0f;
        float hx = tagX + tagW - handleSize;
        float hy = tagY + tagH - handleSize;
        dl->AddRectFilled(ImVec2(hx, hy), ImVec2(hx + handleSize, hy + handleSize), IM_COL32(220, 40, 40, 220));
        dl->AddRect(ImVec2(hx, hy), ImVec2(hx + handleSize, hy + handleSize), IM_COL32(255, 255, 255, 180), 0.0f, 0, 1.0f);
        ImGui::SetCursorScreenPos(ImVec2(hx, hy));
        ImGui::InvisibleButton("##Resize", ImVec2(handleSize, handleSize));
        bool resizing = ImGui::IsItemActive();
        if (resizing && ImGui::IsMouseDragging(0)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            float change = delta.x / drawSize.x;
            m_TagSettings.TagScale += change;
            if (m_TagSettings.TagScale < 0.01f) m_TagSettings.TagScale = 0.01f;
            if (m_TagSettings.TagScale > 1.0f) m_TagSettings.TagScale = 1.0f;
        }
        if (ImGui::IsItemHovered() || resizing) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);

        // Tag Drag Interaction
        ImGui::SetCursorScreenPos(ImVec2(tagX, tagY));
        ImGui::InvisibleButton("##TagDrag", ImVec2(tagW, tagH));
        
        if (ImGui::IsItemActivated()) {
             m_UnsnappedPosX = m_TagSettings.PosXPct;
             m_UnsnappedPosY = m_TagSettings.PosYPct;
        }

        if (!resizing && ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            float rangeX = (maxCX - minCX);
            float rangeY = (maxCY - minCY);
            float dX_pct = (rangeX > 0.0f) ? delta.x / rangeX : 0.0f;
            float dY_pct = (rangeY > 0.0f) ? delta.y / rangeY : 0.0f;
             
            m_UnsnappedPosX += dX_pct;
            m_UnsnappedPosY += dY_pct;
            
            float newXPct = m_UnsnappedPosX;
            float newYPct = m_UnsnappedPosY;

            if (m_TagSettings.SnapGrid && m_TagSettings.GridLines > 0) {
                 float gridStep = 1.0f / (float)m_TagSettings.GridLines;
                 auto checkSnap = [&](float val, float step) -> float {
                    float nearest = floor(val / step + 0.5f) * step;
                    float diff = fabs(val - nearest);
                    if (diff < 0.008f) return nearest; 
                    return val;
                };
                newXPct = checkSnap(newXPct, gridStep);
                newYPct = checkSnap(newYPct, gridStep);
            }
            
            if (newXPct < 0.0f) newXPct = 0.0f;
            if (newXPct > 1.0f) newXPct = 1.0f;
            if (newYPct < 0.0f) newYPct = 0.0f;
            if (newYPct > 1.0f) newYPct = 1.0f;

            m_TagSettings.PosXPct = newXPct;
            m_TagSettings.PosYPct = newYPct;

            m_UnsnappedPosX = newXPct;
            m_UnsnappedPosY = newYPct;
        }
        if (!resizing && ImGui::IsItemHovered()) {
             ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

    } else {
        ImGui::Text("No image loaded or selected.");
    }
    
    ImGui::EndChild(); // RightPanel

    ImGui::End(); // Photo Tagger
}

void PhotoApp::DrawGrid(ImVec2 p, ImVec2 s) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // Cyan, thinner lines
    ImU32 col = IM_COL32(0, 255, 255, 120); 
    int lines = m_TagSettings.GridLines;
    if (lines < 1) lines = 1;

    for (int i = 1; i < lines; i++) {
        float t = (float)i / (float)lines;
        float x = p.x + s.x * t;
        float y = p.y + s.y * t;
        dl->AddLine(ImVec2(x, p.y), ImVec2(x, p.y + s.y), col, 1.0f);
        dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + s.x, y), col, 1.0f);
    }
    
    // Ruler/Margin Display
    if (m_TagSettings.SafeMargin > 0.0f) {
        float mx = s.x * m_TagSettings.SafeMargin;
        float my = s.y * m_TagSettings.SafeMargin;
        dl->AddRect(ImVec2(p.x + mx, p.y + my), ImVec2(p.x + s.x - mx, p.y + s.y - my), IM_COL32(255, 0, 0, 150), 0.0f, 0, 1.0f);
    }
}

void PhotoApp::ProcessAllImages() {
    if (m_IsProcessing) return;
    if (m_ImageFiles.empty()) return;
    if (std::string(m_Config.DestFolder).empty()) return;

    m_IsProcessing = true;
    m_ProcessingProgress = 0.0f;

    // Run in a separate thread to avoid freezing UI
    if (m_WorkerThread.joinable()) m_WorkerThread.join();
    m_WorkerThread = std::thread([this]() {
        size_t total = m_ImageFiles.size();

        // Cache JPEG encoder CLSID once
        CLSID jpegClsid = {};
        {
            UINT num = 0, size = 0;
            Gdiplus::GetImageEncodersSize(&num, &size);
            if (size != 0) {
                Gdiplus::ImageCodecInfo* pInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
                Gdiplus::GetImageEncoders(num, size, pInfo);
                for (UINT j = 0; j < num; ++j) {
                    if (wcscmp(pInfo[j].MimeType, L"image/jpeg") == 0) {
                        jpegClsid = pInfo[j].Clsid;
                        break;
                    }
                }
                free(pInfo);
            }
        }

        // Cache tag bitmaps once (load portrait/landscape tag images before loop)
        Gdiplus::Bitmap* cachedPortraitTag = nullptr;
        Gdiplus::Bitmap* cachedLandscapeTag = nullptr;
        if (strlen(m_Config.PortraitTagPath) > 0) {
            cachedPortraitTag = Gdiplus::Bitmap::FromFile(Utf8ToWide(m_Config.PortraitTagPath).c_str());
            if (cachedPortraitTag && cachedPortraitTag->GetLastStatus() != Gdiplus::Ok) { delete cachedPortraitTag; cachedPortraitTag = nullptr; }
        }
        if (strlen(m_Config.LandscapeTagPath) > 0) {
            cachedLandscapeTag = Gdiplus::Bitmap::FromFile(Utf8ToWide(m_Config.LandscapeTagPath).c_str());
            if (cachedLandscapeTag && cachedLandscapeTag->GetLastStatus() != Gdiplus::Ok) { delete cachedLandscapeTag; cachedLandscapeTag = nullptr; }
        }

        // Snapshot settings for thread safety
        float settingScale = m_TagSettings.TagScale;
        float settingPosX = m_TagSettings.PosXPct;
        float settingPosY = m_TagSettings.PosYPct;
        float settingTagOpacity = m_TagSettings.TagOpacity;
        float settingBgOpacity = m_TagSettings.BackgroundOpacity;
        bool  settingBlur = m_TagSettings.EnableBlur;
        int   settingBlurStrength = m_TagSettings.BlurDownscale;

        for (size_t i = 0; i < total; ++i) {
            if (!m_IsProcessing) break;

            std::string filePath = m_ImageFiles[i];
            m_ProcessingProgress = (float)i / (float)total;

            std::wstring wFilePath = Utf8ToWide(filePath);
            Gdiplus::Bitmap* srcBmp = Gdiplus::Bitmap::FromFile(wFilePath.c_str());
            if (!srcBmp || srcBmp->GetLastStatus() != Gdiplus::Ok) { delete srcBmp; continue; }

            int w = srcBmp->GetWidth();
            int h = srcBmp->GetHeight();
            
            Gdiplus::Bitmap* offScreen = new Gdiplus::Bitmap(w, h, PixelFormat32bppARGB);
            Gdiplus::Graphics* gr = Gdiplus::Graphics::FromImage(offScreen);
            gr->SetCompositingMode(Gdiplus::CompositingModeSourceCopy);
            gr->SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
            gr->DrawImage(srcBmp, 0, 0, w, h);
            gr->SetCompositingMode(Gdiplus::CompositingModeSourceOver);
            gr->SetInterpolationMode(Gdiplus::InterpolationModeBilinear);

            // Select tag
            bool isPortrait = h > w;
            Gdiplus::Bitmap* tagBmp = nullptr;
            if (isPortrait) {
                tagBmp = cachedPortraitTag ? cachedPortraitTag : cachedLandscapeTag;
            } else {
                tagBmp = cachedLandscapeTag ? cachedLandscapeTag : cachedPortraitTag;
            }

            float tagW = 0, tagH = 0;
            if (tagBmp) {
                 float tagAspect = (float)tagBmp->GetWidth() / (float)tagBmp->GetHeight();
                 tagW = (float)w * settingScale;
                 tagH = tagW / tagAspect;
            } else {
                 tagW = (float)w * settingScale;
                 tagH = tagW; 
            }

            float halfTW = tagW * 0.5f;
            float halfTH = tagH * 0.5f;
            float eminCX = halfTW, emaxCX = (float)w - halfTW;
            if (eminCX > emaxCX) eminCX = emaxCX = w * 0.5f;
            float eminCY = halfTH, emaxCY = (float)h - halfTH;
            if (eminCY > emaxCY) eminCY = emaxCY = h * 0.5f;
            float tagX = (eminCX + settingPosX * (emaxCX - eminCX)) - halfTW;
            float tagY = (eminCY + settingPosY * (emaxCY - eminCY)) - halfTH;
            
            if (settingBlur) {
                 int blurRad = settingBlurStrength;
                 if (blurRad < 1) blurRad = 1;

                 const int MAX_BLUR_DIM = 512;
                 float ratio = (float)w / (float)h;
                 int smallW = w, smallH = h;
                 if (smallW > MAX_BLUR_DIM || smallH > MAX_BLUR_DIM) {
                     if (ratio >= 1.0f) { smallW = MAX_BLUR_DIM; smallH = (int)(MAX_BLUR_DIM / ratio); }
                     else { smallH = MAX_BLUR_DIM; smallW = (int)(MAX_BLUR_DIM * ratio); }
                 }
                 if (smallW < 1) smallW = 1;
                 if (smallH < 1) smallH = 1;

                 Gdiplus::Bitmap* smallBmp = new Gdiplus::Bitmap(smallW, smallH, PixelFormat32bppARGB);
                 Gdiplus::Graphics* sg = Gdiplus::Graphics::FromImage(smallBmp);
                 sg->SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
                 sg->DrawImage(srcBmp, 0, 0, smallW, smallH);
                 delete sg;

                 int scaledRadius = (int)(blurRad * ((float)smallW / (float)w));
                 if (scaledRadius < 1) scaledRadius = 1;
                 GenerateGaussianBlur(smallBmp, scaledRadius);

                 gr->SetClip(Gdiplus::RectF(tagX, tagY, tagW, tagH));
                 
                 Gdiplus::ImageAttributes attr;
                 Gdiplus::ColorMatrix cm = { 
                     1,0,0,0,0,
                     0,1,0,0,0,
                     0,0,1,0,0,
                     0,0,0,settingBgOpacity,0,
                     0,0,0,0,1 };
                 attr.SetColorMatrix(&cm);
                 
                 gr->DrawImage(smallBmp, Gdiplus::RectF(0,0,(Gdiplus::REAL)w,(Gdiplus::REAL)h), 0,0, (Gdiplus::REAL)smallW,(Gdiplus::REAL)smallH, Gdiplus::UnitPixel, &attr);
                 
                 gr->ResetClip();
                 delete smallBmp;
            }

            // Free source bitmap early — no longer needed
            delete srcBmp;
            srcBmp = nullptr;

            // Draw Tag
            if (tagBmp) {
                 Gdiplus::ImageAttributes attr;
                 Gdiplus::ColorMatrix cm = { 
                     1,0,0,0,0,
                     0,1,0,0,0,
                     0,0,1,0,0,
                     0,0,0,settingTagOpacity,0,
                     0,0,0,0,1 };
                 attr.SetColorMatrix(&cm);
                 gr->DrawImage(tagBmp, Gdiplus::RectF(tagX, tagY, tagW, tagH), 0,0, (Gdiplus::REAL)tagBmp->GetWidth(), (Gdiplus::REAL)tagBmp->GetHeight(), Gdiplus::UnitPixel, &attr);
            }
            
            // Save with JPEG quality 92
            std::string name = filePath.substr(filePath.find_last_of("\\") + 1);
            std::wstring wDest = Utf8ToWide(std::string(m_Config.DestFolder) + "\\Tagged_" + name);
            Gdiplus::EncoderParameters encParams;
            encParams.Count = 1;
            encParams.Parameter[0].Guid = Gdiplus::EncoderQuality;
            encParams.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
            encParams.Parameter[0].NumberOfValues = 1;
            ULONG jpegQuality = 92;
            encParams.Parameter[0].Value = &jpegQuality;
            offScreen->Save(wDest.c_str(), &jpegClsid, &encParams);

            delete gr;
            delete offScreen;
        }
        
        delete cachedPortraitTag;
        delete cachedLandscapeTag;
        m_ProcessingProgress = 1.0f;
        m_IsProcessing = false;
    });
}

// Dialog stubs
void PhotoApp::OpenFolderDialog(char* buffer, int maxLen) {
    IFileDialog *pfd;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
        DWORD dwOptions;
        if (SUCCEEDED(pfd->GetOptions(&dwOptions))) {
            pfd->SetOptions(dwOptions | FOS_PICKFOLDERS);
        }
        if (SUCCEEDED(pfd->Show(NULL))) {
            IShellItem *psi;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR pszPath;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                    std::string path = WideToUtf8(pszPath);
                    strncpy_s(buffer, maxLen, path.c_str(), _TRUNCATE);
                    CoTaskMemFree(pszPath);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }
}

void PhotoApp::OpenFileDialog(char* buffer, int maxLen) {
     IFileDialog *pfd;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd)))) {
        if (SUCCEEDED(pfd->Show(NULL))) {
            IShellItem *psi;
            if (SUCCEEDED(pfd->GetResult(&psi))) {
                PWSTR pszPath;
                if (SUCCEEDED(psi->GetDisplayName(SIGDN_FILESYSPATH, &pszPath))) {
                    std::string path = WideToUtf8(pszPath);
                    strncpy_s(buffer, maxLen, path.c_str(), _TRUNCATE);
                    CoTaskMemFree(pszPath);
                }
                psi->Release();
            }
        }
        pfd->Release();
    }
}
