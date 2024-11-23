#ifndef _WND_H_
#define _WND_H_

#include <windows.h>
#include <cassert>

#define PROPNAME_WNDPTR TEXT("__WNDPTR")

template <typename TDerived>
struct Wnd {
    HWND hWnd;
    WNDPROC oldproc;

    explicit Wnd(HWND hWnd)
        : hWnd(hWnd)
    {
        assert(hWnd != NULL);

        ::SetProp(hWnd, PROPNAME_WNDPTR,
                  reinterpret_cast<HANDLE>(this));

        oldproc = reinterpret_cast<WNDPROC>(::SetWindowLongPtr(
            hWnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>((WNDPROC)[](HWND h, UINT m, WPARAM w, LPARAM l)->LRESULT {
                Wnd *pWnd = reinterpret_cast<Wnd *>(::GetProp(h, PROPNAME_WNDPTR));
                if (pWnd) {
                    LRESULT result = 0;
                    auto *pDerived = static_cast<TDerived *>(pWnd);
                    return pDerived->WndProc(m, w, l, result) ? result : pDerived->DefWindowProc(m, w, l);
                } else
                    return ::DefWindowProc(h, m, w, l);
            })));
    }
    ~Wnd()
    {
        if (hWnd != NULL) {
            DestroyWindow();
        }
    }

    Wnd(const Wnd &)            = delete;
    Wnd &operator=(const Wnd &) = delete;

    Wnd(Wnd &&other)
    {
        assert(&other != this);

        hWnd    = other.hWnd;
        oldproc = other.oldproc;

        ::SetProp(other.hWnd, PROPNAME_WNDPTR,
                  reinterpret_cast<HANDLE>(this));

        other.hWnd    = NULL;
        other.oldproc = nullptr;
    }
    Wnd &operator=(Wnd &&other)
    {
        if (&other == this) {
            return *this;
        }
        if (hWnd != NULL) {
            DestroyWindow();
        }

        hWnd    = other.hWnd;
        oldproc = other.oldproc;

        ::SetProp(other.hWnd, PROPNAME_WNDPTR,
                  reinterpret_cast<HANDLE>(this));

        other.hWnd    = NULL;
        other.oldproc = nullptr;
        return *this;
    }
    bool operator==(const Wnd &other) const
    {
        return hWnd == other.hWnd;
    }
    bool operator!=(const Wnd &other) const
    {
        return hWnd != other.hWnd;
    }
    explicit operator HWND() const
    {
        return hWnd;
    }

