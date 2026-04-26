/**
 * @file wnd.h
 * @brief Header-only CRTP wrappers (`Wnd<T>` and `Dlg<T>`) for Win32 windows
 *        and dialog boxes.
 *
 * The two templates let derived types receive Win32 messages through a single
 * virtual-like `bool WndProc(Msg&, LRESULT&)` callback while the base classes
 * handle window-class registration, subclassing, prop-table dispatch and
 * lifetime management.
 *
 * Character-set selection: defining `WND_USE_ANSI_API` or
 * `WND_USE_UNICODE_API` before including this header forces the corresponding
 * Win32 variants (the two are mutually exclusive). Without an explicit choice,
 * the header falls back to the ambient `UNICODE` / `_UNICODE` macros and uses
 * ANSI when neither is defined. See the `WND_USE_ANSI_API` documentation
 * below for the full selection rules.
 */

#pragma once

#ifndef WND_H_INCLUDED
#define WND_H_INCLUDED

#include <windows.h>
#include <cstdint>
#include <string>
#include <type_traits>

/**
 * @def WND_USE_ANSI_API
 * @brief When defined (either by the user or auto-detected from `UNICODE` /
 *        `_UNICODE`), switches every Win32 dispatch in this header to the
 *        ANSI (`*A`) variant. Otherwise the Unicode (`*W`) variant is used.
 *
 * Selection rules, in priority order:
 *  1. If the user defines `WND_USE_ANSI_API` and `WND_USE_UNICODE_API`
 *     simultaneously, a compile error is emitted — the choice is mutually
 *     exclusive.
 *  2. If exactly one of `WND_USE_ANSI_API` / `WND_USE_UNICODE_API` is
 *     defined by the user, that selection is honoured.
 *  3. Otherwise the header inspects `UNICODE` / `_UNICODE` and defines
 *     `WND_USE_UNICODE_API` when either is set, or `WND_USE_ANSI_API`
 *     when neither is set.
 *
 * After the header has been parsed exactly one of the two macros is
 * always defined, so external tooling (Doxygen, IDE indexers, downstream
 * `#ifdef` checks) can rely on either.
 *
 * @def WND_USE_UNICODE_API
 * @brief User-facing opt-in counterpart of `WND_USE_ANSI_API`. Define this
 *        to force the Unicode (`*W`) Win32 variants regardless of the
 *        ambient `UNICODE` macro state. See `WND_USE_ANSI_API` for the full
 *        selection rules. The macro is consumed only at header-parse time;
 *        downstream code in this file checks `WND_USE_ANSI_API` exclusively.
 */
#if defined(WND_USE_ANSI_API) && defined(WND_USE_UNICODE_API)
#error "WND_USE_ANSI_API and WND_USE_UNICODE_API are mutually exclusive"
#elif !defined(WND_USE_ANSI_API) && !defined(WND_USE_UNICODE_API)
#if defined(UNICODE) || defined(_UNICODE)
#define WND_USE_UNICODE_API
#else
#define WND_USE_ANSI_API
#endif
#endif

/**
 * @brief Plain-old-data bundle of the three Win32 message arguments.
 *
 * Passed by reference to user-defined `WndProc` overrides so that derived
 * classes can inspect (and, where the API calls for it, modify) any of the
 * fields without dealing with three separate parameters.
 */
struct Msg
{
    UINT   uMsg;   /**< Win32 message identifier (e.g. `WM_PAINT`). */
    WPARAM wParam; /**< Message-specific WPARAM payload. */
    LPARAM lParam; /**< Message-specific LPARAM payload. */
};

/**
 * @brief CRTP base class that wraps a Win32 `HWND` and dispatches messages to
 *        a `WndProc` method on the derived type.
 *
 * Two acquisition modes are supported:
 *  - **Create mode**: `CreateHandle()` registers a derived window class
 *    `<original-class>_Wnd_Class` (subclassing the user-supplied class) and
 *    creates a new window. The instance owns the resulting `HWND` and will
 *    `DestroyWindow` it on destruction.
 *  - **Attach mode**: `AttachHandle()` takes ownership of message dispatch on
 *    an externally-owned `HWND` by replacing its `GWLP_WNDPROC` with the
 *    static trampoline. The instance does **not** own the `HWND` and will
 *    only restore the original `WndProc` and release its prop on destruction.
 *
 * @tparam TDerived The derived class. Must implement
 *                  `bool WndProc(Msg& msg, LRESULT& result)` — return `true`
 *                  to indicate the message was fully handled and `result`
 *                  is the LRESULT, or `false` to defer to `DefWndProc`.
 *
 * @note Each instance binds itself to its `HWND` via a window property keyed
 *       by `_PROP_THIS()`. The static `StaticWndProc` looks the instance up
 *       on every message, which keeps the dispatch logic robust against
 *       reentrancy and outright instance destruction inside callbacks.
 *
 * @note Win32 windows are thread-affine: a `Wnd` must be destructed on the
 *       same thread that created (or attached to) the window. Cross-thread
 *       destruction is detected and warned about in debug builds; release
 *       builds silently no-op the cleanup, leaking the subclass and prop.
 */
