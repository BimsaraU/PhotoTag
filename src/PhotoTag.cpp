#include "PhotoTag.h"
#include <tchar.h>
#include <shobjidl.h>
#include <algorithm>

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
}

PhotoApp::~PhotoApp() {
    Gdiplus::GdiplusShutdown(m_gdiplusToken);
}

void PhotoApp::Update() {
   // Logic updates map go here
}

void PhotoApp::RenderUI() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

    ImGui::Begin("Photo Tagger", NULL, window_flags);

    ImGui::Text("FOLDERS");
    ImGui::Separator();

    if (ImGui::Button("Source Folder")) OpenFolderDialog(m_Config.SourceFolder, 260);
    ImGui::Text("%s", m_Config.SourceFolder);

    if (ImGui::Button("Output Folder")) OpenFolderDialog(m_Config.DestFolder, 260);
    ImGui::Text("%s", m_Config.DestFolder);

    ImGui::End();
}

// Dialog stubs
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