    LRESULT DefWindowProcA(UINT msg, WPARAM wParam, LPARAM lParam) const
    {
        WNDPROC wndproc = oldproc ? oldproc : ::DefWindowProcA;
        return wndproc(hWnd, msg, wParam, lParam);
    }
    LRESULT DefWindowProcW(UINT msg, WPARAM wParam, LPARAM lParam) const
    {
        WNDPROC wndproc = oldproc ? oldproc : ::DefWindowProcW;
        return wndproc(hWnd, msg, wParam, lParam);
    }
    LRESULT SendMessageA(UINT msg, WPARAM wParam, LPARAM lParam) const
    {
        return ::SendMessageA(hWnd, msg, wParam, lParam);
    }
    LRESULT SendMessageW(UINT msg, WPARAM wParam, LPARAM lParam) const
    {
        return ::SendMessageW(hWnd, msg, wParam, lParam);
    }
    BOOL PostMessageA(UINT msg, WPARAM wParam, LPARAM lParam) const
    {
        return ::PostMessageA(hWnd, msg, wParam, lParam);
    }
    BOOL PostMessageW(UINT msg, WPARAM wParam, LPARAM lParam) const
    {
        return ::PostMessageW(hWnd, msg, wParam, lParam);
    }
    LONG GetWindowLongA(int nIndex) const
    {
        return ::GetWindowLongA(hWnd, nIndex);
    }
    LONG GetWindowLongW(int nIndex) const
    {
        return ::GetWindowLongW(hWnd, nIndex);
    }
    LONG SetWindowLongA(int nIndex, LONG dwNewLong) const
    {
        return ::SetWindowLongA(hWnd, nIndex, dwNewLong);
    }
    LONG SetWindowLongW(int nIndex, LONG dwNewLong) const
    {
        return ::SetWindowLongW(hWnd, nIndex, dwNewLong);
    }
    LONG_PTR GetWindowLongPtrA(int nIndex) const
    {
        return ::GetWindowLongPtrA(hWnd, nIndex);
    }
    LONG_PTR GetWindowLongPtrW(int nIndex) const
    {
        return ::GetWindowLongPtrW(hWnd, nIndex);
    }
    LONG_PTR SetWindowLongPtrA(int nIndex, LONG_PTR dwNewLong) const
    {
        return ::SetWindowLongPtrA(hWnd, nIndex, dwNewLong);
    }
    LONG_PTR SetWindowLongPtrW(int nIndex, LONG_PTR dwNewLong) const
    {
        return ::SetWindowLongPtrW(hWnd, nIndex, dwNewLong);
    }
    HANDLE GetPropA(LPCSTR lpString) const
    {
        return ::GetPropA(hWnd, lpString);
    }
    HANDLE GetPropW(LPCWSTR lpString) const
    {
        return ::GetPropW(hWnd, lpString);
    }
    BOOL SetPropA(LPCSTR lpString, HANDLE hData) const
    {
        return ::SetPropA(hWnd, lpString, hData);
    }
    BOOL SetPropW(LPCWSTR lpString, HANDLE hData) const
    {
        return ::SetPropW(hWnd, lpString, hData);
    }
    BOOL SetWindowTextA(LPCSTR lpString) const
    {
        return ::SetWindowTextA(hWnd, lpString);
    }
    BOOL SetWindowTextW(LPCWSTR lpString) const
    {
        return ::SetWindowTextW(hWnd, lpString);
    }
    int GetWindowTextLengthA() const
    {
        return ::GetWindowTextLengthA(hWnd);
    }
    int GetWindowTextLengthW() const
    {
        return ::GetWindowTextLengthW(hWnd);
    }
    int GetWindowTextA(LPSTR lpString, int nMaxCount) const
    {
        return ::GetWindowTextA(hWnd, lpString, nMaxCount);
    }
    int GetWindowTextW(LPWSTR lpString, int nMaxCount) const
    {
        return ::GetWindowTextW(hWnd, lpString, nMaxCount);
    }
    HWND SetParent(HWND hWndNewParent) const
    {
        return ::SetParent(hWnd, hWndNewParent);
    }
    HWND GetParent() const
    {
        return ::GetParent(hWnd);
    }
    BOOL ShowWindow(int nCmdShow) const
    {
        return ::ShowWindow(hWnd, nCmdShow);
    }
    BOOL DestroyWindow() const
    {
        return ::DestroyWindow(hWnd);
    }
    BOOL EnableWindow(BOOL bEnable) const
    {
        return ::EnableWindow(hWnd, bEnable);
    }
    BOOL UpdateWindow() const
    {
        return ::UpdateWindow(hWnd);
    }
    BOOL ValidateRect(const RECT *lpRect) const
    {
        return ::ValidateRect(hWnd, lpRect);
    }
    BOOL InvalidateRect(const RECT *lpRect, BOOL bErase) const
    {
        return ::InvalidateRect(hWnd, lpRect, bErase);
    }
    BOOL MoveWindow(int X, int Y, int nWidth, int nHeight, BOOL bRepaint) const
    {
        return ::MoveWindow(hWnd, X, Y, nWidth, nHeight, bRepaint);
    }
    BOOL SetWindowPos(HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags) const
    {
        return ::SetWindowPos(hWnd, hWndInsertAfter, X, Y, cx, cy, uFlags);
    }
    BOOL BringWindowToTop() const
    {
        return ::BringWindowToTop(hWnd);
    }
    BOOL IsWindowVisible() const
    {
        return ::IsWindowVisible(hWnd);
    }
    BOOL GetWindowRect(LPRECT lpRect) const
    {
        return ::GetWindowRect(hWnd, lpRect);
    }
    BOOL GetClientRect(LPRECT lpRect) const
    {
        return ::GetClientRect(hWnd, lpRect);
    }
    BOOL ScreenToClient(LPPOINT lpPoint) const
    {
        return ::ScreenToClient(hWnd, lpPoint);
    }
    BOOL ClientToScreen(LPPOINT lpPoint) const
    {
        return ::ClientToScreen(hWnd, lpPoint);
    }
    HWND GetDlgItem(int nIDDlgItem) const
    {
        return ::GetDlgItem(hWnd, nIDDlgItem);
    }
    int GetDlgCtrlID() const
    {
        return ::GetDlgCtrlID(hWnd);
    }
    BOOL FlashWindow(BOOL bInvert) const
    {
        return ::FlashWindow(hWnd, bInvert);
    }
    HDC BeginPaint(LPPAINTSTRUCT lpPaint) const
    {
        return ::BeginPaint(hWnd, lpPaint);
    }
    BOOL EndPaint(const PAINTSTRUCT *lpPaint) const
    {
        return ::EndPaint(hWnd, lpPaint);
    }
    HDC GetDC() const
    {
        return ::GetDC(hWnd);
    }
    HDC GetDCEx(HRGN hrgnClip, DWORD flags) const
    {
        return ::GetDCEx(hWnd, hrgnClip, flags);
    }
    int ReleaseDC(HDC hDC) const
    {
        return ::ReleaseDC(hWnd, hDC);
    }
    int GetUpdateRgn(HRGN hRgn, BOOL bErase) const
    {
        return ::GetUpdateRgn(hWnd, hRgn, bErase);
    }
    BOOL GetUpdateRect(LPRECT lpRect, BOOL bErase) const
    {
        return ::GetUpdateRect(hWnd, lpRect, bErase);
    }
    HWND SetCapture() const
    {
        return ::SetCapture(hWnd);
    }
    BOOL ReleaseCapture() const
    {
        return ::ReleaseCapture();
    }
    HWND GetFocus() const
    {
        return ::GetFocus();
    }
    HWND SetFocus() const
    {
        return ::SetFocus(hWnd);
    }
    HMENU GetSystemMenu(BOOL bRevert) const
    {
        return ::GetSystemMenu(hWnd, bRevert);
    }
    void DragAcceptFiles(BOOL fAccept) const
    {
        ::DragAcceptFiles(hWnd, fAccept);
    }
    BOOL SetLayeredWindowAttributes(COLORREF crKey, BYTE bAlpha, DWORD dwFlags) const
    {
        return ::SetLayeredWindowAttributes(hWnd, crKey, bAlpha, dwFlags);
    }