template <typename TDerived>
class Wnd
{
    /**
     * @brief Grants `Dlg<*>` access to the private prop helpers and
     *        `_CreateParam`, since `Dlg` reuses the same prop key and
     *        marshalling structure for its own dialog dispatch.
     */
    template <typename>
    friend class Dlg;

    HWND    _hWnd;       /**< Wrapped window handle, or `NULL` when unbound. */
    WNDPROC _defWndProc; /**< Original WNDPROC to forward to in `DefWndProc()`. */
    bool    _destroyed;  /**< Set to `true` once `WM_NCDESTROY` has been observed. */
    bool    _owned;      /**< `true` if `CreateHandle` produced `_hWnd`; `false` for attach. */

private:
    /**
     * @brief Returns the property name used to store the `Wnd*` back-pointer
     *        on the underlying window.
     *
     * Implemented as a `constexpr` function rather than a `static constexpr`
     * data member so that taking its address (which `SetProp` / `RemoveProp`
     * effectively do) does not trigger an ODR-use that would require an
     * out-of-class definition under C++14.
     */
    static constexpr LPCTSTR _PROP_THIS() noexcept
    {
        return TEXT("__Wnd_This_Ptr");
    }

    /**
     * @brief Marshalling struct passed through `CREATESTRUCT::lpCreateParams`
     *        from `CreateHandle()` so that `StaticWndProc` can recover the
     *        owning `Wnd*` during `WM_NCCREATE` while still forwarding the
     *        user's original `lpParam`.
     */
    struct _CreateParam
    {
        Wnd* pThis;    /**< Pointer to the `Wnd` instance owning the soon-to-be window. */
        LPVOID lpParam;/**< User-supplied `lpParam` to restore into the create struct. */
    };

private:
    /**
     * @brief Looks up the `Wnd*` previously bound to @p hWnd by
     *        `BindThisToHandle`.
     *
     * Uses an internally-managed global atom keyed off `_PROP_THIS()` so that
     * subsequent `GetProp` calls perform an integer comparison instead of a
     * string hash on every dispatched message.
     *
     * @param hWnd The window handle to query.
     * @return The bound `Wnd*`, or `nullptr` if no binding exists.
     */
    static Wnd* GetThisFromHandle(HWND hWnd) noexcept
    {
        // Magic-static `_atom` is created on first call and torn down at
        // process exit. Each template instantiation has its own copy, but
        // the underlying atom string is shared across the global atom table.
        static struct _AtomRaiiHelper {
            ATOM value;
            _AtomRaiiHelper() noexcept : value(::GlobalAddAtom(_PROP_THIS())) {}
            ~_AtomRaiiHelper() noexcept { if (value != 0) ::GlobalDeleteAtom(value); }
        } _atom;
        return reinterpret_cast<Wnd*>(::GetProp(hWnd, MAKEINTATOM(_atom.value)));
    }

    /**
     * @brief Stores @p pThis on @p hWnd as a window property keyed by
     *        `_PROP_THIS()`.
     *
     * @param hWnd  The window to bind to.
     * @param pThis The `Wnd` instance to associate with @p hWnd.
     * @return `true` on success; `false` if `SetProp` failed (typically due
     *         to the per-process prop quota being exhausted).
     */
    static bool BindThisToHandle(HWND hWnd, Wnd* pThis) noexcept
    {
        return ::SetProp(hWnd, _PROP_THIS(), reinterpret_cast<HANDLE>(pThis)) != 0;
    }

    /**
     * @brief Static `WNDPROC` trampoline registered for every subclassed
     *        window. Recovers the bound `Wnd*` and forwards the message to
     *        its `WndProc`.
     *
     * Special handling:
     *  - On `WM_NCCREATE` for a freshly-created window, unwraps the
     *    `_CreateParam*` smuggled through `lpCreateParams`, binds the `Wnd*`
     *    to the HWND, and restores the user's original `lpParam` for the
     *    rest of the create-time messages.
     *  - On `WM_NCDESTROY`, marks the wrapper `_destroyed` and removes the
     *    prop so any later prop-lookup correctly returns `nullptr`.
     *  - After every callback into user code, re-reads the prop to detect
     *    self-deletion (`delete this` inside `WndProc`) and falls through
     *    to `DefWindowProc` instead of touching the freed instance.
     */
    static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        Wnd* pThis = GetThisFromHandle(hWnd);

