#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0600
#define WINVER 0x0600
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef STARTF_USESTDERROR
#define STARTF_USESTDERROR 0x0400
#endif
#ifndef STARTF_USESTDOUT
#define STARTF_USESTDOUT 0x0100
#endif
#ifndef STARTF_USESTDERR
#define STARTF_USESTDERR 0x0400
#endif

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "ole32.lib")

#define COLOR_BG RGB(250, 250, 252)
#define COLOR_SIDEBAR RGB(255, 255, 255)
#define COLOR_PRIMARY RGB(64, 158, 255)
#define COLOR_PRIMARY_HOVER RGB(102, 177, 255)
#define COLOR_TEXT RGB(51, 51, 51)
#define COLOR_TEXT_LIGHT RGB(128, 128, 128)
#define COLOR_BORDER RGB(230, 230, 235)
#define COLOR_SUCCESS RGB(103, 194, 58)
#define COLOR_WARNING RGB(230, 162, 60)
#define COLOR_LIST_BG RGB(255, 255, 255)
#define COLOR_LIST_ITEM RGB(248, 249, 250)
#define COLOR_LIST_ITEM_SEL RGB(235, 245, 255)

#define IDC_LIST_FILES 1001
#define IDC_BTN_ADD 1002
#define IDC_BTN_REMOVE 1003
#define IDC_BTN_CLEAR 1004
#define IDC_BTN_COMPRESS 1005
#define IDC_SLIDER_CRF 1006
#define IDC_STATIC_CRF 1007
#define IDC_COMBO_ENCODER 1008
#define IDC_COMBO_SPEED 1013
#define IDC_PROGRESS 1009
#define IDC_STATIC_STATUS 1010
#define IDC_BTN_BROWSE 1011
#define IDC_STATIC_OUTPUT 1012

#define WM_COMPRESS_PROGRESS (WM_USER + 100)
#define WM_COMPRESS_COMPLETE (WM_USER + 101)

typedef struct {
    WCHAR filePath[MAX_PATH];
    WCHAR fileName[MAX_PATH];
    float duration;
    BOOL isCompressing;
    BOOL isCompleted;
    int progress;
    LONGLONG sourceSize;
    LONGLONG compressedSize;
} VideoFile;

static VideoFile g_files[100];
static int g_fileCount = 0;
static int g_currentFileIndex = -1;
static BOOL g_isCompressing = FALSE;
static HWND g_hwndList = NULL;
static HWND g_hwndSlider = NULL;
static HWND g_hwndCrfValue = NULL;
static HWND g_hwndCombo = NULL;
static HWND g_hwndSpeedCombo = NULL;
static HWND g_hwndBtnCompress = NULL;
static HWND g_hwndProgress = NULL;
static HWND g_hwndStatus = NULL;
static HWND g_hwndOutputPath = NULL;
static HFONT g_hFont = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontSmall = NULL;
static WCHAR g_outputDir[MAX_PATH] = {0};
static float g_totalDuration = 0;
static float g_currentDuration = 0;
static volatile BOOL g_cancelCompression = FALSE;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
static void CreateControls(HWND hwnd);
static void AddFiles(HWND hwnd);
static void RemoveSelectedFile(void);
static void ClearAllFiles(void);
static void StartCompression(HWND hwnd);
static void UpdateFileList(void);
static void UpdateCRFDisplay(HWND hwnd);
static void GetOutputPath(const WCHAR* inputPath, WCHAR* outputPath, const WCHAR* encoder);
static DWORD WINAPI CompressThread(LPVOID param);
static float GetVideoDuration(const WCHAR* filePath);
static void ParseFFmpegProgress(const char* line, float* currentTime, float totalDuration, int* progress);
static void FormatSize(LONGLONG size, WCHAR* buffer);

static void InitCommonControlsWrapper(void) {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    InitCommonControlsWrapper();
    
    HICON hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(1));
    if (!hIcon) {
        hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COLOR_BG);
    wc.lpszClassName = L"VideoCompressorClass";
    wc.hIcon = hIcon;
    wc.hIconSm = hIcon;
    RegisterClassEx(&wc);
    
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int width = 900;
    int height = 590;
    int x = (screenWidth - width) / 2;
    int y = (screenHeight - height) / 2;
    
    HWND hwnd = CreateWindowEx(
        WS_EX_ACCEPTFILES | WS_EX_APPWINDOW,
        L"VideoCompressorClass",
        L"极简视频压缩工具",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, height,
        NULL, NULL, hInstance, NULL
    );
    
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}

