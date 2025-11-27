#include "PhotoTag.h"
#include <tchar.h>
#include <shobjidl.h>
#include <algorithm>

#pragma comment (lib,"Gdiplus.lib")

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

    ImGui::Text("PhotoTag");
    ImGui::Separator();
    ImGui::Text("Source: %s", m_Config.SourceFolder);
    ImGui::Text("Output: %s", m_Config.DestFolder);

    ImGui::End();
}