        // First-time binding path: only at NCCREATE, only when we haven't
        // already been bound (e.g. via AttachHandle's SetWindowLongPtr).
        if (uMsg == WM_NCCREATE && pThis == nullptr && lParam != 0)
        {
            auto* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            auto* pParam = reinterpret_cast<_CreateParam*>(pCreate->lpCreateParams);

            if (pParam != nullptr)
            {
                pThis = pParam->pThis;
                pThis->_hWnd = hWnd;
                if (!BindThisToHandle(hWnd, pThis)) {
                    // SetProp failed — abort window creation by returning
                    // FALSE from NCCREATE, after restoring the original
                    // lpParam so any debugger inspection sees consistent state.
                    pThis->_hWnd = NULL;
                    pCreate->lpCreateParams = pParam->lpParam;
                    return FALSE;
                }
                pCreate->lpCreateParams = pParam->lpParam;
            }
        }

        if (pThis != nullptr)
        {
            Msg msg{ uMsg, wParam, lParam };
            LRESULT result = 0;

            bool handled = pThis->WndProc(msg, result);

            // Re-read the prop: if user code deleted the instance during
            // WndProc, the prop is gone (RemoveProp in NCDESTROY) and we
            // must not dereference pThis again.
            if (GetThisFromHandle(hWnd) != pThis) {
#if defined(WND_USE_ANSI_API)
                return ::DefWindowProcA(hWnd, uMsg, wParam, lParam);
#else
                return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
#endif
            }
            if (!handled) {
                result = pThis->DefWndProc(msg);
                // Same self-deletion guard for DefWndProc, which can also
                // re-enter user code via the original WNDPROC.
                if (GetThisFromHandle(hWnd) != pThis) {
                    return result;
                }
            }
            if (uMsg == WM_NCDESTROY) {
                pThis->_destroyed = true;
                ::RemoveProp(hWnd, _PROP_THIS());
            }
            return result;
        }

        // Unbound window (e.g. a stray message on a torn-down handle):
        // fall back to the system default.
#if defined(WND_USE_ANSI_API)
        return ::DefWindowProcA(hWnd, uMsg, wParam, lParam);
#else
        return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
#endif
    }

protected:
    /**
     * @brief Default-constructs an unbound `Wnd` with no associated `HWND`.
     *
     * Use `CreateHandle()` or `AttachHandle()` to bind to a window after
     * construction.
     */
    Wnd() noexcept
        : _hWnd(NULL), _defWndProc(NULL), _destroyed(false), _owned(false)
    {
    }