static void CreateFonts(void) {
    g_hFont = CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    
    g_hFontBold = CreateFont(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    
    g_hFontSmall = CreateFont(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
}

static void CreateControls(HWND hwnd) {
    CreateFonts();
    
    g_hwndList = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_VISIBLE | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY | WS_VSCROLL | LBS_OWNERDRAWFIXED,
        20, 70, 520, 425,
        hwnd, (HMENU)IDC_LIST_FILES, NULL, NULL
    );
    SendMessage(g_hwndList, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessage(g_hwndList, LB_SETITEMHEIGHT, 0, 50);
    
    HWND hwndBtnAdd = CreateWindowEx(
        0, L"BUTTON", L"添加文件",
        WS_CHILD | WS_VISIBLE | BS_FLAT,
        20, 510, 100, 32,
        hwnd, (HMENU)IDC_BTN_ADD, NULL, NULL
    );
    SendMessage(hwndBtnAdd, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    HWND hwndBtnRemove = CreateWindowEx(
        0, L"BUTTON", L"移除选中",
        WS_CHILD | WS_VISIBLE | BS_FLAT,
        130, 510, 100, 32,
        hwnd, (HMENU)IDC_BTN_REMOVE, NULL, NULL
    );
    SendMessage(hwndBtnRemove, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    HWND hwndBtnClear = CreateWindowEx(
        0, L"BUTTON", L"清空列表",
        WS_CHILD | WS_VISIBLE | BS_FLAT,
        240, 510, 100, 32,
        hwnd, (HMENU)IDC_BTN_CLEAR, NULL, NULL
    );
    SendMessage(hwndBtnClear, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    HWND hwndTitle = CreateWindowEx(
        0, L"STATIC", L"文件列表",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 15, 200, 28,
        hwnd, NULL, NULL, NULL
    );
    SendMessage(hwndTitle, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
    
    HWND hwndHint = CreateWindowEx(
        0, L"STATIC", L"支持拖拽文件到此处",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        20, 45, 200, 22,
        hwnd, NULL, NULL, NULL
    );
    SendMessage(hwndHint, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    
    HWND hwndSettingsTitle = CreateWindowEx(
        0, L"STATIC", L"压缩设置",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        580, 15, 200, 28,
        hwnd, NULL, NULL, NULL
    );
    SendMessage(hwndSettingsTitle, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
    
    HWND hwndEncoderLabel = CreateWindowEx(
        0, L"STATIC", L"编码器",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        580, 50, 100, 22,
        hwnd, NULL, NULL, NULL
    );
    SendMessage(hwndEncoderLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_hwndCombo = CreateWindowEx(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
        580, 72, 280, 180,
        hwnd, (HMENU)IDC_COMBO_ENCODER, NULL, NULL
    );
    SendMessage(g_hwndCombo, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessage(g_hwndCombo, CB_ADDSTRING, 0, (LPARAM)L"H.264 (libx264) - 兼容性好");
    SendMessage(g_hwndCombo, CB_ADDSTRING, 0, (LPARAM)L"H.265 (libx265) - 压缩率高");
    SendMessage(g_hwndCombo, CB_SETCURSEL, 0, 0);
    
    HWND hwndSpeedLabel = CreateWindowEx(
        0, L"STATIC", L"压缩速度",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        580, 105, 100, 22,
        hwnd, NULL, NULL, NULL
    );
    SendMessage(hwndSpeedLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_hwndSpeedCombo = CreateWindowEx(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | CBS_HASSTRINGS,
        580, 127, 280, 180,
        hwnd, (HMENU)IDC_COMBO_SPEED, NULL, NULL
    );
    SendMessage(g_hwndSpeedCombo, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    SendMessage(g_hwndSpeedCombo, CB_ADDSTRING, 0, (LPARAM)L"ultrafast - 最快");
    SendMessage(g_hwndSpeedCombo, CB_ADDSTRING, 0, (LPARAM)L"superfast - 非常快");
    SendMessage(g_hwndSpeedCombo, CB_ADDSTRING, 0, (LPARAM)L"veryfast - 很快");
    SendMessage(g_hwndSpeedCombo, CB_ADDSTRING, 0, (LPARAM)L"faster - 较快");
    SendMessage(g_hwndSpeedCombo, CB_ADDSTRING, 0, (LPARAM)L"fast - 快");
    SendMessage(g_hwndSpeedCombo, CB_ADDSTRING, 0, (LPARAM)L"medium - 中 (推荐)");
    SendMessage(g_hwndSpeedCombo, CB_ADDSTRING, 0, (LPARAM)L"slow - 慢");
    SendMessage(g_hwndSpeedCombo, CB_SETCURSEL, 2, 0);
    
    HWND hwndCrfLabel = CreateWindowEx(
        0, L"STATIC", L"压缩质量 (CRF)",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        580, 160, 200, 22,
        hwnd, NULL, NULL, NULL
    );
    SendMessage(hwndCrfLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    HWND hwndCrfHint = CreateWindowEx(
        0, L"STATIC", L"数值越大压缩率越高，画质越低",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        580, 180, 280, 18,
        hwnd, NULL, NULL, NULL
    );
    SendMessage(hwndCrfHint, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    
    g_hwndSlider = CreateWindowEx(
        0, TRACKBAR_CLASS, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_FIXEDLENGTH,
        580, 202, 280, 20,
        hwnd, (HMENU)IDC_SLIDER_CRF, NULL, NULL
    );
    SendMessage(g_hwndSlider, TBM_SETRANGE, TRUE, MAKELONG(18, 51));
    SendMessage(g_hwndSlider, TBM_SETPOS, TRUE, 28);
    SendMessage(g_hwndSlider, TBM_SETPAGESIZE, 0, 1);
    SendMessage(g_hwndSlider, TBM_SETTHUMBLENGTH, 16, 0);
    
    g_hwndCrfValue = CreateWindowEx(
        0, L"STATIC", L"28",
        WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        815, 200, 45, 22,
        hwnd, (HMENU)IDC_STATIC_CRF, NULL, NULL
    );
    SendMessage(g_hwndCrfValue, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
    
    HWND hwndOutputLabel = CreateWindowEx(
        0, L"STATIC", L"输出目录",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        580, 232, 100, 22,
        hwnd, NULL, NULL, NULL
    );
    SendMessage(hwndOutputLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_hwndOutputPath = CreateWindowEx(
        WS_EX_STATICEDGE,
        L"STATIC", L"默认保存到源文件目录",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_PATHELLIPSIS,
        580, 254, 220, 22,
        hwnd, (HMENU)IDC_STATIC_OUTPUT, NULL, NULL
    );
    SendMessage(g_hwndOutputPath, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    
    HWND hwndBtnBrowse = CreateWindowEx(
        0, L"BUTTON", L"选择",
        WS_CHILD | WS_VISIBLE | BS_FLAT,
        805, 252, 55, 26,
        hwnd, (HMENU)IDC_BTN_BROWSE, NULL, NULL
    );
    SendMessage(hwndBtnBrowse, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    
    g_hwndBtnCompress = CreateWindowEx(
        0, L"BUTTON", L"开始压缩",
        WS_CHILD | WS_VISIBLE | BS_FLAT,
        580, 290, 280, 40,
        hwnd, (HMENU)IDC_BTN_COMPRESS, NULL, NULL
    );
    SendMessage(g_hwndBtnCompress, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);
    
    g_hwndProgress = CreateWindowEx(
        WS_EX_STATICEDGE,
        PROGRESS_CLASS, L"",
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        580, 340, 280, 20,
        hwnd, (HMENU)IDC_PROGRESS, NULL, NULL
    );
    SendMessage(g_hwndProgress, PBM_SETBARCOLOR, 0, COLOR_PRIMARY);
    SendMessage(g_hwndProgress, PBM_SETBKCOLOR, 0, RGB(240, 240, 240));
    SendMessage(g_hwndProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    
    g_hwndStatus = CreateWindowEx(
        0, L"STATIC", L"就绪",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        580, 368, 280, 22,
        hwnd, (HMENU)IDC_STATIC_STATUS, NULL, NULL
    );
    SendMessage(g_hwndStatus, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    
    GetCurrentDirectory(MAX_PATH, g_outputDir);
}

static float GetVideoDuration(const WCHAR* filePath) {
    WCHAR cmd[1024];
    wsprintf(cmd, L"ffprobe.exe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\"", filePath);
    
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    
    HANDLE hReadPipe, hWritePipe;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFO si = {0};
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESTDOUT | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {0};
    float duration = 0;
    
    if (CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(hWritePipe);
        
        char buffer[256];
        DWORD bytesRead;
        if (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = 0;
            duration = (float)atof(buffer);
        }
        
        WaitForSingleObject(pi.hProcess, 5000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        CloseHandle(hWritePipe);
    }
    
    CloseHandle(hReadPipe);
    
    if (duration <= 0) {
        duration = 60.0f;
    }
    
    return duration;
}

static void FormatSize(LONGLONG size, WCHAR* buffer) {
    if (size < 1024) {
        swprintf(buffer, 32, L"%lld B", size);
    } else if (size < 1024 * 1024) {
        swprintf(buffer, 32, L"%.1f KB", (double)size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        swprintf(buffer, 32, L"%.1f MB", (double)size / (1024.0 * 1024.0));
    } else {
        swprintf(buffer, 32, L"%.2f GB", (double)size / (1024.0 * 1024.0 * 1024.0));
    }
}

static void AddFiles(HWND hwnd) {
    OPENFILENAME ofn = {0};
    WCHAR szFile[4096] = {0};
    
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = 4096;
    ofn.lpstrFilter = L"视频文件\0*.mp4;*.avi;*.mkv;*.mov;*.wmv;*.flv;*.webm;*.m4v\0所有文件\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrTitle = L"选择视频文件";
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    
    if (GetOpenFileName(&ofn)) {
        WCHAR* p = szFile;
        WCHAR dir[MAX_PATH];
        wcscpy(dir, p);
        p += wcslen(p) + 1;
        
        if (*p == 0) {
            if (g_fileCount < 100) {
                wcscpy(g_files[g_fileCount].filePath, dir);
                WCHAR* fileName = wcsrchr(dir, L'\\');
                if (fileName) {
                    wcscpy(g_files[g_fileCount].fileName, fileName + 1);
                } else {
                    wcscpy(g_files[g_fileCount].fileName, dir);
                }
                g_files[g_fileCount].isCompressing = FALSE;
                g_files[g_fileCount].isCompleted = FALSE;
                g_files[g_fileCount].progress = 0;
                g_files[g_fileCount].duration = GetVideoDuration(g_files[g_fileCount].filePath);
                
                HANDLE hFile = CreateFile(g_files[g_fileCount].filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    LARGE_INTEGER fileSize;
                    GetFileSizeEx(hFile, &fileSize);
                    g_files[g_fileCount].sourceSize = fileSize.QuadPart;
                    g_files[g_fileCount].compressedSize = 0;
                    CloseHandle(hFile);
                } else {
                    g_files[g_fileCount].sourceSize = 0;
                    g_files[g_fileCount].compressedSize = 0;
                }
                
                g_fileCount++;
            }
        } else {
            while (*p != 0 && g_fileCount < 100) {
                wcscpy(g_files[g_fileCount].filePath, dir);
                wcscat(g_files[g_fileCount].filePath, L"\\");
                wcscat(g_files[g_fileCount].filePath, p);
                wcscpy(g_files[g_fileCount].fileName, p);
                g_files[g_fileCount].isCompressing = FALSE;
                g_files[g_fileCount].isCompleted = FALSE;
                g_files[g_fileCount].progress = 0;
                g_files[g_fileCount].duration = GetVideoDuration(g_files[g_fileCount].filePath);
                
                HANDLE hFile = CreateFile(g_files[g_fileCount].filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    LARGE_INTEGER fileSize;
                    GetFileSizeEx(hFile, &fileSize);
                    g_files[g_fileCount].sourceSize = fileSize.QuadPart;
                    g_files[g_fileCount].compressedSize = 0;
                    CloseHandle(hFile);
                } else {
                    g_files[g_fileCount].sourceSize = 0;
                    g_files[g_fileCount].compressedSize = 0;
                }
                
                g_fileCount++;
                p += wcslen(p) + 1;
            }
        }
        UpdateFileList();
    }
}

static void RemoveSelectedFile(void) {
    int sel = (int)SendMessage(g_hwndList, LB_GETCURSEL, 0, 0);
    if (sel != LB_ERR && sel < g_fileCount) {
        for (int i = sel; i < g_fileCount - 1; i++) {
            g_files[i] = g_files[i + 1];
        }
        g_fileCount--;
        UpdateFileList();
    }
}

static void ClearAllFiles(void) {
    g_fileCount = 0;
    g_totalDuration = 0;
    UpdateFileList();
}

static void UpdateFileList(void) {
    InvalidateRect(g_hwndList, NULL, TRUE);
    SendMessage(g_hwndList, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_fileCount; i++) {
        SendMessage(g_hwndList, LB_ADDSTRING, 0, (LPARAM)&g_files[i]);
    }
    
    WCHAR status[256];
    wsprintf(status, L"共 %d 个文件", g_fileCount);
    SetWindowText(g_hwndStatus, status);
}

static void UpdateCRFDisplay(HWND hwnd) {
    int crf = (int)SendMessage(g_hwndSlider, TBM_GETPOS, 0, 0);
    WCHAR text[8];
    wsprintf(text, L"%d", crf);
    SetWindowText(g_hwndCrfValue, text);
    InvalidateRect(g_hwndCrfValue, NULL, TRUE);
}

static void GetOutputPath(const WCHAR* inputPath, WCHAR* outputPath, const WCHAR* encoder) {
    WCHAR dir[MAX_PATH] = {0};
    WCHAR name[MAX_PATH] = {0};
    
    if (wcslen(g_outputDir) > 0) {
        wcscpy(dir, g_outputDir);
    } else {
        wcscpy(dir, inputPath);
        WCHAR* lastSlash = wcsrchr(dir, L'\\');
        if (lastSlash) *lastSlash = 0;
    }
    
    WCHAR* fileName = wcsrchr((WCHAR*)inputPath, L'\\');
    if (fileName) {
        fileName++;
    } else {
        fileName = (WCHAR*)inputPath;
    }
    
    wcscpy(name, fileName);
    WCHAR* dot = wcsrchr(name, L'.');
    if (dot) {
        *dot = 0;
    }
    
    if (wcsstr(encoder, L"265") || wcsstr(encoder, L"H.265")) {
        wsprintf(outputPath, L"%s\\%s_h265.mp4", dir, name);
    } else {
        wsprintf(outputPath, L"%s\\%s_h264.mp4", dir, name);
    }
}

static void ParseFFmpegProgress(const char* line, float* currentTime, float totalDuration, int* progress) {
    *progress = -1;
    
    const char* timePos = strstr(line, "time=");
    if (timePos) {
        timePos += 5;
        int hours = 0, minutes = 0;
        float seconds = 0;
        if (sscanf(timePos, "%d:%d:%f", &hours, &minutes, &seconds) == 3) {
            *currentTime = hours * 3600 + minutes * 60 + seconds;
            
            if (totalDuration > 0) {
                *progress = (int)((*currentTime / totalDuration) * 100);
                if (*progress > 100) *progress = 100;
            } else {
                *progress = (int)(*currentTime / 10);
                if (*progress > 100) *progress = 100;
            }
            return;
        }
    }
    
    const char* framePos = strstr(line, "frame=");
    if (framePos) {
        int frame = 0;
        if (sscanf(framePos + 6, "%d", &frame) == 1) {
            if (totalDuration > 0) {
                *progress = (int)((*currentTime / totalDuration) * 100);
                if (*progress > 100) *progress = 100;
            } else {
                *progress = (int)(frame / 30);
                if (*progress > 100) *progress = 100;
            }
            return;
        }
    }
}

static DWORD WINAPI ProgressTimerThread(LPVOID param) {
    HWND hwnd = (HWND)param;
    
    while (g_isCompressing && g_currentFileIndex >= 0 && g_currentFileIndex < g_fileCount) {
        float fileDuration = g_files[g_currentFileIndex].duration;
        
        if (fileDuration > 0) {
            int fileProgress = g_files[g_currentFileIndex].progress;
            
            float completedDuration = 0;
            for (int j = 0; j < g_currentFileIndex; j++) {
                completedDuration += g_files[j].duration;
            }
            
            int totalProgress = 0;
            if (g_totalDuration > 0) {
                float currentFileProgress = (fileProgress / 100.0f) * fileDuration;
                totalProgress = (int)(((completedDuration + currentFileProgress) / g_totalDuration) * 100);
            } else {
                totalProgress = fileProgress;
            }
            
            PostMessage(hwnd, WM_COMPRESS_PROGRESS, g_currentFileIndex, MAKELPARAM(fileProgress, totalProgress));
        }
        
        Sleep(100);
    }
    
    return 0;
}

static DWORD WINAPI CompressThread(LPVOID param) {
    HWND hwnd = (HWND)param;
    int crf = (int)SendMessage(g_hwndSlider, TBM_GETPOS, 0, 0);
    
    WCHAR encoder[64];
    GetWindowText(g_hwndCombo, encoder, 64);
    
    const WCHAR* codec = L"libx264";
    if (wcsstr(encoder, L"265")) {
        codec = L"libx265";
    }
    
    int speedIndex = (int)SendMessage(g_hwndSpeedCombo, CB_GETCURSEL, 0, 0);
    const WCHAR* preset = L"veryfast";
    switch (speedIndex) {
        case 0: preset = L"ultrafast"; break;
        case 1: preset = L"superfast"; break;
        case 2: preset = L"veryfast"; break;
        case 3: preset = L"faster"; break;
        case 4: preset = L"fast"; break;
        case 5: preset = L"medium"; break;
        case 6: preset = L"slow"; break;
    }
    
    g_totalDuration = 0;
    for (int i = 0; i < g_fileCount; i++) {
        if (!g_files[i].isCompleted) {
            g_totalDuration += g_files[i].duration;
        }
    }
    
    HANDLE hProgressThread = CreateThread(NULL, 0, ProgressTimerThread, hwnd, 0, NULL);
    
    float completedDuration = 0;
    
    for (int i = 0; i < g_fileCount; i++) {
        if (g_files[i].isCompleted || g_cancelCompression) continue;
        
        g_currentFileIndex = i;
        g_files[i].isCompressing = TRUE;
        g_files[i].progress = 0;
        g_files[i].duration = GetVideoDuration(g_files[i].filePath);
        
        WCHAR outputPath[MAX_PATH];
        GetOutputPath(g_files[i].filePath, outputPath, encoder);
        
        WCHAR cmd[2048];
        wsprintf(cmd, L"ffmpeg.exe -y -i \"%s\" -c:v %s -crf %d -preset %s -c:a aac -b:a 128k -threads 0 \"%s\"",
            g_files[i].filePath, codec, crf, preset, outputPath);
        
        SECURITY_ATTRIBUTES sa = {0};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        
        HANDLE hReadPipe, hWritePipe;
        CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
        
        STARTUPINFO si = {0};
        si.cb = sizeof(STARTUPINFO);
        si.dwFlags = STARTF_USESTDERR | STARTF_USESHOWWINDOW;
        si.hStdError = hWritePipe;
        si.wShowWindow = SW_HIDE;
        
        PROCESS_INFORMATION pi = {0};
        
        DWORD startTime = GetTickCount();
        BOOL processStarted = FALSE;
        
        if (CreateProcess(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            processStarted = TRUE;
            CloseHandle(hWritePipe);
            
            char buffer[4096];
            DWORD bytesRead;
            DWORD lastProgressUpdate = GetTickCount();
            
            while (TRUE) {
                DWORD waitResult = WaitForSingleObject(pi.hProcess, 50);
                
                float elapsed = (GetTickCount() - startTime) / 1000.0f;
                if (g_files[i].duration > 0 && elapsed > 0) {
                    int progress = (int)((elapsed / g_files[i].duration) * 100);
                    if (progress > 100) progress = 100;
                    if (progress > g_files[i].progress) {
                        g_files[i].progress = progress;
                    }
                }
                
                if (waitResult == WAIT_OBJECT_0) {
                    break;
                }
                
                if (PeekNamedPipe(hReadPipe, NULL, 0, NULL, &bytesRead, NULL) && bytesRead > 0) {
                    ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
                }
            }
            
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        
        CloseHandle(hReadPipe);
        
        g_files[i].progress = 100;
        completedDuration += g_files[i].duration;
        g_files[i].isCompressing = FALSE;
        g_files[i].isCompleted = TRUE;
        
        HANDLE hOutFile = CreateFile(outputPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hOutFile != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER fileSize;
            GetFileSizeEx(hOutFile, &fileSize);
            g_files[i].compressedSize = fileSize.QuadPart;
            CloseHandle(hOutFile);
        } else {
            g_files[i].compressedSize = 0;
        }
        
        PostMessage(hwnd, WM_COMPRESS_COMPLETE, i, 0);
    }
    
    g_isCompressing = FALSE;
    if (hProgressThread) {
        WaitForSingleObject(hProgressThread, 1000);
        CloseHandle(hProgressThread);
    }
    
    PostMessage(hwnd, WM_COMPRESS_COMPLETE, -1, 0);
    
    return 0;
}

static void StartCompression(HWND hwnd) {
    if (g_fileCount == 0) {
        MessageBox(hwnd, L"请先添加要压缩的视频文件", L"提示", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    if (g_isCompressing) {
        return;
    }
    
    g_isCompressing = TRUE;
    g_cancelCompression = FALSE;
    EnableWindow(g_hwndBtnCompress, FALSE);
    
    for (int i = 0; i < g_fileCount; i++) {
        g_files[i].isCompleted = FALSE;
        g_files[i].progress = 0;
    }
    
    CreateThread(NULL, 0, CompressThread, hwnd, 0, NULL);
}

static void BrowseOutputDir(HWND hwnd) {
    BROWSEINFO bi = {0};
    bi.hwndOwner = hwnd;
    bi.lpszTitle = L"选择输出目录";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    if (pidl) {
        SHGetPathFromIDList(pidl, g_outputDir);
        CoTaskMemFree(pidl);
        
        SetWindowText(g_hwndOutputPath, g_outputDir);
        InvalidateRect(g_hwndOutputPath, NULL, TRUE);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            CreateControls(hwnd);
            DragAcceptFiles(hwnd, TRUE);
            break;
            
        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            UINT count = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
            
            for (UINT i = 0; i < count && g_fileCount < 100; i++) {
                WCHAR path[MAX_PATH];
                DragQueryFile(hDrop, i, path, MAX_PATH);
                
                DWORD attr = GetFileAttributes(path);
                if (attr & FILE_ATTRIBUTE_DIRECTORY) continue;
                
                wcscpy(g_files[g_fileCount].filePath, path);
                WCHAR* fileName = wcsrchr(path, L'\\');
                if (fileName) {
                    wcscpy(g_files[g_fileCount].fileName, fileName + 1);
                } else {
                    wcscpy(g_files[g_fileCount].fileName, path);
                }
                g_files[g_fileCount].isCompressing = FALSE;
                g_files[g_fileCount].isCompleted = FALSE;
                g_files[g_fileCount].progress = 0;
                g_files[g_fileCount].duration = GetVideoDuration(path);
                
                HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    LARGE_INTEGER fileSize;
                    GetFileSizeEx(hFile, &fileSize);
                    g_files[g_fileCount].sourceSize = fileSize.QuadPart;
                    g_files[g_fileCount].compressedSize = 0;
                    CloseHandle(hFile);
                } else {
                    g_files[g_fileCount].sourceSize = 0;
                    g_files[g_fileCount].compressedSize = 0;
                }
                
                g_fileCount++;
            }
            
            DragFinish(hDrop);
            UpdateFileList();
            break;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_BTN_ADD:
                    AddFiles(hwnd);
                    break;
                case IDC_BTN_REMOVE:
                    RemoveSelectedFile();
                    break;
                case IDC_BTN_CLEAR:
                    ClearAllFiles();
                    break;
                case IDC_BTN_COMPRESS:
                    StartCompression(hwnd);
                    break;
                case IDC_BTN_BROWSE:
                    BrowseOutputDir(hwnd);
                    break;
            }
            break;
            
        case WM_HSCROLL: {
            HWND hSlider = (HWND)lParam;
            if (hSlider == g_hwndSlider) {
                UpdateCRFDisplay(hwnd);
            }
            break;
        }
        
        case WM_COMPRESS_PROGRESS: {
            int index = (int)wParam;
            int fileProgress = LOWORD(lParam);
            int totalProgress = HIWORD(lParam);
            
            SendMessage(g_hwndProgress, PBM_SETPOS, totalProgress, 0);
            
            WCHAR status[512];
            wsprintf(status, L"正在压缩: %s (%d%%)", g_files[index].fileName, fileProgress);
            SetWindowText(g_hwndStatus, status);
            
            InvalidateRect(g_hwndList, NULL, FALSE);
            break;
        }
        
        case WM_COMPRESS_COMPLETE: {
            int index = (int)wParam;
            if (index == -1) {
                SendMessage(g_hwndProgress, PBM_SETPOS, 100, 0);
                SetWindowText(g_hwndStatus, L"压缩完成");
                EnableWindow(g_hwndBtnCompress, TRUE);
                MessageBox(hwnd, L"所有文件压缩完成！", L"完成", MB_OK | MB_ICONINFORMATION);
                SendMessage(g_hwndProgress, PBM_SETPOS, 0, 0);
                SetWindowText(g_hwndStatus, L"就绪");
            } else {
                InvalidateRect(g_hwndList, NULL, FALSE);
            }
            break;
        }
        
        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
            if (lpdis->CtlID == IDC_LIST_FILES) {
                int index = (int)lpdis->itemID;
                if (index == -1) break;
                
                RECT rc = lpdis->rcItem;
                HDC hdc = lpdis->hDC;
                
                COLORREF bgColor = COLOR_LIST_BG;
                COLORREF textColor = COLOR_TEXT;
                
                if (lpdis->itemState & ODS_SELECTED) {
                    bgColor = COLOR_LIST_ITEM_SEL;
                }
                
                HBRUSH hBrush = CreateSolidBrush(bgColor);
                FillRect(hdc, &rc, hBrush);
                DeleteObject(hBrush);
                
                if (index < g_fileCount) {
                    VideoFile* vf = &g_files[index];
                    
                    WCHAR sizeText[128];
                    if (vf->compressedSize > 0) {
                        double ratio = (double)(vf->sourceSize - vf->compressedSize) / vf->sourceSize * 100;
                        swprintf(sizeText, 128, L"%.1f%%", ratio);
                    } else {
                        wcscpy(sizeText, L"-");
                    }
                    
                    RECT textRc = rc;
                    textRc.left += 15;
                    textRc.top += 8;
                    textRc.right -= 120;
                    
                    SetTextColor(hdc, textColor);
                    SetBkMode(hdc, TRANSPARENT);
                    SelectObject(hdc, g_hFont);
                    DrawText(hdc, vf->fileName, -1, &textRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                    
                    RECT statusRc = rc;
                    statusRc.left += 15;
                    statusRc.top += 28;
                    statusRc.right -= 120;
                    
                    WCHAR status[64];
                    if (vf->isCompleted) {
                        wcscpy(status, L"✓ 已完成");
                        SetTextColor(hdc, COLOR_SUCCESS);
                    } else if (vf->isCompressing) {
                        wsprintf(status, L"压缩中 %d%%", vf->progress);
                        SetTextColor(hdc, COLOR_PRIMARY);
                    } else {
                        wcscpy(status, L"等待中");
                        SetTextColor(hdc, COLOR_TEXT_LIGHT);
                    }
                    
                    SelectObject(hdc, g_hFontSmall);
                    DrawText(hdc, status, -1, &statusRc, DT_LEFT | DT_SINGLELINE);
                    
                    RECT sizeRc = rc;
                    sizeRc.left = rc.right - 110;
                    sizeRc.top = rc.top + 8;
                    sizeRc.right = rc.right - 10;
                    
                    WCHAR sourceSize[32];
                    FormatSize(vf->sourceSize, sourceSize);
                    SetTextColor(hdc, COLOR_TEXT);
                    SelectObject(hdc, g_hFontSmall);
                    DrawText(hdc, sourceSize, -1, &sizeRc, DT_RIGHT | DT_SINGLELINE);
                    
                    RECT ratioRc = rc;
                    ratioRc.left = rc.right - 110;
                    ratioRc.top = rc.top + 28;
                    ratioRc.right = rc.right - 10;
                    
                    SetTextColor(hdc, vf->compressedSize > 0 ? COLOR_SUCCESS : COLOR_TEXT_LIGHT);
                    DrawText(hdc, sizeText, -1, &ratioRc, DT_RIGHT | DT_SINGLELINE);
                    
                    if (vf->isCompressing && vf->progress > 0) {
                        RECT progressBg = rc;
                        progressBg.left = rc.right - 120;
                        progressBg.top = rc.top + 15;
                        progressBg.right = rc.right - 15;
                        progressBg.bottom = rc.top + 35;
                        
                        HBRUSH hBgBrush = CreateSolidBrush(RGB(230, 230, 230));
                        FillRect(hdc, &progressBg, hBgBrush);
                        DeleteObject(hBgBrush);
                        
                        RECT progressFill = progressBg;
                        int fillWidth = (int)((progressBg.right - progressBg.left) * vf->progress / 100.0f);
                        progressFill.right = progressBg.left + fillWidth;
                        
                        HBRUSH hFillBrush = CreateSolidBrush(COLOR_PRIMARY);
                        FillRect(hdc, &progressFill, hFillBrush);
                        DeleteObject(hFillBrush);
                    }
                }
                
                HPEN hPen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
                HPEN hOldPen = SelectObject(hdc, hPen);
                MoveToEx(hdc, rc.left, rc.bottom - 1, NULL);
                LineTo(hdc, rc.right, rc.bottom - 1);
                SelectObject(hdc, hOldPen);
                DeleteObject(hPen);
            }
            break;
        }
        
        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT);
            SetTextColor(hdcStatic, COLOR_TEXT);
            return (INT_PTR)CreateSolidBrush(COLOR_BG);
        }
        
        case WM_CTLCOLORLISTBOX: {
            HDC hdcList = (HDC)wParam;
            SetBkMode(hdcList, TRANSPARENT);
            return (INT_PTR)CreateSolidBrush(COLOR_LIST_BG);
        }
        
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            
            RECT rcSidebar = {560, 0, 900, 600};
            HBRUSH hBrush = CreateSolidBrush(COLOR_SIDEBAR);
            FillRect(hdc, &rcSidebar, hBrush);
            DeleteObject(hBrush);
            
            HPEN hPen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
            HPEN hOldPen = SelectObject(hdc, hPen);
            MoveToEx(hdc, 560, 0, NULL);
            LineTo(hdc, 560, 600);
            SelectObject(hdc, hOldPen);
            DeleteObject(hPen);
            
            EndPaint(hwnd, &ps);
            break;
        }
        
        case WM_DESTROY:
            if (g_hFont) DeleteObject(g_hFont);
            if (g_hFontBold) DeleteObject(g_hFontBold);
            if (g_hFontSmall) DeleteObject(g_hFontSmall);
            PostQuitMessage(0);
            break;
            
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
