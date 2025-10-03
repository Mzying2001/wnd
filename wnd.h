#ifndef _WND_H_
#define _WND_H_

#include <windows.h>
#include <string>

#if !(defined(UNICODE) || defined(_UNICODE))
#define WND_USE_ANSI_WINDPROC
#endif

struct Msg
{
    UINT   uMsg;
    WPARAM wParam;
    LPARAM lParam;
};

template <typename TDerived>
class Wnd
{
    template <typename>
    friend class Dlg;

    HWND    _hWnd;
    WNDPROC _defWndProc;
    bool    _destroyed;
    bool    _reserved[sizeof(void*) / sizeof(bool) - 1];

private:
    static constexpr WCHAR
        _PROP_THIS[] = L"__Wnd_This_Ptr";

    struct _CreateParam
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

        if ((uMsg == WM_NCCREATE || uMsg == WM_CREATE) && lParam != 0)
        {
            auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            auto* pParam = reinterpret_cast<_CreateParam*>(pCreate->lpCreateParams);

            if (pParam != nullptr)
            {
                if (pThis == nullptr) {
                    pThis = pParam->pThis;
                    pThis->_hWnd = hWnd;
                    BindThisToHandle(hWnd, pThis);
                }
                pCreate->lpCreateParams = pParam->lpParam;
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

protected:
    Wnd() noexcept
        : _hWnd(NULL), _defWndProc(NULL), _destroyed(false), _reserved{}
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

        _CreateParam createParam = { this, lpParam };

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

    bool AttachHandle(HWND hWnd) noexcept
    {
        if (_hWnd != NULL || hWnd == NULL) {
            return false;
        }

        if (!BindThisToHandle(hWnd, this)) {
            return false;
        }

        WNDPROC proc = reinterpret_cast<WNDPROC>(
            ::GetWindowLongPtrA(hWnd, GWLP_WNDPROC));

        if (proc == StaticWndProc) {
            return false;
        }

        _hWnd = hWnd;
        _defWndProc = proc;

        ::SetWindowLongPtrA(hWnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(StaticWndProc));
        return true;
    }

    LRESULT DefWndProc(Msg& msg)
    {
        return _defWndProc != nullptr
            ? ::CallWindowProcA(_defWndProc, _hWnd, msg.uMsg, msg.wParam, msg.lParam)
            : ::DefWindowProcA(_hWnd, msg.uMsg, msg.wParam, msg.lParam);
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

        _CreateParam createParam = { this, lpParam };

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

    bool AttachHandle(HWND hWnd) noexcept
    {
        if (_hWnd != NULL || hWnd == NULL) {
            return false;
        }

        if (!BindThisToHandle(hWnd, this)) {
            return false;
        }

        WNDPROC proc = reinterpret_cast<WNDPROC>(
            ::GetWindowLongPtrW(hWnd, GWLP_WNDPROC));

        if (proc == StaticWndProc) {
            return false;
        }

        _hWnd = hWnd;
        _defWndProc = proc;

        ::SetWindowLongPtrW(hWnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(StaticWndProc));
        return true;
    }

    LRESULT DefWndProc(Msg& msg)
    {
        return _defWndProc != nullptr
            ? ::CallWindowProcW(_defWndProc, _hWnd, msg.uMsg, msg.wParam, msg.lParam)
            : ::DefWindowProcW(_hWnd, msg.uMsg, msg.wParam, msg.lParam);
    }
#endif

    bool WndProc(Msg& msg, LRESULT& result)
    {
        static_assert(&Wnd::WndProc != &TDerived::WndProc,
            "Derived class must implement WndProc method.");
        return static_cast<TDerived*>(this)->WndProc(msg, result);
    }

public:
    Wnd(const Wnd&) = delete;
    Wnd& operator=(Wnd&&) = delete;
    Wnd& operator=(const Wnd&) = delete;

    Wnd(Wnd&& other) noexcept
        : _hWnd(other._hWnd), _defWndProc(other._defWndProc), _destroyed(other._destroyed), _reserved{}
    {
        if (_hWnd != NULL && !_destroyed) {
            BindThisToHandle(_hWnd, this);
        }
        other._hWnd = NULL;
        other._defWndProc = nullptr;
        other._destroyed = false;
    }

    ~Wnd() noexcept
    {
        if (_hWnd != NULL && !_destroyed)
        {
#if defined(WND_USE_ANSI_WINDPROC)
            ::SetWindowLongPtrA(_hWnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(_defWndProc ? _defWndProc : ::DefWindowProcA));
#else
            ::SetWindowLongPtrW(_hWnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(_defWndProc ? _defWndProc : ::DefWindowProcW));
#endif
            ::RemovePropW(_hWnd, _PROP_THIS);
            ::DestroyWindow(_hWnd);
        }
        _destroyed = true;
    }

    HWND Handle() const noexcept
    {
        return _hWnd;
    }

    bool IsDestroyed() const noexcept
    {
        return _destroyed;
    }

    LRESULT SendMessageA(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0)
    {
        return ::SendMessageA(_hWnd, uMsg, wParam, lParam);
    }

    LRESULT SendMessageW(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0)
    {
        return ::SendMessageW(_hWnd, uMsg, wParam, lParam);
    }

    BOOL PostMessageA(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0) noexcept
    {
        return ::PostMessageA(_hWnd, uMsg, wParam, lParam);
    }

    BOOL PostMessageW(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0) noexcept
    {
        return ::PostMessageW(_hWnd, uMsg, wParam, lParam);
    }
};

template <typename TDerived>
class Dlg : public Wnd<TDerived>
{
    using TBase = Wnd<TDerived>;

    bool _isModal;
    bool _reserved[sizeof(void*) / sizeof(bool) - 1];

private:
    static INT_PTR CALLBACK StaticDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        TBase* pThis = TBase::GetThisFromHandle(hDlg);

        if (pThis == nullptr &&
            uMsg == WM_INITDIALOG)
        {
            auto* pParam = reinterpret_cast<
                typename TBase::_CreateParam*>(lParam);

            if (pParam != nullptr) {
                pThis = pParam->pThis;
                pThis->_hWnd = hDlg;
                TBase::BindThisToHandle(hDlg, pThis);
                lParam = reinterpret_cast<LPARAM>(pParam->lpParam);
            }
        }

        if (pThis != nullptr)
        {
            Msg msg{ uMsg, wParam, lParam };
            LRESULT unused = 0;

            bool result =
                pThis->WndProc(msg, unused);

            if (uMsg == WM_NCDESTROY) {
                pThis->_destroyed = true;
            }
            return result;
        }

        return FALSE;
    }

protected:
    Dlg() noexcept
        : TBase(), _isModal(false), _reserved{}
    {
    }

#if defined(WND_USE_ANSI_WINDPROC)
    bool CreateDlg(
        HINSTANCE hInstance,
        LPCSTR lpTemplateName,
        HWND hWndParent,
        LPARAM dwInitParam = 0) noexcept
    {
        TBase& self = *static_cast<TBase*>(this);

        if (self._hWnd != NULL) {
            return false;
        }

        _isModal = false;
        self._defWndProc = ::DefDlgProcA;

        typename TBase::_CreateParam initParam{
            this, reinterpret_cast<LPVOID>(dwInitParam) };

        ::CreateDialogParamA(
            hInstance,
            lpTemplateName,
            hWndParent,
            StaticDlgProc,
            reinterpret_cast<LPARAM>(&initParam));

        return self._hWnd != NULL;
    }

    INT_PTR CreateModal(
        HINSTANCE hInstance,
        LPCWSTR lpTemplateName,
        HWND hWndParent,
        LPARAM dwInitParam = 0) noexcept
    {
        TBase& self = *static_cast<TBase*>(this);

        if (self._hWnd != NULL) {
            return static_cast<INT_PTR>(-1);
        }

        _isModal = true;
        self._defWndProc = ::DefDlgProcA;

        typename TBase::_CreateParam initParam{
            this, reinterpret_cast<LPVOID>(dwInitParam) };

        return ::DialogBoxParamA(
            hInstance,
            lpTemplateName,
            hWndParent,
            StaticDlgProc,
            reinterpret_cast<LPARAM>(&initParam));
    }
#else
    bool CreateDlg(
        HINSTANCE hInstance,
        LPCWSTR lpTemplateName,
        HWND hWndParent,
        LPARAM dwInitParam = 0) noexcept
    {
        TBase& self = *static_cast<TBase*>(this);

        if (self._hWnd != NULL) {
            return false;
        }

        _isModal = false;
        self._defWndProc = ::DefDlgProcW;

        typename TBase::_CreateParam initParam{
            this, reinterpret_cast<LPVOID>(dwInitParam) };

        ::CreateDialogParamW(
            hInstance,
            lpTemplateName,
            hWndParent,
            StaticDlgProc,
            reinterpret_cast<LPARAM>(&initParam));

        return self._hWnd != NULL;
    }

    INT_PTR CreateModal(
        HINSTANCE hInstance,
        LPCWSTR lpTemplateName,
        HWND hWndParent,
        LPARAM dwInitParam = 0) noexcept
    {
        TBase& self = *static_cast<TBase*>(this);

        if (self._hWnd != NULL) {
            return static_cast<INT_PTR>(-1);
        }

        _isModal = true;
        self._defWndProc = ::DefDlgProcW;

        typename TBase::_CreateParam initParam{
            this, reinterpret_cast<LPVOID>(dwInitParam) };

        return ::DialogBoxParamW(
            hInstance,
            lpTemplateName,
            hWndParent,
            StaticDlgProc,
            reinterpret_cast<LPARAM>(&initParam));
    }
#endif

public:
    Dlg(Dlg&& other) noexcept
        : TBase(std::move(other)), _isModal(other._isModal), _reserved{}
    {
        other._isModal = false;
    }

    ~Dlg() noexcept
    {
        TBase& self = *static_cast<TBase*>(this);

        if (self._hWnd != NULL && !self._destroyed)
        {
            ::RemovePropW(self._hWnd, TBase::_PROP_THIS);
            _isModal ? ::EndDialog(self._hWnd, 0) : ::DestroyWindow(self._hWnd);
        }
        self._destroyed = true;
    }

    bool IsModal() const noexcept
    {
        return _isModal;
    }

    bool EndDialog(INT_PTR nResult) noexcept
    {
        TBase& self = *static_cast<TBase*>(this);
        bool result = ::EndDialog(self._hWnd, nResult);

        if (result) {
            self._destroyed = true;
        }
        return result;
    }
};

#endif // !_WND_H_
