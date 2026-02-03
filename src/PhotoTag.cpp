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
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

    // Apply UI Styling
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.0f); // Dark Gray
    style.Colors[ImGuiCol_ChildBg]  = ImVec4(0.15f, 0.15f, 0.15f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.6f, 0.8f, 1.0f, 1.0f); // Light Blue
    style.Colors[ImGuiCol_Button] = ImVec4(0.2f, 0.2f, 0.2f, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
    style.ScaleAllSizes(1.2f); 
    
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = 1.25f; // Bigger text
}

PhotoApp::~PhotoApp() {
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

    UploadBitmap(bitampToUpload, out_srv);

    if (out_blur_srv) {
        // Create a blurred version.
        // Instead of downscaling heavily for "blur", we use a higher quality Gaussian blur.
        // We still downscale slightly to save performance on the blur pass itself if the image is huge, 
        // but not as aggressively as before if quality is the goal.
        // Let's settle on a reasonable fixed max size for the blur map to keep it fast, e.g., 512px or 1024px?
        // Or just use the proxy.
        
        int w = bitampToUpload->GetWidth();
        int h = bitampToUpload->GetHeight();
        
        // Use a temp bitmap for blurring so we don't mess up the original proxy if we needed it?
        // Actually bitampToUpload is either original or proxy.
        // Let's make a copy for blur.
        Gdiplus::Bitmap* blurBmp = bitampToUpload->Clone(0, 0, w, h, PixelFormat32bppARGB);
        
        // Run Gaussian Blur
        // The radius should probably be proportional to image size to emulate "strength" or use the slider value directly?
        // The user slider is 2-64. If we apply that radius on a full 1920px image, it's decent.
        GenerateGaussianBlur(blurBmp, blur_downscale); // Repurposing blur_downscale as radius
        
        UploadBitmap(blurBmp, out_blur_srv);
        delete blurBmp;
    }

    if (usingProxy) delete bitampToUpload; // Delete the temp proxy
    delete bitmap; // Delete original
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

// Helper for truncated text in box
static void TextInBox(const char* label, const char* text, float width) {
    // ImGui::Text(label); // Removed to avoid empty line if label is hidden ID
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.3f, 0.3f, 1.0f)); // Light Gray-ish
    ImGui::BeginChild(label, ImVec2(width, ImGui::GetTextLineHeightWithSpacing() + 4), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextUnformatted(text);
    ImGui::EndChild();
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

    // Left Panel - Configuration (Hidden if Fullscreen)
    if (!isFullscreen) {
        ImGui::BeginChild("LeftPanel", ImVec2(350, 0), true); // Widen for better font
        ImGui::Text("Configuration");
        
        if (ImGui::Button("Source Folder")) OpenFolderDialog(m_Config.SourceFolder, 260);
        TextInBox("##Src", m_Config.SourceFolder, 330);

        if (ImGui::Button("Dest Folder")) OpenFolderDialog(m_Config.DestFolder, 260);
        TextInBox("##Dst", m_Config.DestFolder, 330);

        ImGui::Separator();
        
        // Portrait Tag
        if (ImGui::Button("Load Portrait Tag")) {
            OpenFileDialog(m_Config.PortraitTagPath, 260);
            if (m_PortraitTagTexture) m_PortraitTagTexture->Release();
            m_PortraitTagTexture = nullptr;
            LoadImageTexture(m_Config.PortraitTagPath, &m_PortraitTagTexture, &m_PortraitTagWidth, &m_PortraitTagHeight);
        }
        TextInBox("##PTag", m_Config.PortraitTagPath, 330);

        // Landscape Tag
        if (ImGui::Button("Load Landscape Tag")) {
            OpenFileDialog(m_Config.LandscapeTagPath, 260);
            if (m_LandscapeTagTexture) m_LandscapeTagTexture->Release();
            m_LandscapeTagTexture = nullptr;
            LoadImageTexture(m_Config.LandscapeTagPath, &m_LandscapeTagTexture, &m_LandscapeTagWidth, &m_LandscapeTagHeight);
        }
        TextInBox("##LTag", m_Config.LandscapeTagPath, 330);

        ImGui::Separator();
        
        if (ImGui::Button("Refresh / Load Images")) {
            LoadSourceFolder();
        }
        ImGui::Text("Loaded %d images", (int)m_ImageFiles.size());

        ImGui::Separator();
        ImGui::Text("Tag Settings");
        ImGui::SliderFloat("Scale", &m_TagSettings.TagScale, 0.01f, 2.0f);
        
        ImGui::Text("Position");
        ImGui::SliderFloat("Pos X %", &m_TagSettings.PosXPct, 0.0f, 1.0f);
        ImGui::SliderFloat("Pos Y %", &m_TagSettings.PosYPct, 0.0f, 1.0f);
        
        ImGui::Checkbox("Snap Grid", &m_TagSettings.SnapGrid);
        ImGui::SliderInt("Grid Lines", &m_TagSettings.GridLines, 2, 20);
        ImGui::SliderFloat("Safe Margin", &m_TagSettings.SafeMargin, 0.0f, 0.5f);

        // Loose Snap Logic handled in Dragging
        // if (m_TagSettings.SnapGrid) { ... } // moved to interactive dragging

        ImGui::Checkbox("Enable Blur", &m_TagSettings.EnableBlur);
        if (ImGui::SliderInt("Blur Range", &m_TagSettings.BlurDownscale, 2, 64)) {
            if (ImGui::IsItemDeactivatedAfterEdit() && !m_ImageFiles.empty()) {
                UnloadCurrentImages();
                LoadImageTexture(m_ImageFiles[m_CurrentImageIdx].c_str(), &m_MainImageTexture, &m_MainImageWidth, &m_MainImageHeight, &m_BlurredImageTexture, m_TagSettings.BlurDownscale);
            }
        }
        ImGui::SliderFloat("Tag Opacity", &m_TagSettings.TagOpacity, 0.0f, 1.0f);
        ImGui::SliderFloat("Blur/BG Opacity", &m_TagSettings.BackgroundOpacity, 0.0f, 1.0f);

        ImGui::Separator();
        if (ImGui::Button("Process All & Export", ImVec2(-1, 40))) {
            ProcessAllImages(); 
        }
        ImGui::EndChild(); // LeftPanel

        ImGui::SameLine();
    }

    // Right Panel - Preview
    ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
    
    // Top Bar (Nav + Fullscreen Toggle)
    if (m_MainImageTexture) {
        if (ImGui::Button(isFullscreen ? "Exit Fullscreen" : "Fullscreen Preview")) {
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
        // Only trigger this if scale hasn't been set by user? 
        // Or re-normalize. For now, assuming user wants to start at "fit width".
        // But TagScale is persistent in m_TagSettings.
        // If we want "loads to 100% to fit the width of the tag", maybe it means the tag should be initially scaled so its width == image width?
        // Let's rely on the user adjusting it, unless user explicitly said "make it so the tag loads to 100%".
        // Since TagScale is a multiplier of image width (tagW = drawSize.x * scale), setting scale = 1.0f makes tag width = image width.
        // Wait, current logic: tagW = drawSize.x * Scale. So Scale 1.0 is 100% width. Default is 0.2.
        // User wants it to load to 100%. I should change default default or let user control.
        // I'll update PhotoTag.h later to set default scale to 1.0f.
        
        float tagW = 0, tagH = 0;
        if (tagTex && tW > 0 && tH > 0) {
             float tagAspect = (float)tW / (float)tH;
             tagW = drawSize.x * m_TagSettings.TagScale; 
             tagH = tagW / tagAspect;
        } else {
             tagW = drawSize.x * m_TagSettings.TagScale;
             tagH = tagW;
        }

        // Clamp Tag to Image Bounds Logic
        // Calculating potential X/Y
        float halfTagW = tagW * 0.5f;
        float halfTagH = tagH * 0.5f;
        
        // Current Center in pixels (relative to drawPos)
        float currentCX = drawSize.x * m_TagSettings.PosXPct;
        float currentCY = drawSize.y * m_TagSettings.PosYPct;
        
        // Clamp Center so edges don't leave image
        // Left Edge: currentCX - halfTagW >= 0 -> currentCX >= halfTagW
        // Right Edge: currentCX + halfTagW <= drawSize.x
        if (halfTagW > drawSize.x * 0.5f) {
            // Tag bigger than image width? Center it.
             currentCX = drawSize.x * 0.5f;
        } else {
             if (currentCX < halfTagW) currentCX = halfTagW;
             if (currentCX > drawSize.x - halfTagW) currentCX = drawSize.x - halfTagW;
        }
        
        if (halfTagH > drawSize.y * 0.5f) {
             currentCY = drawSize.y * 0.5f;
        } else {
             if (currentCY < halfTagH) currentCY = halfTagH;
             if (currentCY > drawSize.y - halfTagH) currentCY = drawSize.y - halfTagH;
        }
        
        // Write back clamped values to persistent state? 
        // Or just use for rendering? Better to write back so slider matches visually.
        // But we only want to write back if not dragging, OR we clamp the dragging result?
        // If we clamp here, it overrides user drag if they try to go out. That's good "cannot go beyond".
        m_TagSettings.PosXPct = currentCX / drawSize.x;
        m_TagSettings.PosYPct = currentCY / drawSize.y;
    }

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
        // Draw crosshair within tag or extending? Usually within tag plus a bit.
        // Let's extend slightly.
        float ext = 10.0f;
        ImU32 axisCol = IM_COL32(255, 255, 0, 150); // Yellowish to match border
        DrawDashedLine(ImVec2(tcx, tagY - ext), ImVec2(tcx, tagY + tagH + ext), axisCol, 1.0f);
        DrawDashedLine(ImVec2(tagX - ext, tcy), ImVec2(tagX + tagW + ext, tcy), axisCol, 1.0f);
    }

    // Grid (Front most)
    if (m_TagSettings.SnapGrid) {
        DrawGrid(drawPos, drawSize); // Grid draws based on m_GridLines
    }

    // Interaction
    ImGui::SetCursorScreenPos(ImVec2(tagX, tagY));
    ImGui::InvisibleButton("##TagDrag", ImVec2(tagW, tagH));
    
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        float dX_pct = delta.x / drawSize.x;
        float dY_pct = delta.y / drawSize.y;
        
        float newXPct = m_TagSettings.PosXPct + dX_pct;
        float newYPct = m_TagSettings.PosYPct + dY_pct;

        // Loose Snapping logic ...
        // Apply snapping only if holding Shift? Or alway loose snap?
        // "loosely snapped so u can drag it with a bit extra effort"
        // My threshold logic handles "extra effort" implicitly (you have to move past the threshold to break snap if implemented with sticky logic, but here it just snaps if close).
        // To make it "sticky" (resistance), we would need to store "snapped" state. 
        // Simple proximity snap is usually what is meant by "loose" in UI terms (not checking every pixel, but a radius).
        
        if (m_TagSettings.SnapGrid) {
             float gridStep = 1.0f / (float)m_TagSettings.GridLines;
             auto looseSnap = [&](float val, float step) -> float {
                float nearest = floor(val / step + 0.5f) * step;
                float diff = fabs(val - nearest);
                if (diff < 0.02f) return nearest; 
                return val;
            };
            newXPct = looseSnap(newXPct, gridStep);
            newYPct = looseSnap(newYPct, gridStep);
        }
        
        m_TagSettings.PosXPct = newXPct;
        m_TagSettings.PosYPct = newYPct;
    }
    if ImGui::.IsItemHovered()) {
         ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    // Resizing Handle (Bottom Right)
    float handleSize = 10.0f;
    ImGui::SetCursorScreenPos(ImVec2(tagX + tagW - handleSize, tagY + tagH - handleSize));
    ImGui::Button("##Resize", ImVec2(handleSize, handleSize)); 
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        float change = delta.x / drawSize.x;
        m_TagSettings.TagScale += change;
        if (m_TagSettings.TagScale < 0.01f) m_TagSettings.TagScale = 0.01f;
    }
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);

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
    // Basic implementation of export using GDI+
    // NOTE: This runs on UI thread, blocks interaction
    if (std::string(m_Config.DestFolder).empty()) return;

    for (const auto& filePath : m_ImageFiles) {
        std::wstring wFilePath = Utf8ToWide(filePath);
        Gdiplus::Bitmap* srcBmp = Gdiplus::Bitmap::FromFile(wFilePath.c_str());
        if (srcBmp->GetLastStatus() != Gdiplus::Ok) { delete srcBmp; continue; }

        int w = srcBmp->GetWidth();
        int h = srcBmp->GetHeight();
        
        Gdiplus::Bitmap* offScreen = new Gdiplus::Bitmap(w, h, PixelFormat32bppARGB);
        Gdiplus::Graphics* gr = Gdiplus::Graphics::FromImage(offScreen);
        
        gr->DrawImage(srcBmp, 0,0, w, h);

        // Decide tag size/dimensions based on loaded logic
        float tagW = 0, tagH = 0;
        
        // Find which tag
        Gdiplus::Bitmap* tagBmp = nullptr;
        std::string tagPathToUse = "";
        
        bool isPortrait = h > w;
        
        // Auto-identify and fallback
        if (isPortrait) {
            if (m_PortraitTagTexture) tagPathToUse = m_Config.PortraitTagPath;
            else if (m_LandscapeTagTexture) tagPathToUse = m_Config.LandscapeTagPath;
        } else {
            if (m_LandscapeTagTexture) tagPathToUse = m_Config.LandscapeTagPath;
            else if (m_PortraitTagTexture) tagPathToUse = m_Config.PortraitTagPath;
        }

        if (!tagPathToUse.empty()) {
             tagBmp = Gdiplus::Bitmap::FromFile(Utf8ToWide(tagPathToUse).c_str());
        }

        if (tagBmp && tagBmp->GetLastStatus() == Gdiplus::Ok) {
             int tW = tagBmp->GetWidth();
             int tH = tagBmp->GetHeight();
             float tagAspect = (float)tW / (float)tH;
             tagW = (float)w * m_TagSettings.TagScale;
             tagH = tagW / tagAspect;
        } else {
            // Fallback
             tagW = (float)w * m_TagSettings.TagScale;
             tagH = tagW; // Square assumption if failure
             // We can't really draw tag if it failed loading but we proceed for blur calculation? 
             // If no tagBmp, we skip tag drawing but maybe still blur? 
        }

        float tagX = (w * m_TagSettings.PosXPct) - (tagW * 0.5f);
        float tagY = (h * m_TagSettings.PosYPct) - (tagH * 0.5f);
        
        if (m_TagSettings.EnableBlur) {
            // Very simple blur: Draw a scaled down version then scale up
             int blurScale = m_TagSettings.BlurDownscale;
             if (blurScale < 1) blurScale = 1;

             int smallW = max(1, w/blurScale);
             int smallH = max(1, h/blurScale);
             Gdiplus::Bitmap* smallBmp = new Gdiplus::Bitmap(smallW, smallH, PixelFormat32bppARGB);
             Gdiplus::Graphics* sgr = Gdiplus::Graphics::FromImage(smallBmp);
             sgr->SetInterpolationMode(Gdiplus::InterpolationModeBilinear);
             sgr->DrawImage(srcBmp, 0,0, smallW, smallH);
             

             // Draw back clipped
             // GDI+ doesn't have easy "PushClip" for raster ops but we can use SetClip
             gr->SetClip(Gdiplus::RectF(tagX, tagY, tagW, tagH));
             
             // Draw blurred image
             Gdiplus::ImageAttributes attr;
             Gdiplus::ColorMatrix cm = { 
                 1,0,0,0,0,
                 0,1,0,0,0,
                 0,0,1,0,0,
                 0,0,0,m_TagSettings.BackgroundOpacity,0,
                 0,0,0,0,1 };
             attr.SetColorMatrix(&cm);
             
             gr->DrawImage(smallBmp, Gdiplus::RectF(0,0,(Gdiplus::REAL)w,(Gdiplus::REAL)h), 0,0, (Gdiplus::REAL)smallW,(Gdiplus::REAL)smallH, Gdiplus::UnitPixel, &attr);
             
             gr->ResetClip();
             delete sgr;
             delete smallBmp;
        }

        // Draw Tag
        if (tagBmp && tagBmp->GetLastStatus() == Gdiplus::Ok) {
             Gdiplus::ImageAttributes attr;
             Gdiplus::ColorMatrix cm = { 
                 1,0,0,0,0,
                 0,1,0,0,0,
                 0,0,1,0,0,
                 0,0,0,m_TagSettings.TagOpacity,0,
                 0,0,0,0,1 };
             attr.SetColorMatrix(&cm);
             gr->DrawImage(tagBmp, Gdiplus::RectF(tagX, tagY, tagW, tagH), 0,0, (Gdiplus::REAL)tagBmp->GetWidth(), (Gdiplus::REAL)tagBmp->GetHeight(), Gdiplus::UnitPixel, &attr);
             delete tagBmp;
        }
        
        // Save
        std::string fileName = filePath.substr(filePath.find_last_of("\\") + 1);
        std::wstring wDest = Utf8ToWide(std::string(m_Config.DestFolder) + "\\Tagged_" + fileName);
        
        CLSID pngClsid;
        // Helper to get CLSID
        // (Simplified for brevity, assuming CLSID retrieval works or provided)
        // We will just use a hardcoded one or search
        UINT  num = 0;          // number of image encoders
        UINT  size = 0;         // size of the image encoder array in bytes
        Gdiplus::GetImageEncodersSize(&num, &size);
        if(size != 0) {
            Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
            Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);
            for(UINT j = 0; j < num; ++j) {
                if( wcscmp(pImageCodecInfo[j].MimeType, L"image/jpeg") == 0 ) {
                    pngClsid = pImageCodecInfo[j].Clsid;
                    break;
                }
            }
            free(pImageCodecInfo);
        }
        
        offScreen->Save(wDest.c_str(), &pngClsid, NULL);

        delete gr;
        delete offScreen;
        delete srcBmp;
    }
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
