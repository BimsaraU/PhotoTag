#include "PhotoTag.h"
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

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);
}

PhotoApp::~PhotoApp() {
    m_IsProcessing = false; // Signal thread to stop
    if (m_WorkerThread.joinable()) {
        m_WorkerThread.join();
    }
    UnloadCurrentImages();
    Gdiplus::GdiplusShutdown(m_gdiplusToken);
}

// Separable Gaussian Blur (approximated by passes of Box Blur for speed)
void PhotoApp::GenerateGaussianBlur(Gdiplus::Bitmap* bmp, int radius) {
    if (radius < 1) return;

    int w = bmp->GetWidth();
    int h = bmp->GetHeight();

    Gdiplus::Rect rect(0, 0, w, h);
    Gdiplus::BitmapData data;
    if (bmp->LockBits(&rect, Gdiplus::ImageLockModeRead | Gdiplus::ImageLockModeWrite, PixelFormat32bppARGB, &data) != Gdiplus::Ok) return;

    int stride = data.Stride;
    unsigned char* pixels = (unsigned char*)data.Scan0;

    std::vector<unsigned char> buffer(h * stride);
    memcpy(buffer.data(), pixels, h * stride);

    // Single box pass, horizontal only for now
    float iarr = 1.0f / (radius + radius + 1);
    for (int i = 0; i < h; i++) {
        int ti = i * stride;
        for (int j = 0; j < w; j++) {
            int sb = 0, sg = 0, sr = 0, cnt = 0;
            for (int k = -radius; k <= radius; k++) {
                int x = j + k;
                if (x < 0) x = 0;
                if (x >= w) x = w - 1;
                sb += buffer[ti + x * 4];
                sg += buffer[ti + x * 4 + 1];
                sr += buffer[ti + x * 4 + 2];
                cnt++;
            }
            pixels[ti + j * 4]     = (unsigned char)(sb / cnt);
            pixels[ti + j * 4 + 1] = (unsigned char)(sg / cnt);
            pixels[ti + j * 4 + 2] = (unsigned char)(sr / cnt);
            pixels[ti + j * 4 + 3] = 255;
        }
    }

    bmp->UnlockBits(&data);
}

void PhotoApp::UnloadCurrentImages() {
    if (m_MainImageTexture) { m_MainImageTexture->Release(); m_MainImageTexture = nullptr; }
}

// Simple GDI+ to DX11 loader
void PhotoApp::LoadImageTexture(const char* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height) {
    if (!filename || !*filename) return;

    std::wstring wFilename = Utf8ToWide(filename);
    Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(wFilename.c_str());

    if (bitmap->GetLastStatus() != Gdiplus::Ok) {
        delete bitmap;
        return;
    }

    *out_width = bitmap->GetWidth();
    *out_height = bitmap->GetHeight();

    Gdiplus::BitmapData data;
    Gdiplus::Rect rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight());
    bitmap->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data);

    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = bitmap->GetWidth();
    desc.Height = bitmap->GetHeight();
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
        m_pd3dDevice->CreateShaderResourceView(pTexture, &srvDesc, out_srv);
        pTexture->Release();
    }
    bitmap->UnlockBits(&data);
    delete bitmap;
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
        LoadImageTexture(m_ImageFiles[0].c_str(), &m_MainImageTexture, &m_MainImageWidth, &m_MainImageHeight);
    }
}

void PhotoApp::Update() {
   // Logic updates map go here
}

static void PathLabel(const char* text) {
    const char* display = (text && text[0]) ? text : "(none)";
    ImGui::TextWrapped("%s", display);
}

void PhotoApp::RenderUI() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    ImGui::Begin("Photo Tagger", NULL, window_flags);

    ImGui::Text("FOLDERS");
    ImGui::Separator();

    if (ImGui::Button("Source Folder", ImVec2(-1, 0))) OpenFolderDialog(m_Config.SourceFolder, 260);
    PathLabel(m_Config.SourceFolder);

    if (ImGui::Button("Output Folder", ImVec2(-1, 0))) OpenFolderDialog(m_Config.DestFolder, 260);
    PathLabel(m_Config.DestFolder);

    ImGui::Separator();
    ImGui::Text("IMAGES");

    if (ImGui::Button("Load Images", ImVec2(-1, 0))) {
        LoadSourceFolder();
    }
    ImGui::Text("%d images loaded", (int)m_ImageFiles.size());

    ImGui::Separator();
    if (ImGui::Button("Export All", ImVec2(-1, 36))) {
        ProcessAllImages();
    }

    if (m_MainImageTexture) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float aspect = (float)m_MainImageWidth / (float)m_MainImageHeight;
        ImVec2 drawSize = avail;
        if (avail.x / avail.y > aspect) {
            drawSize.x = avail.y * aspect;
        } else {
            drawSize.y = avail.x / aspect;
        }
        ImGui::Image((void*)m_MainImageTexture, drawSize);
    } else {
        ImGui::Text("No image loaded or selected.");
    }

    ImGui::End();
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

        for (size_t i = 0; i < total; ++i) {
            if (!m_IsProcessing) break;

            std::string filePath = m_ImageFiles[i];
            m_ProcessingProgress = (float)i / (float)total;

            std::wstring wFilePath = Utf8ToWide(filePath);
            Gdiplus::Bitmap* srcBmp = Gdiplus::Bitmap::FromFile(wFilePath.c_str());
            if (!srcBmp || srcBmp->GetLastStatus() != Gdiplus::Ok) { delete srcBmp; continue; }

            std::string name = filePath.substr(filePath.find_last_of("\\") + 1);
            std::wstring wDest = Utf8ToWide(std::string(m_Config.DestFolder) + "\\Tagged_" + name);
            srcBmp->Save(wDest.c_str(), &jpegClsid, NULL);

            delete srcBmp;
        }

        m_ProcessingProgress = 1.0f;
        m_IsProcessing = false;
    });
}

// Dialog stubs
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

        for (size_t i = 0; i < total; ++i) {
            if (!m_IsProcessing) break;

            std::string filePath = m_ImageFiles[i];
            m_ProcessingProgress = (float)i / (float)total;

            std::wstring wFilePath = Utf8ToWide(filePath);
            Gdiplus::Bitmap* srcBmp = Gdiplus::Bitmap::FromFile(wFilePath.c_str());
            if (!srcBmp || srcBmp->GetLastStatus() != Gdiplus::Ok) { delete srcBmp; continue; }

            std::string name = filePath.substr(filePath.find_last_of("\\") + 1);
            std::wstring wDest = Utf8ToWide(std::string(m_Config.DestFolder) + "\\Tagged_" + name);
            srcBmp->Save(wDest.c_str(), &jpegClsid, NULL);

            delete srcBmp;
        }

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
