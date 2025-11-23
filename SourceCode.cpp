#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <cctype>
#include <cstring>

#pragma comment(lib, "comctl32.lib")

#define IDC_EDIT_TEXT     1001
#define IDC_EDIT_KEY      1002
#define IDC_BUTTON_SWAP   1003
#define IDC_BUTTON_COPY   1004
#define IDC_BUTTON_PASTE  1005
#define IDC_STATIC_AUTHOR 1006

// ID для акселераторов (любой уникальный номер > 1000)
#define ID_ACCEL_CTRL_A   9000
#define ID_ACCEL_CTRL_C   9001
#define ID_ACCEL_CTRL_V   9002

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Глобальная таблица акселераторов
HACCEL g_hAccel = nullptr;

static const std::string ENC_PREFIX = "Marin:"; // маркер зашифрованного текста, не удаляйте его

static std::string WstrToUtf8(const std::wstring& w)
{
    if (w.empty()) return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return {};
    std::string out(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &out[0], size_needed, nullptr, nullptr);
    return out;
}

static std::wstring Utf8ToWstr(const std::string& s)
{
    if (s.empty()) return {};
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (size_needed <= 0) return {};
    std::wstring out(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &out[0], size_needed);
    return out;
}

static const char* b64_chars =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

static inline bool is_b64(unsigned char c)
{
    return (std::isalnum(c) || (c == '+') || (c == '/'));
}

static std::string Base64Encode(const std::vector<unsigned char>& data)
{
    std::string ret;
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    size_t in_len = data.size();
    size_t pos = 0;
    while (in_len--)
    {
        char_array_3[i++] = data[pos++];
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++) ret += b64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i)
    {
        for (int j = i; j < 3; j++) char_array_3[j] = '\0';
        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int j = 0; j < i + 1; j++) ret += b64_chars[char_array_4[j]];
        while ((i++ < 3)) ret += '=';
    }

    return ret;
}

