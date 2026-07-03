#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#pragma execution_character_set("utf-8")
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <sodium.h>
#include <cstdint>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cmath>
#include "core/SecureBuffer.h"
#include "core/EntropyAccumulator.h"
#include "core/StatelessGenerator.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
static HWND                     g_hWnd = nullptr;
static float                    g_dpiScale = 1.0f;
static float                    g_baseFontSize = 16.0f;
static bool                     g_d3d11Ready = false;

static constexpr UINT CLIPBOARD_CLEAR_MS = 15000;
static bool g_clipboardActive = false;
static std::thread g_clipboardTimer;

static void createRenderTarget()
{
    ID3D11Texture2D* backBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    g_pd3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_mainRenderTargetView);
    backBuffer->Release();
}

static void cleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

static bool createDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL arr[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
        nullptr, createFlags, arr, 2, D3D11_SDK_VERSION, &sd,
        &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (FAILED(hr)) return false;
    createRenderTarget();
    return true;
}

static void cleanupDeviceD3D()
{
    cleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

static void applyTheme(float scale)
{
    ImGuiStyle& s = ImGui::GetStyle();

    s.WindowRounding    = 12.0f * scale;
    s.ChildRounding     = 8.0f  * scale;
    s.FrameRounding     = 6.0f  * scale;
    s.PopupRounding     = 8.0f  * scale;
    s.GrabRounding      = 4.0f  * scale;
    s.ScrollbarRounding = 6.0f  * scale;
    s.TabRounding       = 6.0f  * scale;

    s.WindowBorderSize  = 0.0f;
    s.ChildBorderSize   = 1.0f;
    s.PopupBorderSize   = 0.0f;
    s.FrameBorderSize   = 0.0f;

    s.WindowPadding     = ImVec2(12 * scale, 12 * scale);
    s.FramePadding      = ImVec2(10 * scale, 7 * scale);
    s.ItemSpacing       = ImVec2(8 * scale, 6 * scale);
    s.ItemInnerSpacing  = ImVec2(6 * scale, 4 * scale);
    s.ScrollbarSize     = 14.0f * scale;
    s.GrabMinSize       = 10.0f * scale;
    s.IndentSpacing     = 20.0f * scale;
    s.ColumnsMinSpacing = 6.0f  * scale;

    ImVec4* c = s.Colors;

    // ── Backgrounds ──
    c[ImGuiCol_WindowBg]          = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    c[ImGuiCol_ChildBg]           = ImVec4(0.95f, 0.96f, 0.97f, 1.00f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    c[ImGuiCol_Border]            = ImVec4(0.88f, 0.88f, 0.92f, 1.00f);

    // ── Text ──
    c[ImGuiCol_Text]              = ImVec4(0.07f, 0.09f, 0.16f, 1.00f);
    c[ImGuiCol_TextDisabled]      = ImVec4(0.61f, 0.64f, 0.69f, 1.00f);
    c[ImGuiCol_TextSelectedBg]    = ImVec4(0.49f, 0.23f, 0.93f, 0.20f);

    // ── Headers ──
    c[ImGuiCol_Header]            = ImVec4(0.96f, 0.95f, 1.00f, 1.00f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(0.87f, 0.84f, 1.00f, 1.00f);
    c[ImGuiCol_HeaderActive]      = ImVec4(0.77f, 0.71f, 0.99f, 1.00f);

    // ── Buttons (pastel purple → solid purple on interaction) ──
    c[ImGuiCol_Button]            = ImVec4(0.96f, 0.95f, 1.00f, 1.00f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.87f, 0.84f, 1.00f, 1.00f);
    c[ImGuiCol_ButtonActive]      = ImVec4(0.49f, 0.23f, 0.93f, 1.00f);

    // ── Frames (inputs, combo, etc) ──
    c[ImGuiCol_FrameBg]           = ImVec4(0.97f, 0.98f, 0.99f, 1.00f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.95f, 0.91f, 1.00f, 1.00f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

    // ── Accent: purple ──
    c[ImGuiCol_CheckMark]         = ImVec4(0.49f, 0.23f, 0.93f, 1.00f);
    c[ImGuiCol_SliderGrab]        = ImVec4(0.43f, 0.18f, 0.85f, 1.00f);
    c[ImGuiCol_SliderGrabActive]  = ImVec4(0.49f, 0.23f, 0.93f, 1.00f);

    // ── Title ──
    c[ImGuiCol_TitleBg]           = ImVec4(0.95f, 0.96f, 0.97f, 1.00f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.95f, 0.96f, 0.97f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]  = ImVec4(0.95f, 0.96f, 0.97f, 1.00f);

    // ── Scrollbar ──
    c[ImGuiCol_ScrollbarBg]       = ImVec4(0.95f, 0.96f, 0.97f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]     = ImVec4(0.82f, 0.82f, 0.88f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.72f, 0.72f, 0.78f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.62f, 0.62f, 0.68f, 1.00f);

    // ── Separator / Resize ──
    c[ImGuiCol_Separator]         = ImVec4(0.88f, 0.88f, 0.92f, 1.00f);
    c[ImGuiCol_ResizeGrip]        = ImVec4(0.49f, 0.23f, 0.93f, 0.20f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.49f, 0.23f, 0.93f, 0.40f);
    c[ImGuiCol_ResizeGripActive]  = ImVec4(0.49f, 0.23f, 0.93f, 0.60f);

    // ── Progress / Plot ──
    c[ImGuiCol_PlotHistogram]     = ImVec4(0.49f, 0.23f, 0.93f, 1.00f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.55f, 0.30f, 0.96f, 1.00f);

    // ── Tabs ──
    c[ImGuiCol_Tab]               = ImVec4(0.95f, 0.96f, 0.97f, 1.00f);
    c[ImGuiCol_TabHovered]        = ImVec4(0.87f, 0.84f, 1.00f, 1.00f);
    c[ImGuiCol_TabActive]         = ImVec4(0.49f, 0.23f, 0.93f, 0.15f);
    c[ImGuiCol_TabUnfocused]      = ImVec4(0.95f, 0.96f, 0.97f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.49f, 0.23f, 0.93f, 0.10f);
}

static void reloadFont(float dpiScale)
{
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    float fontSize = g_baseFontSize * dpiScale;

    ImFontConfig cfg;
    cfg.MergeMode = false;
    ImFont* font = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\segoeui.ttf", fontSize, &cfg,
        io.Fonts->GetGlyphRangesCyrillic());
    if (!font) {
        font = io.Fonts->AddFontDefault();
    }

    static const ImWchar emoji_ranges[] = {
        0x2728, 0x2728, // ✨ Sparkles
        0x26A1, 0x26A1, // ⚡ High Voltage
        0x26A0, 0x26A0, // ⚠ Warning
        0x2705, 0x2705, // ✅ Check mark
        0x274C, 0x274C, // ❌ Cross mark
        0x1F300, 0x1F30A, // 🌊 + adjacent
        0x1F510, 0x1F512, // 🔐 🔑 🔒
        0x1F524, 0x1F524, // 🔤 Input Latin
        0x1F4CB, 0x1F4CB, // 📋 Clipboard
        0x1F5B1, 0x1F5B1, // 🖱 Mouse
        0
    };
    cfg.MergeMode = true;
    cfg.GlyphMinAdvanceX = fontSize;
    if (!io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguiemj.ttf", fontSize, &cfg, emoji_ranges)) {
        // fallback: try Segoe UI Symbol for partial coverage
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\seguisym.ttf", fontSize, &cfg, emoji_ranges);
    }

    io.Fonts->Build();
    if (g_d3d11Ready) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        ImGui_ImplDX11_CreateDeviceObjects();
    }
}

static void handleDpiChange(HWND hWnd, float newDpiScale, RECT* newRect)
{
    g_dpiScale = newDpiScale;
    applyTheme(g_dpiScale);
    reloadFont(g_dpiScale);
    if (newRect) {
        SetWindowPos(hWnd, nullptr, newRect->left, newRect->top,
            newRect->right - newRect->left, newRect->bottom - newRect->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            cleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            createRenderTarget();
        }
        return 0;
    case WM_DPICHANGED: {
        float newScale = HIWORD(wParam) / 96.0f;
        RECT* rect = reinterpret_cast<RECT*>(lParam);
        handleDpiChange(hWnd, newScale, rect);
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void clipboardClearAfter(const std::string&)
{
    if (g_clipboardTimer.joinable()) g_clipboardTimer.detach();
    g_clipboardActive = true;
    g_clipboardTimer = std::thread([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(CLIPBOARD_CLEAR_MS));
        if (OpenClipboard(nullptr)) { EmptyClipboard(); CloseClipboard(); }
        g_clipboardActive = false;
    });
    g_clipboardTimer.detach();
}

static double calcEntropy(const std::string& password, size_t poolSize)
{
    if (poolSize == 0) return 0.0;
    return password.size() * (log(static_cast<double>(poolSize)) / log(2.0));
}

static std::string buildAlphabet(bool lower, bool upper, bool digits, bool special, const char* exclude)
{
    std::string a;
    if (lower)   a += "abcdefghijklmnopqrstuvwxyz";
    if (upper)   a += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (digits)  a += "0123456789";
    if (special) a += "!@#$%^&*()_+-=[]{}|;':\",./<>?";
    for (const char* p = exclude; p && *p; ++p) {
        size_t pos;
        while ((pos = a.find(*p)) != std::string::npos) a.erase(pos, 1);
    }
    return a;
}

static size_t countPoolSize(bool lower, bool upper, bool digits, bool special, const char* exclude)
{
    size_t s = 0;
    if (lower)   s += 26;
    if (upper)   s += 26;
    if (digits)  s += 10;
    if (special) s += 31;
    for (const char* p = exclude; p && *p; ++p)
        if (s > 0) s--;
    return s;
}

static void generateRandomPassword(char* buf, size_t bufSize, int length, const std::string& alphabet)
{
    if (alphabet.empty() || length <= 0) { buf[0] = 0; return; }
    std::string pwd(static_cast<size_t>(length), '\0');
    randombytes_buf(pwd.data(), pwd.size());
    for (int i = 0; i < length; i++)
        pwd[i] = alphabet[static_cast<unsigned char>(pwd[i]) % alphabet.size()];
    strncpy_s(buf, bufSize, pwd.c_str(), _TRUNCATE);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    if (sodium_init() < 0) {
        MessageBoxA(nullptr, "Failed to initialize libsodium", "Error", MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    wc.hIconSm = LoadIconW(hInst, MAKEINTRESOURCEW(101));
    wc.lpszClassName = L"PwGenClass";
    RegisterClassExW(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    g_hWnd = CreateWindowExW(0, wc.lpszClassName, L"Генератор Паролей",
        WS_OVERLAPPEDWINDOW, (sw - 960) / 2, (sh - 720) / 2, 960, 720,
        nullptr, nullptr, wc.hInstance, nullptr);
    if (!createDeviceD3D(g_hWnd)) { cleanupDeviceD3D(); UnregisterClassW(wc.lpszClassName, wc.hInstance); return 1; }
    ShowWindow(g_hWnd, SW_SHOWDEFAULT); UpdateWindow(g_hWnd);

    g_dpiScale = GetDpiForWindow(g_hWnd) / 96.0f;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = "config.ini";

    applyTheme(g_dpiScale);
    reloadFont(g_dpiScale);

    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    g_d3d11Ready = true;
    reloadFont(g_dpiScale); // rebuild D3D font resources now that backend is ready

    // ── State ──
    int activeMethod = 0;
    char passwordBuf[4096] = "";
    double entropyVal = 0.0;
    bool copied = false;

    int     maskLen = 32;       bool maskLower = true, maskUpper = true, maskDigits = true, maskSpecial = false;
    char    maskExclude[128] = "", maskCustom[256] = "";

    char    detMaster[256] = "", detDomain[256] = "", detLogin[256] = "";
    int     detLen = 32;        bool detLower = true, detUpper = true, detDigits = true, detSpecial = false;
    char    detExclude[128] = "";

    int     phonLen = 16;       bool phonDigits = true, phonSpecial = false;

    EntropyAccumulator* entropyAcc = new EntropyAccumulator();
    int     noiseLen = 32;      bool noiseLower = true, noiseUpper = true, noiseDigits = true, noiseSpecial = false;
    char    noiseExclude[128] = "";
    bool    noiseReady = false;

    const char* vowels = "aeiouy";
    const char* consonants = "bcdfghjklmnpqrstvwxz";

    MSG msg; bool done = false;
    while (!done) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) done = true;
            TranslateMessage(&msg); DispatchMessageA(&msg);
        }
        if (done) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##main", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        // ── Header gradient ──
        ImGui::BeginChild("header", ImVec2(0, 56 * g_dpiScale), false);
        ImVec2 wSize = ImGui::GetWindowSize();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        dl->AddRectFilledMultiColor(p0, ImVec2(p0.x + wSize.x, p0.y + 56 * g_dpiScale),
            IM_COL32(249, 250, 251, 255), IM_COL32(245, 243, 255, 255),
            IM_COL32(245, 243, 255, 255), IM_COL32(249, 250, 251, 255));
        ImGui::SetCursorPos(ImVec2(20 * g_dpiScale, 12 * g_dpiScale));
        ImGui::Text("\xF0\x9F\x94\x92");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.49f, 0.23f, 0.93f, 1), "Генератор Паролей");
        ImGui::EndChild();

        // ── Tabs ──
        ImGui::BeginChild("tabs", ImVec2(0, 48 * g_dpiScale), false);
        const char* tabs[] = { "\xE2\x9C\xA8  Маски", "\xF0\x9F\x94\x90  Детерминир.", "\xF0\x9F\x94\xA4  Фонетика", "\xF0\x9F\x8C\x8A  Шум" };
        for (int i = 0; i < 4; i++) {
            if (i > 0) ImGui::SameLine(0, 4 * g_dpiScale);
            bool act = (activeMethod == i);
            ImVec4 btnCol = act ? ImVec4(0.49f, 0.23f, 0.93f, 0.15f) : ImVec4(0.95f, 0.96f, 0.97f, 1);
            ImVec4 btnHov = act ? ImVec4(0.49f, 0.23f, 0.93f, 0.25f) : ImVec4(0.87f, 0.84f, 1.00f, 1);
            ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHov);
            float tw = ImGui::CalcTextSize(tabs[i]).x + 24 * g_dpiScale;
            if (ImGui::Button(tabs[i], ImVec2(tw, 36 * g_dpiScale))) { activeMethod = i; }
            ImGui::PopStyleColor(2);
        }
        ImGui::EndChild();

        // ── Content ──
        ImGui::BeginChild("content", ImVec2(0, -100 * g_dpiScale), true);

        if (activeMethod == 0) {
            ImGui::TextColored(ImVec4(0.49f, 0.23f, 0.93f, 1), "Настройки маски");
            ImGui::Separator();
            if (ImGui::BeginTable("m0grid", 2, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed, 140.0f * g_dpiScale);
                ImGui::TableSetupColumn("ctl", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Длина пароля");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::SliderInt("##m0len", &maskLen, 8, 128);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Строчные (a-z) ?l");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m0l", &maskLower);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Заглавные (A-Z) ?u");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m0u", &maskUpper);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Цифры (0-9) ?d");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m0d", &maskDigits);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Спецсимволы ?s");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m0s", &maskSpecial);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Исключить");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::InputText("##m0exc", maskExclude, sizeof(maskExclude));
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Маска");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::InputText("##m0mask", maskCustom, sizeof(maskCustom));
                ImGui::EndTable();
            }
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 6 * g_dpiScale));
            if (ImGui::Button("\xE2\x9A\xA1  Сгенерировать", ImVec2(280 * g_dpiScale, 44 * g_dpiScale))) {
                std::string a = buildAlphabet(maskLower, maskUpper, maskDigits, maskSpecial, maskExclude);
                if (!a.empty()) {
                    generateRandomPassword(passwordBuf, sizeof(passwordBuf), maskLen, a);
                    entropyVal = calcEntropy(passwordBuf, countPoolSize(maskLower, maskUpper, maskDigits, maskSpecial, maskExclude));
                    copied = false;
                }
            }
        } else if (activeMethod == 1) {
            ImGui::TextColored(ImVec4(0.49f, 0.23f, 0.93f, 1), "Детерминированная генерация");
            ImGui::Separator();
            if (ImGui::BeginTable("m1grid", 2, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed, 140.0f * g_dpiScale);
                ImGui::TableSetupColumn("ctl", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Мастер-пароль");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::InputText("##m1mp", detMaster, sizeof(detMaster), ImGuiInputTextFlags_Password);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Домен");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::InputText("##m1dom", detDomain, sizeof(detDomain));
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Логин");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::InputText("##m1log", detLogin, sizeof(detLogin));
                ImGui::EndTable();
            }
            ImGui::Separator();
            if (ImGui::BeginTable("m1grid2", 2, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed, 140.0f * g_dpiScale);
                ImGui::TableSetupColumn("ctl", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Длина");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::SliderInt("##m1len", &detLen, 8, 128);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("a-z");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m1l", &detLower);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("A-Z");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m1u", &detUpper);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("0-9");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m1d", &detDigits);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("!@#");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m1s", &detSpecial);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Исключить");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::InputText("##m1exc", detExclude, sizeof(detExclude));
                ImGui::EndTable();
            }
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 6 * g_dpiScale));
            if (ImGui::Button("\xF0\x9F\x94\x90  Сгенерировать", ImVec2(280 * g_dpiScale, 44 * g_dpiScale))) {
                std::string a = buildAlphabet(detLower, detUpper, detDigits, detSpecial, detExclude);
                if (!a.empty() && strlen(detMaster) > 0 && strlen(detDomain) > 0) {
                    StatelessGenerator gen;
                    std::string pwd = gen.generate(detMaster, detDomain, detLogin, static_cast<size_t>(detLen), a);
                    strncpy_s(passwordBuf, pwd.c_str(), _TRUNCATE);
                    entropyVal = calcEntropy(passwordBuf, countPoolSize(detLower, detUpper, detDigits, detSpecial, detExclude));
                    copied = false;
                }
            }
        } else if (activeMethod == 2) {
            ImGui::TextColored(ImVec4(0.49f, 0.23f, 0.93f, 1), "Фонетическая генерация");
            ImGui::Separator();
            ImGui::TextWrapped("Пароль строится на чередовании гласных и согласных — легко читается и запоминается.");
            ImGui::Separator();
            if (ImGui::BeginTable("m2grid", 2, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed, 140.0f * g_dpiScale);
                ImGui::TableSetupColumn("ctl", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Длина");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::SliderInt("##m2len", &phonLen, 8, 64);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Цифры");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m2d", &phonDigits);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Спецсимволы");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m2s", &phonSpecial);
                ImGui::EndTable();
            }
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 6 * g_dpiScale));
            if (ImGui::Button("\xF0\x9F\x94\xA4  Сгенерировать", ImVec2(280 * g_dpiScale, 44 * g_dpiScale))) {
                std::string pwd;
                pwd.reserve(static_cast<size_t>(phonLen) + 4);
                bool lastVowel = false; unsigned char r;
                randombytes_buf(&r, 1); lastVowel = r % 2 == 0;
                for (int i = 0; i < phonLen; i++) {
                    const char* set = lastVowel ? consonants : vowels;
                    size_t sl = lastVowel ? strlen(consonants) : strlen(vowels);
                    randombytes_buf(&r, 1);
                    pwd += set[r % sl];
                    lastVowel = !lastVowel;
                }
                if (phonLen > 2) {
                    randombytes_buf(&r, 1);
                    size_t posP = r % static_cast<size_t>(phonLen - 1);
                    pwd[posP] = static_cast<char>(toupper(pwd[posP]));
                }
                if (phonDigits && phonLen > 3) {
                    randombytes_buf(&r, 1);
                    size_t posD = 1 + (r % static_cast<size_t>(phonLen - 2));
                    pwd.insert(pwd.begin() + static_cast<ptrdiff_t>(posD), '0' + (r % 10));
                    if (static_cast<int>(pwd.size()) > phonLen) pwd.resize(static_cast<size_t>(phonLen));
                }
                strncpy_s(passwordBuf, pwd.c_str(), _TRUNCATE);
                entropyVal = calcEntropy(passwordBuf, 36);
                copied = false;
            }
        } else if (activeMethod == 3) {
            ImGui::TextColored(ImVec4(0.49f, 0.23f, 0.93f, 1), "Сбор энтропии");
            ImGui::Separator();
            ImGui::TextWrapped("\xF0\x9F\x96\xB1  Подвигайте мышкой, чтобы накопить случайные данные для генерации.");
            ImGui::Separator();

            ImVec2 mPos = io.MousePos;
            if (ImGui::IsWindowHovered() && io.MouseDelta.x != 0 && io.MouseDelta.y != 0)
                noiseReady = entropyAcc->addEvent(static_cast<int>(mPos.x), static_cast<int>(mPos.y));

            float pct = entropyAcc->getProgress();
            float prog = (std::min)(1.0f, pct / 100.0f);
            char pbarStr[64];
            snprintf(pbarStr, sizeof(pbarStr), "  %.0f / %d байт",
                pct * EntropyAccumulator::REQUIRED_ENTROPY_BYTES / 100.0f,
                static_cast<int>(EntropyAccumulator::REQUIRED_ENTROPY_BYTES));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.49f, 0.23f, 0.93f, 1));
            ImGui::ProgressBar(prog, ImVec2(-1, 28 * g_dpiScale), pbarStr);
            ImGui::PopStyleColor();

            ImGui::Separator();
            if (ImGui::BeginTable("m3grid", 2, ImGuiTableFlags_SizingFixedFit)) {
                ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthFixed, 140.0f * g_dpiScale);
                ImGui::TableSetupColumn("ctl", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Длина");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::SliderInt("##m3len", &noiseLen, 8, 128);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("a-z");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m3l", &noiseLower);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("A-Z");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m3u", &noiseUpper);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("0-9");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m3d", &noiseDigits);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("!@#");
                ImGui::TableSetColumnIndex(1); ImGui::Checkbox("##m3s", &noiseSpecial);
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Исключить");
                ImGui::TableSetColumnIndex(1); ImGui::SetNextItemWidth(-1); ImGui::InputText("##m3exc", noiseExclude, sizeof(noiseExclude));
                ImGui::EndTable();
            }
            ImGui::Separator();
            ImGui::Dummy(ImVec2(0, 6 * g_dpiScale));
            if (!noiseReady) {
                ImGui::BeginDisabled();
                ImGui::Button("\xF0\x9F\x8C\x8A  Соберите энтропию...", ImVec2(320 * g_dpiScale, 44 * g_dpiScale));
                ImGui::EndDisabled();
            } else {
                if (ImGui::Button("\xF0\x9F\x8C\x8A  Сгенерировать из шума", ImVec2(320 * g_dpiScale, 44 * g_dpiScale))) {
                    SecureBuffer seed = entropyAcc->getFinalSeed();
                    std::string a = buildAlphabet(noiseLower, noiseUpper, noiseDigits, noiseSpecial, noiseExclude);
                    if (!a.empty() && seed.size() >= 32) {
                        std::string pwd(static_cast<size_t>(noiseLen), '\0');
                        randombytes_buf_deterministic(pwd.data(), pwd.size(), seed.data());
                        for (int i = 0; i < noiseLen; i++)
                            pwd[i] = a[static_cast<unsigned char>(pwd[i]) % a.size()];
                        strncpy_s(passwordBuf, pwd.c_str(), _TRUNCATE);
                        entropyVal = calcEntropy(passwordBuf, countPoolSize(noiseLower, noiseUpper, noiseDigits, noiseSpecial, noiseExclude));
                        copied = false;
                    }
                    delete entropyAcc; entropyAcc = new EntropyAccumulator(); noiseReady = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("\xE2\x9D\x8C  Сбросить", ImVec2(120 * g_dpiScale, 44 * g_dpiScale))) {
                    delete entropyAcc; entropyAcc = new EntropyAccumulator(); noiseReady = false;
                }
            }
        }

        ImGui::EndChild(); // content

        // ── Bottom output ──
        ImGui::BeginChild("output", ImVec2(0, 0), true);

        ImVec4 pwdColor = ImVec4(0, 0.6f, 0.3f, 1);
        if (entropyVal > 0) {
            if (entropyVal < 50)      pwdColor = ImVec4(0.85f, 0.25f, 0.25f, 1);
            else if (entropyVal < 80) pwdColor = ImVec4(0.85f, 0.65f, 0.10f, 1);
            else                      pwdColor = ImVec4(0.49f, 0.23f, 0.93f, 1);
        }

        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.97f, 0.98f, 0.99f, 1));
        ImGui::PushStyleColor(ImGuiCol_Text, pwdColor);
        ImGui::InputTextMultiline("##pwd", passwordBuf, sizeof(passwordBuf),
            ImVec2(-1, 56 * g_dpiScale), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor(2);

        if (entropyVal > 0) {
            ImGui::Text("Энтропия:"); ImGui::SameLine(100 * g_dpiScale);
            char eStr[32]; snprintf(eStr, sizeof(eStr), "%.1f бит", entropyVal);
            ImGui::TextColored(pwdColor, "%s", eStr);

            ImGui::SameLine(220 * g_dpiScale);
            float eFrac = static_cast<float>((std::min)(1.0, entropyVal / 128.0));
            ImVec4 eColor = entropyVal < 50 ? ImVec4(0.85f, 0.25f, 0.25f, 1) :
                            (entropyVal < 80 ? ImVec4(0.85f, 0.65f, 0.10f, 1) : ImVec4(0.49f, 0.23f, 0.93f, 1));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, eColor);
            ImGui::ProgressBar(eFrac, ImVec2(200 * g_dpiScale, 16 * g_dpiScale),
                entropyVal < 50 ? "Слабый" : (entropyVal < 80 ? "Средний" : "Надёжный"));
            ImGui::PopStyleColor();
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 250 * g_dpiScale);
        if (ImGui::Button("\xF0\x9F\x93\x8B  Копировать", ImVec2(140 * g_dpiScale, 32 * g_dpiScale))) {
            if (strlen(passwordBuf) > 0) {
                if (OpenClipboard(nullptr)) {
                    EmptyClipboard();
                    size_t len = strlen(passwordBuf) + 1;
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
                    if (hMem) { memcpy(GlobalLock(hMem), passwordBuf, len); GlobalUnlock(hMem); SetClipboardData(CF_TEXT, hMem); }
                    CloseClipboard();
                }
                clipboardClearAfter(passwordBuf);
                copied = true;
            }
        }
        if (copied) {
            ImGui::SameLine();
            ImGui::TextColored(g_clipboardActive ? ImVec4(0.49f, 0.23f, 0.93f, 1) : ImVec4(0.85f, 0.65f, 0.10f, 1),
                g_clipboardActive ? "\xE2\x9C\x85  Очистится через 15с" : "\xE2\x9A\xA0  Буфер очищен");
        }

        ImGui::EndChild(); // output
        ImGui::End(); // main

        // Render
        ImGui::Render();
        const float clearColor[4] = { 0.95f, 0.96f, 0.97f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    delete entropyAcc;
    ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext();
    cleanupDeviceD3D(); DestroyWindow(g_hWnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