    // bool WndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT &result)
    // {
    //     return false;
    // }
};

#define DECLARE_CONTROL(_class, _lpClassName, _dwStyle, _dwExStyle)                               \
    class _class : public Wnd<_class>                                                             \
    {                                                                                             \
        using TBase = Wnd<_class>;                                                                \
                                                                                                  \
    public:                                                                                       \
        _class(HWND hParent, int id, LPCTSTR lpWindowName = TEXT(#_class),                        \
               int x = 0, int y = 0, int cx = 100, int cy = 100, LPVOID lpParam = NULL)           \
            : TBase(CreateWindowEx(_dwExStyle, _lpClassName, lpWindowName, _dwStyle,              \
                                   x, y, cx, cy,                                                  \
                                   hParent, (HMENU)(LONG_PTR)id, GetModuleHandle(NULL), lpParam)) \
        {                                                                                         \
        }                                                                                         \
        virtual ~_class()                                                                         \
        {                                                                                         \
        }                                                                                         \
        _class(_class &&other) : TBase(std::move(other))                                          \
        {                                                                                         \
        }                                                                                         \
        _class &operator=(_class &&other)                                                         \
        {                                                                                         \
            TBase::operator=(std::move(other));                                                   \
            return *this;                                                                         \
        }                                                                                         \
        virtual bool WndProc(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT &result)             \
        {                                                                                         \
            return false;                                                                         \
        }                                                                                         \
    }

#endif // _WND_H_
