#define WIN32_LEAN_AND_MEAN
#include "gui_app.hpp"

#include <Windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <commdlg.h>
#include <windowsx.h>
#include <dwmapi.h>

#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <iomanip>
#include <cstring>

#include "../core/core.hpp"
#include "../handler/handler.hpp"
#include "../utils/arguments.hpp"
#include "../utils/utils.hpp"

#include "../../vendor/imgui/imgui.h"
#include "../../vendor/imgui/backends/imgui_impl_dx11.h"
#include "../../vendor/imgui/backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dwmapi.lib")

static constexpr int kGuiWidth = 800;
static constexpr int kGuiHeight = 600;

struct GuiState {
    std::array<char, MAX_PATH> input{};
    std::array<char, MAX_PATH> output{};
    std::array<char, 32> fpackStart{};
    std::array<char, 32> fpackEnd{};
    int mutationBase = 1;
    bool removeAslr = false;
    bool obfuscateOep = false;
    bool antiDisasm = false;
    bool mba = false;
    bool encryptSections = false;
    bool fakeInstructions = false;
    bool packFunctions = false;
    bool outputCustomized = false;
    std::vector<std::pair<std::string, bool>> logEntries;
};

static ID3D11Device*           g_pd3dDevice = nullptr;
static ID3D11DeviceContext*    g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*         g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static bool CreateDeviceD3D(HWND hWnd);
static void CleanupDeviceD3D();
static void CreateRenderTarget();
static void CleanupRenderTarget();
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static bool ExecutePacking(const GuiState& state, std::string& statusMessage);
static void RenderGui(GuiState& state, std::string& statusMessage, bool& lastSuccess);
static bool BrowseInputFile(GuiState& state);
static bool BrowseOutputFile(GuiState& state);
static void SetDefaultOutputFromInput(GuiState& state);
static void CopyStringToBuffer(const std::string& value, std::array<char, MAX_PATH>& buffer);
static void ApplyBorderlessWindow(HWND hwnd, int width, int height);
static void ApplyCustomTheme(ImGuiStyle& style);