#if defined(WND_USE_ANSI_API)
    /**
     * @brief Creates a new window of class @p lpClassName, transparently
     *        subclassing it so that messages route through this instance.
     *
     * Internally registers a derived class named `<lpClassName>_Wnd_Class`
     * the first time the function runs for a given base class; subsequent
     * calls reuse the existing registration.
     *
     * @param dwExStyle    Extended window style passed to `CreateWindowExA`.
     * @param lpClassName  Name of the base class to subclass; must be non-null.
     * @param lpWindowName Initial window text.
     * @param dwStyle      Window style.
     * @param X, Y         Initial position.
     * @param nWidth, nHeight Initial size.
     * @param hWndParent   Parent / owner window.
     * @param hMenu        Menu handle, or child-window id when @p dwStyle has
     *                     `WS_CHILD`.
     * @param hInstance    Module instance owning the new class registration.
     * @param lpParam      Forwarded as `CREATESTRUCT::lpCreateParams` to the
     *                     user's `WM_NCCREATE` / `WM_CREATE` handlers.
     * @return `true` on success; `false` if the instance was already bound,
     *         `lpClassName` was null, the base class did not exist, class
     *         registration failed, or `CreateWindowExA` returned NULL.
     *         On failure, all internal state is rolled back.
     */
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

        // Look up the base class so we can preserve its WNDPROC for
        // forwarding through DefWndProc().
        if (!::GetClassInfoExA(hInstance, lpClassName, &wc)) {
            return false;
        }

        _defWndProc = wc.lpfnWndProc;

        // The address of StaticWndProc is unique per Wnd<T> instantiation, so
        // appending it makes the registered class name unique per T. Without
        // it, two Wnd<A>/Wnd<B> subclassing the same base would share one
        // registration and dispatch through the wrong StaticWndProc.
        std::string className = lpClassName;
        className += "_Wnd_Class_";
        className += std::to_string(reinterpret_cast<std::uintptr_t>(&StaticWndProc));

        // Register the derived class once per process. If it already exists,
        // GetClassInfoEx populates `wc` and we skip re-reg.
        if (!::GetClassInfoExA(hInstance, className.c_str(), &wc))
        {
            wc.lpfnWndProc = StaticWndProc;
            wc.lpszClassName = className.c_str();
            // GetClassInfoEx does NOT fill in hInstance; set it explicitly so
            // the new class is owned by the caller's module rather than NULL.
            wc.hInstance = hInstance;

            if (!::RegisterClassExA(&wc)) {
                _defWndProc = nullptr;
                return false;
            }
        }

        // _CreateParam is consumed by StaticWndProc on WM_NCCREATE.
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

        if (_hWnd != NULL) {
            _owned = true;
            return true;
        }
        _defWndProc = nullptr;
        return false;
    }

    /**
     * @brief Subclasses an externally-created window so that its messages
     *        route through this instance, without taking ownership.
     *
     * The original `GWLP_WNDPROC` is saved for forwarding via `DefWndProc()`.
     * On destruction the original procedure is restored and the prop is
     * cleared, but the window itself is left intact.
     *
     * @param hWnd The window to attach to.
     * @return `true` on success; `false` if this instance is already bound,
     *         @p hWnd is null, @p hWnd is already subclassed by some other
     *         `Wnd` instance, or the prop store failed.
     */
    bool AttachHandle(HWND hWnd) noexcept
    {
        if (_hWnd != NULL || hWnd == NULL) {
            return false;
        }

        WNDPROC proc = reinterpret_cast<WNDPROC>(
            ::GetWindowLongPtrA(hWnd, GWLP_WNDPROC));

        // Refuse to attach if the window is already routed through us
        // (would corrupt the existing chain) or already bound to another
        // Wnd instance via the prop (would silently steal ownership).
        if (proc == StaticWndProc || GetThisFromHandle(hWnd) != nullptr) {
            return false;
        }

        if (!BindThisToHandle(hWnd, this)) {
            return false;
        }

        _hWnd = hWnd;
        _defWndProc = proc;
        _owned = false;

        ::SetWindowLongPtrA(hWnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(StaticWndProc));
        return true;
    }

    /**
     * @brief Forwards @p msg to the original (pre-subclass) `WNDPROC`,
     *        falling back to `DefWindowProcA` when no original is recorded.
     *
     * Call from inside your `WndProc` override when you want default Win32
     * processing for messages you do not handle yourself.
     */
    LRESULT DefWndProc(const Msg& msg)
    {
        return _defWndProc != nullptr
            ? ::CallWindowProcA(_defWndProc, _hWnd, msg.uMsg, msg.wParam, msg.lParam)
            : ::DefWindowProcA(_hWnd, msg.uMsg, msg.wParam, msg.lParam);
    }
#else
    /**
     * @brief Unicode counterpart of `CreateHandle()`. See the ANSI overload
     *        for full documentation.
     */
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

        // Look up the base class so we can preserve its WNDPROC for
        // forwarding through DefWndProc().
        if (!::GetClassInfoExW(hInstance, lpClassName, &wc)) {
            return false;
        }

        _defWndProc = wc.lpfnWndProc;

        // The address of StaticWndProc is unique per Wnd<T> instantiation, so
        // appending it makes the registered class name unique per T. Without
        // it, two Wnd<A>/Wnd<B> subclassing the same base would share one
        // registration and dispatch through the wrong StaticWndProc.
        std::wstring className = lpClassName;
        className += L"_Wnd_Class_";
        className += std::to_wstring(reinterpret_cast<std::uintptr_t>(&StaticWndProc));

        // Register the derived class once per process. If it already exists,
        // GetClassInfoEx populates `wc` and we skip re-reg.
        if (!::GetClassInfoExW(hInstance, className.c_str(), &wc))
        {
            wc.lpfnWndProc = StaticWndProc;
            wc.lpszClassName = className.c_str();
            // GetClassInfoEx does NOT fill in hInstance; set it explicitly so
            // the new class is owned by the caller's module rather than NULL.
            wc.hInstance = hInstance;

            if (!::RegisterClassExW(&wc)) {
                _defWndProc = nullptr;
                return false;
            }
        }

        // _CreateParam is consumed by StaticWndProc on WM_NCCREATE.
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

        if (_hWnd != NULL) {
            _owned = true;
            return true;
        }
        _defWndProc = nullptr;
        return false;
    }

    /**
     * @brief Unicode counterpart of `AttachHandle()`. See the ANSI overload
     *        for full documentation.
     */
    bool AttachHandle(HWND hWnd) noexcept
    {
        if (_hWnd != NULL || hWnd == NULL) {
            return false;
        }

        WNDPROC proc = reinterpret_cast<WNDPROC>(
            ::GetWindowLongPtrW(hWnd, GWLP_WNDPROC));

        // Refuse to attach if the window is already routed through us
        // (would corrupt the existing chain) or already bound to another
        // Wnd instance via the prop (would silently steal ownership).
        if (proc == StaticWndProc || GetThisFromHandle(hWnd) != nullptr) {
            return false;
        }

        if (!BindThisToHandle(hWnd, this)) {
            return false;
        }

        _hWnd = hWnd;
        _defWndProc = proc;
        _owned = false;

        ::SetWindowLongPtrW(hWnd, GWLP_WNDPROC,
            reinterpret_cast<LONG_PTR>(StaticWndProc));
        return true;
    }

    /**
     * @brief Unicode counterpart of `DefWndProc()`. See the ANSI overload
     *        for full documentation.
     */
    LRESULT DefWndProc(const Msg& msg)
    {
        return _defWndProc != nullptr
            ? ::CallWindowProcW(_defWndProc, _hWnd, msg.uMsg, msg.wParam, msg.lParam)
            : ::DefWindowProcW(_hWnd, msg.uMsg, msg.wParam, msg.lParam);
    }
