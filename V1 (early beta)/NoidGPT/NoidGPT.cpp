// NoidGPT.cpp : Lokale AI-chat (Ollama + Qwen) in pure Win32
#include "framework.h"
#include "NoidGPT.h"
#include "OllamaManager.h"

#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include <utility>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#define MAX_LOADSTRING 100

#define WM_APP_STATUS   (WM_APP + 1)  // lParam = std::wstring*
#define WM_APP_PROGRESS (WM_APP + 2)  // wParam = percentage (-1 = verberg)
#define WM_APP_TOKEN    (WM_APP + 3)  // lParam = std::wstring*
#define WM_APP_CHATDONE (WM_APP + 4)  // wParam = ok, lParam = std::wstring*
#define WM_APP_READY    (WM_APP + 5)  // wParam = ok

// Kleuren (donker thema)
static const COLORREF kBg      = RGB(32, 33, 35);
static const COLORREF kPanel   = RGB(48, 49, 54);
static const COLORREF kBorder  = RGB(69, 70, 77);
static const COLORREF kText    = RGB(236, 236, 241);
static const COLORREF kMuted   = RGB(160, 160, 172);
static const COLORREF kAccent  = RGB(88, 101, 242);
static const COLORREF kAccentH = RGB(71, 82, 213);
static const COLORREF kUser    = RGB(140, 158, 255);
static const COLORREF kBot     = RGB(80, 200, 150);
static const COLORREF kError   = RGB(240, 100, 100);

// Globals
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];

static HWND g_hMainWindow, g_hTitle, g_hStatus, g_hProgress, g_hChat, g_hInput, g_hSend, g_hNew;
static HWND g_hModel, g_hEffort, g_hLanguage, g_hWebSearch;
static HFONT g_fontUi, g_fontTitle, g_fontChat;
static HBRUSH g_brBg, g_brPanel;
static int g_dpi = 96;
static bool g_ready = false;
static bool g_busy = false;
static bool g_assistantOpen = false; // staat de cursor in een lopend antwoord?
static std::unique_ptr<OllamaManager> g_ollama;


ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK    InputProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

static int S(int v) { return MulDiv(v, g_dpi, 96); }

static void Post(HWND hwnd, UINT msg, WPARAM w, const std::wstring* s)
{
    PostMessageW(hwnd, msg, w, (LPARAM)(s ? new std::wstring(*s) : nullptr));
}


