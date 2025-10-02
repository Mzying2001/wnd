#ifndef _WND_H_
#define _WND_H_

#include <windows.h>
#include <string>

#if !(defined(UNICODE) || defined(_UNICODE))
#define WND_USE_ANSI_WINDPROC
#endif

struct Msg
{
    UINT    uMsg;
    WPARAM  wParam;
    LPARAM  lParam;
};

template <typename TDerived>
class Wnd
{
    HWND    _hWnd;
    bool    _destroyed;
    WNDPROC _defWndProc;

private:
    static constexpr WCHAR
        _PROP_THIS[] = L"__Wnd_This_Ptr";

    struct _CreateParamWrapper
    {
        Wnd* pThis;
        LPVOID lpParam;
    };

private:
    static Wnd* GetThisFromHandle(HWND hWnd) noexcept
    {
        return reinterpret_cast<Wnd*>(::GetPropW(hWnd, _PROP_THIS));
    }

    static bool BindThisToHandle(HWND hWnd, Wnd* pThis) noexcept
    {
        return ::SetPropW(hWnd, _PROP_THIS, reinterpret_cast<HANDLE>(pThis)) != 0;
    }

    static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        Wnd* pThis = GetThisFromHandle(hWnd);

        if (pThis == nullptr &&
            (uMsg == WM_NCCREATE || uMsg == WM_CREATE))
        {
            auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            auto* pWrapper = reinterpret_cast<_CreateParamWrapper*>(pCreate->lpCreateParams);

            if (pWrapper != nullptr) {
                pThis = pWrapper->pThis;
                pThis->_hWnd = hWnd;
                BindThisToHandle(hWnd, pThis);
                pCreate->lpCreateParams = pWrapper->lpParam;
            }
        }

        if (pThis != nullptr)
        {
            Msg msg{ uMsg, wParam, lParam };
            LRESULT result = 0;

            if (!pThis->WndProc(msg, result)) {
                result = pThis->DefWndProc(msg);
            }
            if (uMsg == WM_NCDESTROY) {
                pThis->_destroyed = true;
            }
            return result;
        }

#if defined(WND_USE_ANSI_WINDPROC)
        return ::DefWindowProcA(hWnd, uMsg, wParam, lParam);
#else
        return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
#endif
    }

    static INT_PTR CALLBACK StaticDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        Wnd* pThis = GetThisFromHandle(hDlg);

        if (pThis == nullptr &&
            uMsg == WM_INITDIALOG)
        {
            auto* pWrapper = reinterpret_cast<_CreateParamWrapper*>(lParam);

            if (pWrapper != nullptr) {
                pThis = pWrapper->pThis;
                pThis->_hWnd = hDlg;
                BindThisToHandle(hDlg, pThis);
                lParam = reinterpret_cast<LPARAM>(pWrapper->lpParam);
            }
        }

        if (pThis != nullptr)
        {
            Msg msg{ uMsg, wParam, lParam };
            LRESULT unused = 0;

            bool result =
                pThis->WndProc(msg, unused);
            return result;
        }

        return FALSE;
    }

protected:
    Wnd() noexcept
        : _hWnd(NULL), _destroyed(false), _defWndProc(NULL)
    {
    }

#if defined(WND_USE_ANSI_WINDPROC)
    bool CreateHandle(
        DWORD dwExStyle,
        LPCSTR lpClassName,
        LPCSTR lpWindowName,
        DWORD dwStyle,
        int X,
        int Y,
        int nWidth,
        int nHeight,
        HWND hWndParent,
        HMENU hMenu,
        HINSTANCE hInstance,
        LPVOID lpParam) noexcept
    {
        if (_hWnd != NULL || lpClassName == nullptr) {
            return false;
        }

        WNDCLASSEXA wc = { 0 };
        wc.cbSize = sizeof(wc);

        if (!::GetClassInfoExA(hInstance, lpClassName, &wc)) {
            return false;
        }
        else {
            _defWndProc = wc.lpfnWndProc;
        }

        std::string className = lpClassName;
        className += "_Wnd_Class";

        if (!::GetClassInfoExA(hInstance, className.c_str(), &wc))
        {
            wc.lpfnWndProc = StaticWndProc;
            wc.lpszClassName = className.c_str();

            if (!::RegisterClassExA(&wc)) {
                return false;
            }
        }

        _CreateParamWrapper createParam = { this, lpParam };

        _hWnd = ::CreateWindowExA(
            dwExStyle,
            className.c_str(),
            lpWindowName,
            dwStyle,
            X,
            Y,
            nWidth,
            nHeight,
            hWndParent,
            hMenu,
            hInstance,
            &createParam);

        return _hWnd != NULL;
    }

    bool CreateDlg(
        HINSTANCE hInstance,
        LPCSTR lpTemplateName,
        HWND hWndParent,
        LPARAM dwInitParam = 0)
    {
        if (_hWnd != NULL) {
            return false;
        }

        _CreateParamWrapper initParam{
            this, reinterpret_cast<LPVOID>(dwInitParam) };

        _defWndProc = DefDlgProcA;

        _hWnd = CreateDialogParamA(
            hInstance,
            lpTemplateName,
            hWndParent,
            StaticDlgProc,
            reinterpret_cast<LPARAM>(&initParam));

        return _hWnd != NULL;
    }