#endif

    /**
     * @brief Base-class trampoline that statically asserts the CRTP contract
     *        and dispatches to the derived `WndProc`.
     *
     * This is the function `StaticWndProc` calls; it is intentionally NOT a
     * `virtual` function so the dispatch is a single static down-cast with no
     * v-table cost. The `static_assert`s give a readable diagnostic when a
     * user forgets to implement `WndProc` or implements it with the wrong
     * signature.
     *
     * @param msg    Inbound message bundle.
     * @param result LRESULT the derived class wishes to return when handled.
     * @return `true` if the derived class handled the message, `false` to
     *         defer to `DefWndProc`.
     */
    bool WndProc(Msg& msg, LRESULT& result)
    {
        static_assert(
            std::is_base_of<Wnd<TDerived>, TDerived>::value,
            "TDerived must derive from Wnd<TDerived> (CRTP)");

        // Catch the "forgot to override" case: when TDerived inherits the
        // base WndProc unchanged, the type of `&TDerived::WndProc` reduces
        // to that of `&Wnd<TDerived>::WndProc`, and dispatching back into
        // it would infinite-recurse.
        static_assert(
            !std::is_same<
                decltype(&Wnd<TDerived>::WndProc),
                decltype(&TDerived::WndProc)>::value,
            "TDerived must override WndProc (otherwise dispatch would infinitely recurse)");

        // Catch wrong-signature overrides (e.g. `void WndProc(int)`):
        // `&TDerived::WndProc` resolves to whatever the user defined; if its
        // type is not exactly `bool (TDerived::*)(Msg&, LRESULT&)` the
        // comparison fails with a readable message before the call below
        // would error out with a cryptic overload-resolution diagnostic.
        static_assert(
            std::is_same<
                bool (TDerived::*)(Msg&, LRESULT&),
                decltype(&TDerived::WndProc)>::value,
            "TDerived must implement WndProc method with signature: bool WndProc(Msg& msg, LRESULT& result)");

        return static_cast<TDerived *>(this)->WndProc(msg, result);
    }