int run_gui() {
    WNDCLASSEX wc = {
        sizeof(WNDCLASSEX),
        CS_CLASSDC,
        WndProc,
        0L,
        0L,
        GetModuleHandle(nullptr),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        _T("pe-packer-gui"),
        nullptr
    };
    RegisterClassEx(&wc);

    HWND hwnd = CreateWindow(
        wc.lpszClassName,
        _T("pe-packer"),
        WS_POPUP,
        100, 100,
        kGuiWidth, kGuiHeight,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr);

    ApplyBorderlessWindow(hwnd, kGuiWidth, kGuiHeight);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return EXIT_FAILURE;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ApplyCustomTheme(ImGui::GetStyle());

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    GuiState state{};
    std::string statusMessage;
    bool lastSuccess = false;

    MSG msg{};
    bool done = false;
    while (!done) {
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                done = true;
            }
        }
        if (done) {
            break;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderGui(state, statusMessage, lastSuccess);

        ImGui::Render();
        ImVec4 windowColor = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        const float clear_color_with_alpha[4] = { windowColor.x, windowColor.y, windowColor.z, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClass(wc.lpszClassName, wc.hInstance);

    return EXIT_SUCCESS;
}

bool ExecutePacking(const GuiState& state, std::string& statusMessage) {
    std::string inputPath(state.input.data());
    std::string outputPath(state.output.data());
    std::string fpackStart(state.fpackStart.data());
    std::string fpackEnd(state.fpackEnd.data());

    if (inputPath.empty()) {
        statusMessage = "Input file path is required.";
        return false;
    }
    if (outputPath.empty()) {
        statusMessage = "Output file path is required.";
        return false;
    }
    if (state.mutationBase < 1) {
        statusMessage = "Mutations must be at least 1.";
        return false;
    }
    if (state.packFunctions && (fpackStart.empty() || fpackEnd.empty())) {
        statusMessage = "Provide both start and end addresses for -fpack.";
        return false;
    }

    std::vector<std::string> args;
    args.emplace_back("pe-packer");
    args.emplace_back(inputPath);
    args.emplace_back(outputPath);
    args.emplace_back(std::to_string(state.mutationBase));

    auto append_flag = [&args](bool enabled, const char* flag) {
        if (enabled) {
            args.emplace_back(flag);
        }
    };

    append_flag(state.removeAslr, "-noaslr");
    append_flag(state.obfuscateOep, "-oep_call");
    append_flag(state.antiDisasm, "-adasm");
    append_flag(state.mba, "-mba");
    append_flag(state.encryptSections, "-senc");
    append_flag(state.fakeInstructions, "-finstr");

    if (state.packFunctions) {
        args.emplace_back("-fpack");
        args.emplace_back(fpackStart);
        args.emplace_back(fpackEnd);
    }

    std::vector<char*> argv;
    argv.reserve(args.size());
    for (auto& arg : args) {
        argv.push_back(const_cast<char*>(arg.data()));
    }

    arguments::init(static_cast<int>(argv.size()), argv.data());

    try {
        const auto mutations = static_cast<uint32_t>(state.mutationBase * 10);
        print_info("Mutations count: %u\n", mutations);
        auto packer = std::make_unique<c_core>(inputPath, outputPath, mutations);
        packer->process();
        statusMessage = "Packing completed successfully.";
        return true;
    }
    catch (const std::exception& ex) {
        statusMessage = ex.what();
        return false;
    }
}

void RenderGui(GuiState& state, std::string& statusMessage, bool& lastSuccess) {
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus;

    if (ImGui::Begin("pe-packer GUI", nullptr, windowFlags)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.88f, 0.95f, 1.0f));
        ImGui::Text("pe-packer");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("| Version 1.0.1");
        ImGui::Spacing();

        const float browseWidth = 110.0f;
        const float minFieldWidth = 160.0f;
        ImGuiStyle& style = ImGui::GetStyle();

        auto fileInputRow = [&](const char* label, std::array<char, MAX_PATH>& buffer, auto&& browseFn) -> bool {
            ImGui::PushID(label);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::SameLine();
            float avail = ImGui::GetContentRegionAvail().x;
            float fieldWidth = avail - browseWidth - style.ItemSpacing.x;
            if (fieldWidth < minFieldWidth) {
                fieldWidth = minFieldWidth;
            }
            ImGui::SetNextItemWidth(fieldWidth);
            bool edited = ImGui::InputText("##path", buffer.data(), buffer.size());
            ImGui::SameLine();
            if (ImGui::Button("Browse", ImVec2(browseWidth, 0.f)) && browseFn()) {
                edited = true;
            }
            ImGui::PopID();
            return edited;
        };

        if (fileInputRow("Input file", state.input, [&]() { return BrowseInputFile(state); })) {
            SetDefaultOutputFromInput(state);
        }

        if (fileInputRow("Output file", state.output, [&]() { return BrowseOutputFile(state); })) {
            state.outputCustomized = true;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SliderInt("Mutations", &state.mutationBase, 1, 100);
        ImGui::SameLine();
        ImGui::TextDisabled("=> %d iterations", state.mutationBase * 10);

        ImGui::Spacing();
        ImGui::Text("Options");
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(6.f, 4.f));
        if (ImGui::BeginTable("OptionsFlags", 2, ImGuiTableFlags_SizingStretchProp)) {
            auto checkbox = [](const char* label, bool* value) {
                ImGui::TableNextColumn();
                ImGui::Checkbox(label, value);
            };
            checkbox("Remove ASLR", &state.removeAslr);
            checkbox("OEP call obfuscation", &state.obfuscateOep);
            checkbox("Anti-disassembly", &state.antiDisasm);
            checkbox("Mixed Boolean Arithmetic obfuscation", &state.mba);
            checkbox("Encrypt sections", &state.encryptSections);
            checkbox("Generate fake instructions", &state.fakeInstructions);
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();

        ImGui::Spacing();
        if (ImGui::Checkbox("Encrypt function", &state.packFunctions)) {
            if (!state.packFunctions) {
                state.fpackStart.fill(0);
                state.fpackEnd.fill(0);
            }
        }

        if (state.packFunctions) {
            ImGui::Indent();
            ImGui::InputText("Start address (hex)", state.fpackStart.data(), state.fpackStart.size());
            ImGui::InputText("End address (hex)", state.fpackEnd.data(), state.fpackEnd.size());
            ImGui::Unindent();
        }

        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float packWidth = 150.0f;
        const float exitWidth = 110.0f;
        float buttonSpacing = style.ItemSpacing.x;
        float availableWidth = ImGui::GetContentRegionAvail().x;
        float totalWidth = packWidth + exitWidth + buttonSpacing;
        float offset = (availableWidth - totalWidth) * 0.5f;
        if (offset < 0.0f) {
            offset = 0.0f;
        }
        float startX = ImGui::GetCursorPosX() + offset;
        ImGui::SetCursorPosX(startX);

        if (ImGui::Button("Pack", ImVec2(packWidth, 0.0f))) {
            lastSuccess = ExecutePacking(state, statusMessage);
            std::ostringstream line;
            SYSTEMTIME st{};
            GetLocalTime(&st);
            line << "[" << std::setfill('0') << std::setw(2) << st.wHour << ":"
                 << std::setfill('0') << std::setw(2) << st.wMinute << ":"
                 << std::setfill('0') << std::setw(2) << st.wSecond << "] "
                 << statusMessage;
            state.logEntries.emplace_back(line.str(), lastSuccess);
            if (state.logEntries.size() > 100) {
                state.logEntries.erase(state.logEntries.begin());
            }
        }
        ImGui::SameLine(0.0f, buttonSpacing);
        if (ImGui::Button("Exit", ImVec2(exitWidth, 0.0f))) {
            PostQuitMessage(0);
        }

        ImGui::Spacing();
        ImGui::Spacing();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 min = ImGui::GetCursorScreenPos();
        ImVec2 avail = ImGui::GetContentRegionAvail();
        ImVec2 max = ImVec2(min.x + avail.x, min.y + 160.0f);
        ImVec4 headerColor = ImVec4(0.16f, 0.18f, 0.22f, 0.95f);
        drawList->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(headerColor), 16.0f);
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::SetCursorScreenPos(ImVec2(min.x + 12.0f, min.y + 10.0f));
        ImGui::TextDisabled("Activity");
        ImGui::SetCursorScreenPos(ImVec2(min.x + 10.0f, min.y + 36.0f));

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
        if (ImGui::BeginChild("log_region", ImVec2(avail.x - 20.0f, 110.0f), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground)) {
            if (state.logEntries.empty()) {
                ImGui::TextDisabled("Logs will appear here.");
            } else {
                for (auto it = state.logEntries.rbegin(); it != state.logEntries.rend(); ++it) {
                    ImVec4 color = it->second ? ImVec4(0.45f, 0.82f, 0.53f, 1.0f) : ImVec4(0.91f, 0.48f, 0.40f, 1.0f);
                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextWrapped("%s", it->first.c_str());
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::End();
}

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevelArray,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &g_pSwapChain,
        &g_pd3dDevice,
        &featureLevel,
        &g_pd3dDeviceContext);

    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createDeviceFlags,
            featureLevelArray,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &g_pSwapChain,
            &g_pd3dDevice,
            &featureLevel,
            &g_pd3dDeviceContext);
    }

    if (FAILED(res)) {
        return false;
    }

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    if (g_pSwapChain && SUCCEEDED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer)))) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }

    switch (msg) {
    case WM_NCHITTEST: {
        POINT cursor{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &cursor);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        if (cursor.y >= rc.top && cursor.y < rc.top + 32) {
            return HTCAPTION;
        }
        return HTCLIENT;
    }
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        if (const RECT* const prcNewWindow = reinterpret_cast<RECT*>(lParam)) {
            SetWindowPos(hWnd, nullptr,
                prcNewWindow->left,
                prcNewWindow->top,
                prcNewWindow->right - prcNewWindow->left,
                prcNewWindow->bottom - prcNewWindow->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        return 0;
    default:
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

std::string BuildDefaultOutputPath(const std::string& input) {
    if (input.empty()) {
        return {};
    }

    std::filesystem::path inputPath(input);
    auto parent = inputPath.parent_path();
    std::string stem = inputPath.stem().string();
    if (stem.empty()) {
        stem = "packed";
    }

    std::filesystem::path outName = stem + "_packed.exe";
    if (!parent.empty()) {
        outName = parent / outName;
    }
    return outName.string();
}

void CopyStringToBuffer(const std::string& value, std::array<char, MAX_PATH>& buffer) {
    strncpy_s(buffer.data(), buffer.size(), value.c_str(), buffer.size() - 1);
}

void SetDefaultOutputFromInput(GuiState& state) {
    if (state.outputCustomized) {
        return;
    }

    std::string defaultPath = BuildDefaultOutputPath(state.input.data());
    if (!defaultPath.empty()) {
        CopyStringToBuffer(defaultPath, state.output);
    }
}

bool BrowseInputFile(GuiState& state) {
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetForegroundWindow();
    ofn.lpstrFile = state.input.data();
    ofn.nMaxFile = static_cast<DWORD>(state.input.size());
    ofn.lpstrFilter = "Executable Files (*.exe)\0*.exe\0All Files (*.*)\0*.*\0\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn) == TRUE) {
        state.outputCustomized = false;
        SetDefaultOutputFromInput(state);
        return true;
    }
    return false;
}

bool BrowseOutputFile(GuiState& state) {
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetForegroundWindow();
    ofn.lpstrFile = state.output.data();
    ofn.nMaxFile = static_cast<DWORD>(state.output.size());
    ofn.lpstrFilter = "Executable Files (*.exe)\0*.exe\0All Files (*.*)\0*.*\0\0";
    ofn.lpstrDefExt = "exe";
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn) == TRUE) {
        state.outputCustomized = true;
        return true;
    }
    return false;
}