#else
    bool CreateHandle(
        DWORD dwExStyle,
        LPCWSTR lpClassName,
        LPCWSTR lpWindowName,
        DWORD dwStyle,
        int X,
        int Y,
        int nWidth,
        int nHeight,
        HWND hWndParent,
        HMENU hMenu,
        HINSTANCE hInstance,
        LPVOID lpParam) noexcept
    {
        if (_hWnd != NULL || lpClassName == nullptr) {
            return false;
        }

        WNDCLASSEXW wc = { 0 };
        wc.cbSize = sizeof(wc);

        if (!::GetClassInfoExW(hInstance, lpClassName, &wc)) {
            return false;
        }
        else {
            _defWndProc = wc.lpfnWndProc;
        }

        std::wstring className = lpClassName;
        className += L"_Wnd_Class";

        if (!::GetClassInfoExW(hInstance, className.c_str(), &wc))
        {
            wc.lpfnWndProc = StaticWndProc;
            wc.lpszClassName = className.c_str();

            if (!::RegisterClassExW(&wc)) {
                return false;
            }
        }

        _CreateParamWrapper createParam = { this, lpParam };

        _hWnd = ::CreateWindowExW(
            dwExStyle,
            className.c_str(),
            lpWindowName,
            dwStyle,
            X,
            Y,
            nWidth,
            nHeight,
            hWndParent,
            hMenu,
            hInstance,
            &createParam);

        return _hWnd != NULL;
    }

    bool CreateDlg(
        HINSTANCE hInstance,
        LPCWSTR lpTemplateName,
        HWND hWndParent,
        LPARAM dwInitParam = 0)
    {
        if (_hWnd != NULL) {
            return false;
        }

        _CreateParamWrapper initParam{
            this, reinterpret_cast<LPVOID>(dwInitParam) };

        _defWndProc = DefDlgProcW;

        _hWnd = CreateDialogParamW(
            hInstance,
            lpTemplateName,
            hWndParent,
            StaticDlgProc,
            reinterpret_cast<LPARAM>(&initParam));

        return _hWnd != NULL;
    }
#endif

    LRESULT DefWndProc(const Msg& msg)
    {
#if defined(WND_USE_ANSI_WINDPROC)
        return _defWndProc != nullptr
            ? ::CallWindowProcA(_defWndProc, _hWnd, msg.uMsg, msg.wParam, msg.lParam)
            : ::DefWindowProcA(_hWnd, msg.uMsg, msg.wParam, msg.lParam);
#else
        return _defWndProc != nullptr
            ? ::CallWindowProcW(_defWndProc, _hWnd, msg.uMsg, msg.wParam, msg.lParam)
            : ::DefWindowProcW(_hWnd, msg.uMsg, msg.wParam, msg.lParam);
#endif
    }

    bool WndProc(const Msg& msg, LRESULT& result)
    {
        static_assert(&Wnd::WndProc != &TDerived::WndProc,
            "Derived class must implement WndProc method.");
        return static_cast<TDerived*>(this)->TDerived::WndProc(msg, result);
    }

public:
    Wnd(const Wnd&) = delete;
    Wnd& operator=(const Wnd&) = delete;
    Wnd& operator=(Wnd&&) = delete;

    Wnd(Wnd&& other) noexcept
        : _hWnd(other._hWnd), _destroyed(other._destroyed), _defWndProc(other._defWndProc)
    {
        if (!_destroyed && _hWnd != NULL) {
            BindThisToHandle(_hWnd, this);
        }
        other._hWnd = NULL;
        other._destroyed = true;
        other._defWndProc = NULL;
    }

    ~Wnd() noexcept
    {
        if (!_destroyed && _hWnd != NULL)
        {
            ::RemovePropW(_hWnd, _PROP_THIS);
            ::DestroyWindow(_hWnd);
            _hWnd = NULL;
            _destroyed = true;
        }
    }

    HWND Handle() const noexcept
    {
        return _hWnd;
    }

    bool IsDestroyed() const noexcept
    {
        return _destroyed;
    }

    LRESULT SendMessageA(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0) noexcept
    {
        return ::SendMessageA(_hWnd, uMsg, wParam, lParam);
    }

    LRESULT SendMessageW(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0) noexcept
    {
        return ::SendMessageW(_hWnd, uMsg, wParam, lParam);
    }

    LRESULT PostMessageA(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0) noexcept
    {
        return ::PostMessageA(_hWnd, uMsg, wParam, lParam);
    }

    LRESULT PostMessageW(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0) noexcept
    {
        return ::PostMessageW(_hWnd, uMsg, wParam, lParam);
    }

    bool EndDialog(INT_PTR nResult)
    {
        bool result = ::EndDialog(_hWnd, nResult);

        if (result)
            _destroyed = true;
        return result;
    }
};

#endif // !_WND_H_