public:
    Wnd(const Wnd&) = delete;            /**< Non-copyable: copying a window handle is meaningless. */
    Wnd& operator=(Wnd&&) = delete;      /**< Move-assignment is disabled to avoid aliasing on the prop. */
    Wnd& operator=(const Wnd&) = delete; /**< Non-copyable. */

    /**
     * @brief Move-constructs from @p other, transferring `HWND` ownership and
     *        re-binding the window's prop to point at the new instance.
     *
     * The prop transfer is performed first; if it fails the new instance is
     * left empty (`_hWnd == NULL`, `_destroyed == true`) and @p other retains
     * its original state, so the window's lifetime is never orphaned.
     *
     * Callers that need to detect the rare `BindThisToHandle` failure can
     * check `Handle()` / `IsDestroyed()` on the new instance after the move.
     */
    Wnd(Wnd&& other) noexcept
        : _hWnd(NULL), _defWndProc(nullptr), _destroyed(true), _owned(false)
    {
        if (other._hWnd != NULL && !other._destroyed) {
            // Try to rebind the prop to the new instance first; only commit
            // the field transfer if that succeeds, so a Bind failure keeps
            // the source intact and its destructor still works.
            if (BindThisToHandle(other._hWnd, this)) {
                _hWnd       = other._hWnd;
                _defWndProc = other._defWndProc;
                _destroyed  = false;
                _owned      = other._owned;

                other._hWnd       = NULL;
                other._defWndProc = nullptr;
                other._destroyed  = true;
                other._owned      = false;
            }
        }
        else {
            // Source already empty / destroyed: just normalise its fields.
            other._hWnd       = NULL;
            other._defWndProc = nullptr;
            other._destroyed  = true;
            other._owned      = false;
        }
    }

    /**
     * @brief Tears down the binding and, when in create-mode, destroys the
     *        underlying window.
     *
     * Order of operations when the window is still alive:
     *  1. Restore the original `WNDPROC` so the system never invokes our
     *     `StaticWndProc` again on this `HWND`.
     *  2. Remove the `_PROP_THIS` prop so `GetThisFromHandle` cleanly
     *     returns `nullptr` for any in-flight messages.
     *  3. If `_owned` (created by `CreateHandle`), call `DestroyWindow`.
     *     Attached windows are left running.
     *
     * Must be invoked on the same thread that created or attached to the
     * window. Cross-thread destruction is detected and warned about in
     * debug builds; release builds silently leak the cleanup work.
     */
    ~Wnd() noexcept
    {
        if (_hWnd != NULL && !_destroyed)
        {
#if !defined(NDEBUG)
            // SetWindowLongPtr / DestroyWindow only succeed on the window's
            // owning thread. Surface the violation in the debugger output.
            DWORD wndTid = ::GetWindowThreadProcessId(_hWnd, nullptr);
            if (wndTid != 0 && wndTid != ::GetCurrentThreadId()) {
                ::OutputDebugString(TEXT("[Wnd] WARNING: destructor invoked on non-creator thread\n"));
            }
#endif
#if defined(WND_USE_ANSI_API)
            ::SetWindowLongPtrA(_hWnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(_defWndProc ? _defWndProc : ::DefWindowProcA));
#else
            ::SetWindowLongPtrW(_hWnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(_defWndProc ? _defWndProc : ::DefWindowProcW));
#endif
            ::RemoveProp(_hWnd, _PROP_THIS());
            if (_owned) {
                ::DestroyWindow(_hWnd);
            }
        }
        _destroyed = true;
    }

    /**
     * @brief Returns the wrapped window handle, or `NULL` when unbound.
     *
     * @note The handle may also be technically invalid (post-`WM_NCDESTROY`)
     *       even when non-null; pair with `IsDestroyed()` if precise
     *       lifetime detection matters.
     */
    HWND Handle() const noexcept
    {
        return _hWnd;
    }

    /**
     * @brief Reports whether the window has already received `WM_NCDESTROY`.
     *
     * Once this returns `true`, calling Win32 APIs against `Handle()` is
     * undefined behaviour.
     */
    bool IsDestroyed() const noexcept
    {
        return _destroyed;
    }

    /**
     * @brief Synchronously sends an ANSI message to this window.
     * @param uMsg   Message identifier.
     * @param wParam WPARAM payload.
     * @param lParam LPARAM payload.
     * @return The LRESULT returned by the receiving WNDPROC.
     */
    LRESULT SendMessageA(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0) const
    {
        return ::SendMessageA(_hWnd, uMsg, wParam, lParam);
    }

    /**
     * @brief Synchronously sends a Unicode message to this window.
     * @copydetails SendMessageA
     */
    LRESULT SendMessageW(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0) const
    {
        return ::SendMessageW(_hWnd, uMsg, wParam, lParam);
    }

    /**
     * @brief Asynchronously posts an ANSI message to this window's queue.
     * @param uMsg   Message identifier.
     * @param wParam WPARAM payload.
     * @param lParam LPARAM payload.
     * @return Non-zero on success; zero on failure (call `GetLastError()`).
     */
    BOOL PostMessageA(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0) const noexcept
    {
        return ::PostMessageA(_hWnd, uMsg, wParam, lParam);
    }

    /**
     * @brief Asynchronously posts a Unicode message to this window's queue.
     * @copydetails PostMessageA
     */
    BOOL PostMessageW(UINT uMsg, WPARAM wParam = 0, LPARAM lParam = 0) const noexcept
    {
        return ::PostMessageW(_hWnd, uMsg, wParam, lParam);
    }
};