void ApplyBorderlessWindow(HWND hwnd, int width, int height) {
    if (!hwnd) {
        return;
    }

    RECT rect{};
    GetWindowRect(hwnd, &rect);

    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    style |= WS_POPUP;
    SetWindowLong(hwnd, GWL_STYLE, style);

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_CLIENTEDGE | WS_EX_DLGMODALFRAME);
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

    SetWindowPos(
        hwnd,
        nullptr,
        rect.left,
        rect.top,
        width,
        height,
        SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);

    const int radius = 24;
    HRGN region = CreateRoundRectRgn(0, 0, width + 1, height + 1, radius, radius);
    if (region) {
        SetWindowRgn(hwnd, region, TRUE);
    }

#if defined(DWMWA_WINDOW_CORNER_PREFERENCE)
    const DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
#endif
}

// Some styles xD
void ApplyCustomTheme(ImGuiStyle& style) {
    style.WindowPadding = ImVec2(20.0f, 16.0f);
    style.FramePadding = ImVec2(12.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.WindowRounding = 20.0f;
    style.FrameRounding = 10.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 10.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    style.ChildBorderSize = 0.0f;

    ImVec4 bg = ImVec4(0.11f, 0.13f, 0.17f, 1.0f);
    ImVec4 panel = ImVec4(0.16f, 0.18f, 0.24f, 1.0f);
    ImVec4 accent = ImVec4(0.29f, 0.47f, 0.83f, 1.0f);
    ImVec4 accentHover = ImVec4(0.36f, 0.55f, 0.91f, 1.0f);
    ImVec4 accentActive = ImVec4(0.24f, 0.40f, 0.75f, 1.0f);

    style.Colors[ImGuiCol_WindowBg] = bg;
    style.Colors[ImGuiCol_ChildBg] = ImVec4(bg.x + 0.01f, bg.y + 0.01f, bg.z + 0.02f, 1.0f);
    style.Colors[ImGuiCol_PopupBg] = panel;
    style.Colors[ImGuiCol_FrameBg] = panel;
    style.Colors[ImGuiCol_FrameBgHovered] = accentHover;
    style.Colors[ImGuiCol_FrameBgActive] = accentActive;
    style.Colors[ImGuiCol_Button] = accent;
    style.Colors[ImGuiCol_ButtonHovered] = accentHover;
    style.Colors[ImGuiCol_ButtonActive] = accentActive;
    style.Colors[ImGuiCol_Header] = accent;
    style.Colors[ImGuiCol_HeaderHovered] = accentHover;
    style.Colors[ImGuiCol_HeaderActive] = accentActive;
    style.Colors[ImGuiCol_CheckMark] = ImVec4(0.92f, 0.95f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(0.88f, 0.92f, 1.0f, 1.0f);
    style.Colors[ImGuiCol_SliderGrabActive] = accentActive;
    style.Colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.27f, 0.32f, 1.0f);
    style.Colors[ImGuiCol_Text] = ImVec4(0.90f, 0.94f, 0.98f, 1.0f);
    style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.64f, 0.70f, 1.0f);
}