static std::string ToUtf8(const std::wstring& w)
{
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

// ---------------------------------------------------------------------------
// Chat-weergave (RichEdit)
// ---------------------------------------------------------------------------
static void AppendChat(const std::wstring& text, COLORREF color, bool bold)
{
    std::wstring t;
    t.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i)
    {
        if (text[i] == L'\r') continue;
        t += (text[i] == L'\n') ? L'\r' : text[i];
    }

    // Alleen automatisch scrollen als de gebruiker al onderaan zit
    SCROLLINFO si{ sizeof(si), SIF_ALL };
    GetScrollInfo(g_hChat, SB_VERT, &si);
    bool atBottom = (si.nPos + (int)si.nPage + 4 >= si.nMax);

    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_BOLD;
    cf.crTextColor = color;
    cf.dwEffects = bold ? CFE_BOLD : 0;

    SendMessageW(g_hChat, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageW(g_hChat, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(g_hChat, EM_REPLACESEL, FALSE, (LPARAM)t.c_str());
    if (atBottom) SendMessageW(g_hChat, WM_VSCROLL, SB_BOTTOM, 0);
}

static void UpdateControls()
{
    EnableWindow(g_hSend, g_ready || g_busy);
    EnableWindow(g_hInput, g_ready);
    EnableWindow(g_hNew, g_ready && !g_busy);
    SetWindowTextW(g_hSend, g_busy ? L"Stop" : L"Versturen");
    InvalidateRect(g_hSend, nullptr, TRUE);
}

static void ApplyFonts()
{
    if (g_fontUi) DeleteObject(g_fontUi);
    if (g_fontTitle) DeleteObject(g_fontTitle);
    if (g_fontChat) DeleteObject(g_fontChat);
    g_fontUi    = CreateFontW(-MulDiv(10, g_dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontChat  = CreateFontW(-MulDiv(11, g_dpi, 72), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontTitle = CreateFontW(-MulDiv(20, g_dpi, 72), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    SendMessageW(g_hTitle, WM_SETFONT, (WPARAM)g_fontTitle, TRUE);
    SendMessageW(g_hStatus, WM_SETFONT, (WPARAM)g_fontUi, TRUE);
    SendMessageW(g_hChat, WM_SETFONT, (WPARAM)g_fontChat, TRUE);
    SendMessageW(g_hInput, WM_SETFONT, (WPARAM)g_fontChat, TRUE);
    SendMessageW(g_hSend, WM_SETFONT, (WPARAM)g_fontUi, TRUE);
    SendMessageW(g_hNew, WM_SETFONT, (WPARAM)g_fontUi, TRUE);
    if (g_hModel) SendMessageW(g_hModel, WM_SETFONT, (WPARAM)g_fontUi, TRUE);
    if (g_hEffort) SendMessageW(g_hEffort, WM_SETFONT, (WPARAM)g_fontUi, TRUE);
    if (g_hLanguage) SendMessageW(g_hLanguage, WM_SETFONT, (WPARAM)g_fontUi, TRUE);
    if (g_hWebSearch) SendMessageW(g_hWebSearch, WM_SETFONT, (WPARAM)g_fontUi, TRUE);
}

// Layoutberekeningen (gedeeld door WM_SIZE en WM_PAINT)
struct Layout { RECT chatPanel, inputPanel; };
static Layout ComputeLayout(int w, int h)
{
    const int m = S(16), titleH = S(34), statusH = S(20), progH = S(6);
    const int inputH = S(76), btnW = S(120), gap = S(10), pad = S(10);
    Layout l{};
    int top = m + titleH + statusH + S(6) + progH + S(42);
    l.inputPanel = { m, h - m - inputH, w - m - btnW - gap, h - m };
    l.chatPanel  = { m, top, w - m, h - m - inputH - gap };
    (void)pad;
    return l;
}

static void DoLayout(int w, int h)
{
    const int m = S(16), titleH = S(34), statusH = S(20), progH = S(6);
    const int btnW = S(120), gap = S(10), pad = S(10);
    Layout l = ComputeLayout(w, h);

    MoveWindow(g_hTitle, m, m, w - 2 * m, titleH, TRUE);
    MoveWindow(g_hStatus, m, m + titleH, w - 2 * m, statusH, TRUE);
    MoveWindow(g_hProgress, m, m + titleH + statusH + S(6), w - 2 * m, progH, TRUE);

    int settingsY = m + titleH + statusH + S(6) + progH + S(8);
    int ch = S(27);
    MoveWindow(g_hModel, m, settingsY, S(140), ch, TRUE);
    MoveWindow(g_hEffort, m + S(148), settingsY, S(105), ch, TRUE);
    MoveWindow(g_hLanguage, m + S(261), settingsY, S(115), ch, TRUE);
    MoveWindow(g_hWebSearch, m + S(384), settingsY, S(130), ch, TRUE);

    MoveWindow(g_hChat, l.chatPanel.left + pad, l.chatPanel.top + pad,
               (l.chatPanel.right - l.chatPanel.left) - 2 * pad,
               (l.chatPanel.bottom - l.chatPanel.top) - 2 * pad, TRUE);
    MoveWindow(g_hInput, l.inputPanel.left + pad, l.inputPanel.top + pad,
               (l.inputPanel.right - l.inputPanel.left) - 2 * pad,
               (l.inputPanel.bottom - l.inputPanel.top) - 2 * pad, TRUE);

    int bx = w - m - btnW, bh = (l.inputPanel.bottom - l.inputPanel.top - gap) / 2;
    MoveWindow(g_hSend, bx, l.inputPanel.top, btnW, bh, TRUE);
    MoveWindow(g_hNew, bx, l.inputPanel.top + bh + gap, btnW, bh, TRUE);
}

// ---------------------------------------------------------------------------
// Acties
// ---------------------------------------------------------------------------
static void SendCurrent(HWND hWnd)
{
    if (!g_ready) return;
    if (g_busy) { g_ollama->Cancel(); return; }

    int len = GetWindowTextLengthW(g_hInput);
    if (len <= 0) return;
    std::wstring text(len + 1, L'\0');
    GetWindowTextW(g_hInput, &text[0], len + 1);
    text.resize(len);
    // trim
    size_t a = text.find_first_not_of(L" \t\r\n");
    if (a == std::wstring::npos) { SetWindowTextW(g_hInput, L""); return; }
    size_t b = text.find_last_not_of(L" \t\r\n");
    text = text.substr(a, b - a + 1);

    SetWindowTextW(g_hInput, L"");
    AppendChat(L"Jij\n", kUser, true);
    AppendChat(text + L"\n\n", kText, false);
    AppendChat(L"NoidGPT\n", kBot, true);
    g_assistantOpen = true;

    g_busy = true;
    UpdateControls();
    if (!g_ollama->SendUserMessage(ToUtf8(text)))
    {
        g_busy = false;
        UpdateControls();
    }
    (void)hWnd;
}

static void NewChat()
{
    if (g_busy) return;
    g_ollama->ResetChat();
    SetWindowTextW(g_hChat, L"");
    AppendChat(L"Nieuw gesprek gestart. Stel je vraag hieronder.\n\n", kMuted, false);
}

static void OpenDataFolder()
{
    ShellExecuteW(nullptr, L"open", g_ollama->DataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}


// ---------------------------------------------------------------------------
int APIENTRY wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    if (!LoadLibraryW(L"Msftedit.dll"))
    {
        MessageBoxW(nullptr, L"Msftedit.dll kon niet worden geladen.", L"NoidGPT", MB_ICONERROR);
        return 1;
    }

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_NOIDGPT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
        return FALSE;

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_NOIDGPT));
    HWND mainWnd = FindWindowW(szWindowClass, nullptr);
    (void)mainWnd;

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_NOIDGPT));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(kBg);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_NOIDGPT);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                              CW_USEDEFAULT, 0, 1100, 750, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        g_hMainWindow = hWnd;
        g_dpi = GetDpiForWindow(hWnd);
        g_brBg = CreateSolidBrush(kBg);
        g_brPanel = CreateSolidBrush(kPanel);

        g_hTitle = CreateWindowW(L"STATIC", L"NoidGPT", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)IDC_TITLE, hInst, nullptr);
        g_hStatus = CreateWindowW(L"STATIC", L"AI wordt gestart...", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, hWnd, (HMENU)IDC_STATUS, hInst, nullptr);
        g_hProgress = CreateWindowW(PROGRESS_CLASSW, nullptr, WS_CHILD | PBS_SMOOTH | PBS_MARQUEE, 0, 0, 0, 0, hWnd, (HMENU)IDC_PROGRESS, hInst, nullptr);
        SendMessageW(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        SendMessageW(g_hProgress, PBM_SETBARCOLOR, 0, (LPARAM)kAccent);
        SendMessageW(g_hProgress, PBM_SETBKCOLOR, 0, (LPARAM)kPanel);

        g_hModel = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
            0, 0, 0, 0, hWnd, (HMENU)IDC_MODEL, hInst, nullptr);
        const wchar_t* models[] = { L"Qwen 3.5 4B (aanbevolen)", L"Llama 3.1 8B", L"Phi-4 Mini 3.8B", L"Llama 3.2 3B" };
        for (auto item : models) SendMessageW(g_hModel, CB_ADDSTRING, 0, (LPARAM)item);
        g_hEffort = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
            0, 0, 0, 0, hWnd, (HMENU)IDC_EFFORT, hInst, nullptr);
        const wchar_t* efforts[] = { L"Snel", L"Normaal", L"Grondig", L"Diep nadenken" };
        for (auto item : efforts) SendMessageW(g_hEffort, CB_ADDSTRING, 0, (LPARAM)item);
        SendMessageW(g_hEffort, CB_SETCURSEL, 1, 0);
        g_hLanguage = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
            0, 0, 0, 0, hWnd, (HMENU)IDC_LANGUAGE, hInst, nullptr);
        const wchar_t* languages[] = { L"Automatisch", L"Nederlands", L"English", L"Français", L"Deutsch" };
        for (auto item : languages) SendMessageW(g_hLanguage, CB_ADDSTRING, 0, (LPARAM)item);
        SendMessageW(g_hLanguage, CB_SETCURSEL, 0, 0);
        g_hWebSearch = CreateWindowW(L"BUTTON", L"Web zoeken", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
            0, 0, 0, 0, hWnd, (HMENU)IDC_WEBSEARCH, hInst, nullptr);
        SendMessageW(g_hWebSearch, BM_SETCHECK, BST_CHECKED, 0);

        g_hChat = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, hWnd, (HMENU)IDC_CHAT, hInst, nullptr);
        SendMessageW(g_hChat, EM_SETBKGNDCOLOR, 0, (LPARAM)kPanel);
        SendMessageW(g_hChat, EM_SETTEXTMODE, TM_RICHTEXT, 0);

        g_hInput = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            0, 0, 0, 0, hWnd, (HMENU)IDC_INPUT, hInst, nullptr);
        SetWindowSubclass(g_hInput, InputProc, 1, 0);
        SendMessageW(g_hInput, EM_SETCUEBANNER, TRUE, (LPARAM)L"Typ een bericht... (Enter = versturen, Shift+Enter = nieuwe regel)");

        g_hSend = CreateWindowW(L"BUTTON", L"Versturen", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, hWnd, (HMENU)IDC_SEND, hInst, nullptr);
        g_hNew = CreateWindowW(L"BUTTON", L"Nieuw gesprek", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, hWnd, (HMENU)IDC_NEWCHAT_BTN, hInst, nullptr);

        ApplyFonts();

        OllamaManager::Callbacks cb;
        cb.status = [hWnd](const std::wstring& s) { Post(hWnd, WM_APP_STATUS, 0, &s); };
        cb.progress = [hWnd](int p) { PostMessageW(hWnd, WM_APP_PROGRESS, (WPARAM)p, 0); };
        cb.token = [hWnd](const std::wstring& s) { Post(hWnd, WM_APP_TOKEN, 0, &s); };
        cb.chatDone = [hWnd](bool ok, const std::wstring& e) { Post(hWnd, WM_APP_CHATDONE, ok, &e); };
        cb.ready = [hWnd](bool ok) { PostMessageW(hWnd, WM_APP_READY, ok, 0); };
        g_ollama = std::make_unique<OllamaManager>(cb);
        std::wstring configuredModel = g_ollama->Model();
        int modelIndex = 0;
        if (configuredModel == L"llama3.1:8b") modelIndex = 1;
        else if (configuredModel == L"phi4-mini") modelIndex = 2;
        else if (configuredModel == L"llama3.2:3b") modelIndex = 3;
        SendMessageW(g_hModel, CB_SETCURSEL, modelIndex, 0);
        g_ollama->SetChatOptions("Automatisch", "Normaal", true);

        AppendChat(L"Welkom bij NoidGPT!\n", kText, true);
        AppendChat(L"Een lokale AI die op jouw pc draait (model: " + g_ollama->Model() +
                   L"). Bij de eerste start worden Ollama en het model gedownload. Gebruik Web zoeken om actuele informatie via internet op te halen.\n\n", kMuted, false);

        UpdateControls();
        g_ollama->Start();
        return 0;
    }

    case WM_SIZE:
        DoLayout(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_GETMINMAXINFO:
    {
        auto* mm = (MINMAXINFO*)lParam;
        mm->ptMinTrackSize.x = S(560);
        mm->ptMinTrackSize.y = S(420);
        return 0;
    }

    case WM_DPICHANGED:
    {
        g_dpi = HIWORD(wParam);
        ApplyFonts();
        RECT* r = (RECT*)lParam;
        SetWindowPos(hWnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == CBN_SELCHANGE && (LOWORD(wParam) == IDC_EFFORT || LOWORD(wParam) == IDC_LANGUAGE))
        {
            wchar_t effort[64]{}, language[64]{};
            GetWindowTextW(g_hEffort, effort, 64);
            GetWindowTextW(g_hLanguage, language, 64);
            bool web = SendMessageW(g_hWebSearch, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (g_ollama) g_ollama->SetChatOptions(ToUtf8(language), ToUtf8(effort), web);
            return 0;
        }
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_WEBSEARCH)
        {
            wchar_t effort[64]{}, language[64]{};
            GetWindowTextW(g_hEffort, effort, 64);
            GetWindowTextW(g_hLanguage, language, 64);
            bool web = SendMessageW(g_hWebSearch, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (g_ollama) g_ollama->SetChatOptions(ToUtf8(language), ToUtf8(effort), web);
            return 0;
        }
        if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_MODEL)
        {
            int ix = (int)SendMessageW(g_hModel, CB_GETCURSEL, 0, 0);
            const wchar_t* tags[] = { L"qwen3.5:4b-q4_K_M", L"llama3.1:8b", L"phi4-mini", L"llama3.2:3b" };
            if (g_ollama && ix >= 0 && ix < 4)
            {
                WritePrivateProfileStringW(L"NoidGPT", L"Model", tags[ix], g_ollama->ConfigFile().c_str());
                SetWindowTextW(g_hStatus, L"Modelkeuze opgeslagen. Herstart NoidGPT om dit model te laden.");
            }
            return 0;
        }
        switch (LOWORD(wParam))
        {
        case IDC_SEND:       SendCurrent(hWnd); SetFocus(g_hInput); break;
        case IDC_NEWCHAT_BTN:
        case IDM_NEWCHAT:    NewChat(); break;
        case IDM_OPENDATA:   OpenDataFolder(); break;
        case IDM_ABOUT:      DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About); break;
        case IDM_EXIT:       DestroyWindow(hWnd); break;
        default:             return DefWindowProc(hWnd, message, wParam, lParam);
        }
        return 0;
    }

    case WM_APP_STATUS:
    {
        std::unique_ptr<std::wstring> s((std::wstring*)lParam);
        if (s) SetWindowTextW(g_hStatus, s->c_str());
        return 0;
    }
    case WM_APP_PROGRESS:
    {
        int p = (int)wParam;
        if (p < 0)
        {
            SendMessageW(g_hProgress, PBM_SETMARQUEE, FALSE, 0);
            ShowWindow(g_hProgress, SW_HIDE);
        }
        else
        {
            SendMessageW(g_hProgress, PBM_SETMARQUEE, FALSE, 0);
            ShowWindow(g_hProgress, SW_SHOW);
            SendMessageW(g_hProgress, PBM_SETPOS, p, 0);
        }
        return 0;
    }
    case WM_APP_READY:
    {
        g_ready = wParam != 0;
        if (g_ready)
        {
            SetWindowTextW(g_hStatus, L"NoidGPT is klaar. Je kunt chatten.");
            UpdateControls();
        }
        else
        {
            AppendChat(L"Ollama starten mislukt. Zie de statusregel bovenaan.\n\n", kError, false);
            UpdateControls();
        }
        return 0;
    }
    case WM_APP_TOKEN:
    {
        std::unique_ptr<std::wstring> s((std::wstring*)lParam);
        if (s) AppendChat(*s, kText, false);
        return 0;
    }
    case WM_APP_CHATDONE:
    {
        std::unique_ptr<std::wstring> e((std::wstring*)lParam);
        AppendChat(L"\n\n", kText, false);
        if (wParam == 0 && e && !e->empty())
            AppendChat(L"Fout: " + *e + L"\n\n", kError, false);
        g_busy = false;
        g_assistantOpen = false;
        UpdateControls();
        SetFocus(g_hInput);
        return 0;
    }

    case WM_DRAWITEM:
    {
        auto* d = (DRAWITEMSTRUCT*)lParam;
        if (d->CtlType != ODT_BUTTON) break;
        bool disabled = (d->itemState & ODS_DISABLED) != 0;
        bool pressed = (d->itemState & ODS_SELECTED) != 0;
        bool secondary = d->CtlID == IDC_NEWCHAT_BTN;
        COLORREF fill = disabled ? kPanel : (secondary ? (pressed ? kBorder : RGB(58, 59, 65))
                                                       : (pressed ? kAccentH : kAccent));
        if (d->CtlID == IDC_SEND && g_busy && !disabled) fill = pressed ? RGB(180, 60, 60) : RGB(214, 76, 76);
        HBRUSH br = CreateSolidBrush(fill);
        HPEN pen = CreatePen(PS_SOLID, 1, fill);
        HGDIOBJ ob = SelectObject(d->hDC, br), op = SelectObject(d->hDC, pen);
        RoundRect(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right, d->rcItem.bottom, S(12), S(12));
        SelectObject(d->hDC, ob);
        SelectObject(d->hDC, op);
        DeleteObject(br);
        DeleteObject(pen);
        wchar_t txt[64];
        GetWindowTextW(d->hwndItem, txt, 64);
        SetBkMode(d->hDC, TRANSPARENT);
        SetTextColor(d->hDC, disabled ? kMuted : RGB(255, 255, 255));
        HGDIOBJ of = SelectObject(d->hDC, g_fontUi);
        DrawTextW(d->hDC, txt, -1, &d->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(d->hDC, of);
        return TRUE;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC dc = (HDC)wParam;
        SetBkColor(dc, kBg);
        SetTextColor(dc, (HWND)lParam == g_hStatus ? kMuted : kText);
        return (LRESULT)g_brBg;
    }
    case WM_CTLCOLOREDIT:
    {
        HDC dc = (HDC)wParam;
        SetBkColor(dc, kPanel);
        SetTextColor(dc, kText);
        return (LRESULT)g_brPanel;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        Layout l = ComputeLayout(rc.right, rc.bottom);
        HBRUSH br = CreateSolidBrush(kPanel);
        HPEN pen = CreatePen(PS_SOLID, 1, kBorder);
        HGDIOBJ ob = SelectObject(dc, br), op = SelectObject(dc, pen);
        RoundRect(dc, l.chatPanel.left, l.chatPanel.top, l.chatPanel.right, l.chatPanel.bottom, S(14), S(14));
        RoundRect(dc, l.inputPanel.left, l.inputPanel.top, l.inputPanel.right, l.inputPanel.bottom, S(14), S(14));
        SelectObject(dc, ob);
        SelectObject(dc, op);
        DeleteObject(br);
        DeleteObject(pen);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        if (g_ollama) g_ollama->Shutdown();
        g_ollama.reset();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Enter = versturen, Shift+Enter = nieuwe regel
LRESULT CALLBACK InputProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    if ((msg == WM_KEYDOWN || msg == WM_CHAR) && wParam == VK_RETURN && !(GetKeyState(VK_SHIFT) & 0x8000))
    {
        if (msg == WM_KEYDOWN)
            PostMessageW(GetParent(hWnd), WM_COMMAND, MAKEWPARAM(IDC_SEND, BN_CLICKED), (LPARAM)g_hSend);
        return 0; // geen newline en geen piepje
    }
    if (msg == WM_KEYDOWN && wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000))
    {
        SendMessageW(hWnd, EM_SETSEL, 0, -1);
        return 0;
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