/**
 * @brief CRTP wrapper around a Win32 dialog (`DialogBoxParam` /
 *        `CreateDialogParam`).
 *
 * Reuses the `bool WndProc(Msg&, LRESULT&)` callback signature inherited from
 * `Wnd`. The two channels are mapped to the DLGPROC convention as follows:
 *  - Returning `true` makes `StaticDlgProc` forward `result` via
 *    `DWLP_MSGRESULT` and return `TRUE` to the dialog manager.
 *  - Returning `false` makes `StaticDlgProc` return `FALSE`, telling the
 *    dialog manager to apply its own default processing.
 *
 * The `result` channel matters for messages that need to communicate an
 * `LRESULT` back to the system — `WM_CTLCOLOR*`, `WM_INITDIALOG` returning
 * `false` to keep custom focus, `WM_COMPAREITEM`, `WM_CHARTOITEM`,
 * `WM_VKEYTOITEM`, `WM_QUERYDRAGICON`, etc.
 *
 * @tparam TDerived The user's dialog class. Must satisfy the same CRTP
 *                  contract as `Wnd<TDerived>`.
 */
template <typename TDerived>
class Dlg : public Wnd<TDerived>
{
    /** Convenience alias for the `Wnd` base, used inside Dlg member bodies. */
    using TBase = Wnd<TDerived>;

    bool _isModal; /**< `true` when the dialog was created via `CreateModal()`. */

private:
    /**
     * @brief Static `DLGPROC` trampoline used for both modal and modeless
     *        dialogs.
     *
     * Behaves analogously to `Wnd::StaticWndProc`:
     *  - On the very first `WM_INITDIALOG`, unwraps the `_CreateParam*`
     *    smuggled through `lParam`, binds the `Dlg*`, and replaces `lParam`
     *    with the user's original `dwInitParam` for the user's WndProc.
     *  - On `WM_NCDESTROY`, marks the wrapper destroyed and removes the prop.
     *  - Maps the WndProc `bool`/`LRESULT` pair onto the DLGPROC
     *    `INT_PTR`/`DWLP_MSGRESULT` convention as described in the class
     *    header.
     */
    static INT_PTR CALLBACK StaticDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        TBase* pThis = TBase::GetThisFromHandle(hDlg);

        // First-time binding for dialogs happens at WM_INITDIALOG. Until
        // then the system has not given us a chance to plant the prop.
        if (pThis == nullptr &&
            uMsg == WM_INITDIALOG)
        {
            auto* pParam = reinterpret_cast<
                typename TBase::_CreateParam*>(lParam);

            if (pParam != nullptr) {
                pThis = pParam->pThis;
                pThis->_hWnd = hDlg;
                TBase::BindThisToHandle(hDlg, pThis);
                // Restore the user's original init param so the WndProc sees
                // the value they passed to CreateDlg / CreateModal.
                lParam = reinterpret_cast<LPARAM>(pParam->lpParam);
            }
        }

        if (pThis != nullptr)
        {
            Msg msg{ uMsg, wParam, lParam };
            LRESULT lResult = 0;

            bool handled =
                pThis->WndProc(msg, lResult);

            if (uMsg == WM_NCDESTROY) {
                pThis->_destroyed = true;
                ::RemoveProp(hDlg, TBase::_PROP_THIS());
            }
            if (handled) {
                // DLGPROC convention: return TRUE and stash the actual
                // LRESULT in DWLP_MSGRESULT for the dialog manager to read.
                ::SetWindowLongPtr(hDlg, DWLP_MSGRESULT, static_cast<LONG_PTR>(lResult));
                return TRUE;
            }
            return FALSE;
        }

        return FALSE;
    }

protected:
    /**
     * @brief Default-constructs an unbound `Dlg` with no associated dialog.
     *
     * Use `CreateDlg()` or `CreateModal()` to instantiate the underlying
     * dialog after construction.
     */
    Dlg() noexcept
        : TBase(), _isModal(false)
    {
    }