static std::vector<unsigned char> Base64Decode(const std::string& encoded_string)
{
    size_t in_len = encoded_string.size();
    size_t i = 0;
    size_t in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<unsigned char> ret;

    while (in_len-- && (encoded_string[in_] != '=') && is_b64((unsigned char)encoded_string[in_])) {
        char_array_4[i++] = encoded_string[in_]; in_++;
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
            {
                const char* p = std::strchr(b64_chars, char_array_4[i]);
                if (!p) throw std::runtime_error("Invalid base64 char");
                char_array_4[i] = (unsigned char)(p - b64_chars);
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++) ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i)
    {
        for (size_t j = i; j < 4; j++) char_array_4[j] = 0;

        for (size_t j = 0; j < 4; j++)
        {
            if (char_array_4[j] == 0)
            {
                char_array_4[j] = 0;
            }
            else {
                const char* p = std::strchr(b64_chars, char_array_4[j]);
                if (!p) throw std::runtime_error("Invalid base64 char");
                char_array_4[j] = (unsigned char)(p - b64_chars);
            }
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (size_t j = 0; j < i - 1; j++) ret.push_back(char_array_3[j]);
    }

    return ret;
}

static std::vector<unsigned char> XorBytes(const std::vector<unsigned char>& data, const std::vector<unsigned char>& key)
{
    if (key.empty()) return data;
    std::vector<unsigned char> out(data.size());
    for (size_t i = 0; i < data.size(); ++i)
        out[i] = data[i] ^ key[i % key.size()];
    return out;
}

std::wstring XorCrypt(const std::wstring& text, const std::wstring& key)
{
    if (key.empty()) return text;

    std::string utf8_text = WstrToUtf8(text);
    std::string utf8_key = WstrToUtf8(key);

    if (utf8_text.rfind(ENC_PREFIX, 0) == 0)
    {
        std::string b64 = utf8_text.substr(ENC_PREFIX.size());
        try
        {
            std::vector<unsigned char> data = Base64Decode(b64);
            std::vector<unsigned char> kbytes(utf8_key.begin(), utf8_key.end());
            std::vector<unsigned char> plain = XorBytes(data, kbytes);
            std::string splain(plain.begin(), plain.end());
            return Utf8ToWstr(splain);
        }
        catch (...)
        {
            return text;
        }
    }
    else
    {
        std::vector<unsigned char> data(utf8_text.begin(), utf8_text.end());
        std::vector<unsigned char> kbytes(utf8_key.begin(), utf8_key.end());
        std::vector<unsigned char> enc = XorBytes(data, kbytes);
        std::string b64 = Base64Encode(enc);
        std::string out = ENC_PREFIX + b64;
        return Utf8ToWstr(out);
    }
}

bool CopyToClipboard(HWND hwnd, const std::wstring& text)
{
    if (!OpenClipboard(hwnd)) return false;
    EmptyClipboard();

    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (!hGlob) { CloseClipboard(); return false; }

    wchar_t* pBuf = static_cast<wchar_t*>(GlobalLock(hGlob));
    wcscpy_s(pBuf, text.size() + 1, text.c_str());
    GlobalUnlock(hGlob);

    SetClipboardData(CF_UNICODETEXT, hGlob);
    CloseClipboard();
    return true;
}

std::wstring PasteFromClipboard(HWND hwnd)
{
    std::wstring result;
    if (!OpenClipboard(hwnd) || !IsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        CloseClipboard();
        return result;
    }

    HGLOBAL hGlob = GetClipboardData(CF_UNICODETEXT);
    if (hGlob)
    {
        wchar_t* pBuf = static_cast<wchar_t*>(GlobalLock(hGlob));
        if (pBuf) result = pBuf;
        GlobalUnlock(hGlob);
    }
    CloseClipboard();
    return result;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    InitCommonControls();

    const wchar_t CLASS_NAME[] = L"MarinTxtCrypt3";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"MarinTxtCrypt3",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX & ~WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 420,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Таблица акселераторов
    ACCEL accel[] =
    {
        { FCONTROL | FVIRTKEY, 'A', ID_ACCEL_CTRL_A },
        { FCONTROL | FVIRTKEY, 'C', ID_ACCEL_CTRL_C },
        { FCONTROL | FVIRTKEY, 'V', ID_ACCEL_CTRL_V }
    };
    g_hAccel = CreateAcceleratorTable(accel, _countof(accel));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!g_hAccel || !TranslateAccelerator(hwnd, g_hAccel, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (g_hAccel) DestroyAcceleratorTable(g_hAccel);
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static HWND hEditText, hEditKey;

    switch (uMsg)
    {
    case WM_CREATE:
    {
        HFONT hFont = CreateFont(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        HFONT hFontBtn = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        CreateWindow(L"STATIC", L"Enter the text:", WS_VISIBLE | WS_CHILD,
            20, 15, 460, 20, hwnd, nullptr, nullptr, nullptr);

        hEditText = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | ES_MULTILINE | ES_WANTRETURN | WS_VSCROLL | WS_TABSTOP,
            20, 40, 460, 120, hwnd, (HMENU)IDC_EDIT_TEXT, nullptr, nullptr);

        CreateWindow(L"STATIC", L"Enter the key:", WS_VISIBLE | WS_CHILD,
            20, 175, 460, 20, hwnd, nullptr, nullptr, nullptr);

        hEditKey = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_VISIBLE | WS_CHILD | ES_AUTOHSCROLL | WS_TABSTOP,
            20, 200, 460, 28, hwnd, (HMENU)IDC_EDIT_KEY, nullptr, nullptr);

        CreateWindow(L"BUTTON", L"Encrypt / Decrypt",
            WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            20, 245, 460, 40, hwnd, (HMENU)IDC_BUTTON_SWAP, nullptr, nullptr);

        CreateWindow(L"BUTTON", L"Copy the result",
            WS_VISIBLE | WS_CHILD, 20, 300, 225, 35, hwnd, (HMENU)IDC_BUTTON_COPY, nullptr, nullptr);

        CreateWindow(L"BUTTON", L"Paste from the buffer",
            WS_VISIBLE | WS_CHILD, 255, 300, 225, 35, hwnd, (HMENU)IDC_BUTTON_PASTE, nullptr, nullptr);

        CreateWindow(L"STATIC", L"Coder: Keneshov Maren / Kensec Proj. / YT: Maren 704",
            WS_VISIBLE | WS_CHILD | SS_CENTER,
            0, 350, 520, 20, hwnd, (HMENU)IDC_STATIC_AUTHOR, nullptr, nullptr);

        // Шрифты
        SendMessage(hEditText, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hEditKey, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(GetDlgItem(hwnd, IDC_BUTTON_SWAP), WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(GetDlgItem(hwnd, IDC_BUTTON_COPY), WM_SETFONT, (WPARAM)hFontBtn, TRUE);
        SendMessage(GetDlgItem(hwnd, IDC_BUTTON_PASTE), WM_SETFONT, (WPARAM)hFontBtn, TRUE);

        HFONT hFontAuthor = CreateFont(13, 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
        SendMessage(GetDlgItem(hwnd, IDC_STATIC_AUTHOR), WM_SETFONT, (WPARAM)hFontAuthor, TRUE);
    }
    break;

    case WM_COMMAND:
    {
        // Обработка акселераторов
        if (LOWORD(wParam) >= ID_ACCEL_CTRL_A && LOWORD(wParam) <= ID_ACCEL_CTRL_V)
        {
            HWND hFocus = GetFocus();
            if (hFocus == hEditText || hFocus == hEditKey)
            {
                switch (LOWORD(wParam))
                {
                case ID_ACCEL_CTRL_A: SendMessage(hFocus, EM_SETSEL, 0, -1); break;
                case ID_ACCEL_CTRL_C: SendMessage(hFocus, WM_COPY, 0, 0); break;
                case ID_ACCEL_CTRL_V: SendMessage(hFocus, WM_PASTE, 0, 0); break;
                }
            }
            return 0;
        }

        switch (LOWORD(wParam))
        {
        case IDC_BUTTON_SWAP:
        {
            int lenText = GetWindowTextLength(hEditText) + 1;
            int lenKey = GetWindowTextLength(hEditKey) + 1;

            std::wstring text(lenText, L'\0');
            std::wstring key(lenKey, L'\0');

            GetWindowText(hEditText, &text[0], lenText);
            GetWindowText(hEditKey, &key[0], lenKey);

            if (!text.empty() && text.back() == L'\0') text.pop_back();
            if (!key.empty() && key.back() == L'\0') key.pop_back();

            std::wstring result = XorCrypt(text, key);
            SetWindowText(hEditText, result.c_str());
        }
        break;

        case IDC_BUTTON_COPY:
        {
            int len = GetWindowTextLength(hEditText) + 1;
            std::wstring text(len, L'\0');
            GetWindowText(hEditText, &text[0], len);
            if (!text.empty() && text.back() == L'\0') text.pop_back();
            if (!text.empty())
            {
                CopyToClipboard(hwnd, text);
                MessageBeep(MB_ICONINFORMATION);
            }
        }
        break;
        
        case IDC_BUTTON_PASTE:
        {
            std::wstring clip = PasteFromClipboard(hwnd);
            if (!clip.empty())
                SetWindowText(hEditText, clip.c_str());
        }
        break;
        }
    }
    break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}