#if defined(WND_USE_ANSI_API)
    /**
     * @brief Creates a **modeless** dialog from the given template.
     *
     * @param hInstance      Module that owns the dialog template resource.
     * @param lpTemplateName Template identifier (use `MAKEINTRESOURCE`).
     * @param hWndParent     Owner window of the dialog.
     * @param dwInitParam    Forwarded as `lParam` of `WM_INITDIALOG`.
     * @return `true` if the dialog window was created successfully.
     */
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
        // DefDlgProc is the dialog-class default; preserved here so DefWndProc
        // calls coming from the user's WndProc forward sensibly even though
        // dialog dispatch normally goes through StaticDlgProc, not the
        // subclass chain.
        self._defWndProc = ::DefDlgProcA;

        typename TBase::_CreateParam initParam{
            this, reinterpret_cast<LPVOID>(dwInitParam) };

        // The `self._hWnd` field is populated by StaticDlgProc on the
        // synchronous WM_INITDIALOG dispatch inside CreateDialogParamA.
        ::CreateDialogParamA(
            hInstance,
            lpTemplateName,
            hWndParent,
            StaticDlgProc,
            reinterpret_cast<LPARAM>(&initParam));

        return self._hWnd != NULL;
    }

    /**
     * @brief Creates and runs a **modal** dialog. Blocks until `EndDialog`
     *        is called from inside the dialog procedure.
     *
     * @param hInstance      Module that owns the dialog template resource.
     * @param lpTemplateName Template identifier (use `MAKEINTRESOURCE`).
     * @param hWndParent     Owner window of the dialog.
     * @param dwInitParam    Forwarded as `lParam` of `WM_INITDIALOG`.
     * @return The value passed to `EndDialog`, or `-1` if the dialog could
     *         not be created (or this instance was already bound).
     */
    INT_PTR CreateModal(
        HINSTANCE hInstance,
        LPCSTR lpTemplateName,
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
    /**
     * @brief Unicode counterpart of `CreateDlg()`. See the ANSI overload for
     *        full documentation.
     */
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
        // DefDlgProc is the dialog-class default; preserved here so DefWndProc
        // calls coming from the user's WndProc forward sensibly.
        self._defWndProc = ::DefDlgProcW;

        typename TBase::_CreateParam initParam{
            this, reinterpret_cast<LPVOID>(dwInitParam) };

        // The `self._hWnd` field is populated by StaticDlgProc on the
        // synchronous WM_INITDIALOG dispatch inside CreateDialogParamW.
        ::CreateDialogParamW(
            hInstance,
            lpTemplateName,
            hWndParent,
            StaticDlgProc,
            reinterpret_cast<LPARAM>(&initParam));

        return self._hWnd != NULL;
    }

    /**
     * @brief Unicode counterpart of `CreateModal()`. See the ANSI overload
     *        for full documentation.
     */
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
    Dlg(const Dlg&) = delete;            /**< Non-copyable. */
    Dlg& operator=(Dlg&&) = delete;      /**< Move-assignment disabled (mirrors `Wnd`). */
    Dlg& operator=(const Dlg&) = delete; /**< Non-copyable. */

    /**
     * @brief Move-constructs from @p other, transferring both base-class
     *        state and the modal-flag.
     *
     * If `TBase`'s move fails (see `Wnd(Wnd&&)` — `BindThisToHandle` may
     * fail), @p other keeps its handle and the modal-flag is left untouched
     * on the source so its destructor still picks the right teardown path.
     */
    Dlg(Dlg&& other) noexcept
        : TBase(std::move(other)), _isModal(false)
    {
        // After a successful base move `other._hWnd == NULL`. On bind
        // failure `other` retains its HWND, and clearing its `_isModal`
        // would make ~Dlg call DestroyWindow on a modal window.
        if (other._hWnd == NULL) {
            _isModal = other._isModal;
            other._isModal = false;
        }
    }

    /**
     * @brief Tears down the dialog when the wrapper goes out of scope.
     *
     * Calls `EndDialog` for modal dialogs and `DestroyWindow` for modeless
     * ones. Both APIs synchronously deliver `WM_NCDESTROY`, so the user's
     * WndProc still gets one last chance to run cleanup code, and
     * `StaticDlgProc` removes the prop on the way out.
     *
     * @note The modal branch is essentially defensive — `DialogBoxParam`
     *       blocks the calling stack frame, so a `Dlg` holding `_isModal`
     *       cannot normally outlive its modal loop. The branch is preserved
     *       to cover pathological lifetimes (e.g. heap-allocated `Dlg` torn
     *       down on a different thread).
     */
    ~Dlg() noexcept
    {
        TBase& self = *static_cast<TBase*>(this);

        if (self._hWnd != NULL && !self._destroyed)
        {
            if (_isModal) {
                ::EndDialog(self._hWnd, 0);
            }
            else {
                ::DestroyWindow(self._hWnd);
            }
        }
        // Mark destroyed before ~Wnd runs so the base destructor short-circuits.
        self._destroyed = true;
    }

    /**
     * @brief Returns whether the dialog was created via `CreateModal()`.
     */
    bool IsModal() const noexcept
    {
        return _isModal;
    }

    /**
     * @brief Programmatically closes the dialog.
     *
     * Calls `EndDialog(hwnd, nResult)` for modal dialogs and
     * `DestroyWindow(hwnd)` for modeless ones.
     *
     * @param nResult Value to return from `CreateModal()`. Ignored for
     *                modeless dialogs.
     * @return Non-zero on success.
     */
    bool DestroyDlg(INT_PTR nResult = 0) noexcept
    {
        TBase& self = *static_cast<TBase*>(this);
        return _isModal ? ::EndDialog(self._hWnd, nResult) : ::DestroyWindow(self._hWnd);
    }
};

#endif // !WND_H_INCLUDED
