# wnd.h

A single-header, header-only C++ library that wraps Win32 windows and dialog boxes in CRTP base classes, routing messages to a simple virtual-like callback without virtual functions or heap allocation.

## Features

- **Zero overhead dispatch** — CRTP static down-cast, no vtable, no `std::function`.
- **Two acquisition modes** for windows — create a new `HWND` or attach to an existing one.
- **Two dialog modes** — modal (`DialogBoxParam`) and modeless (`CreateDialogParam`).
- **Visual-style preservation** — the original class name is passed untouched to `CreateWindowEx`, so `BUTTON`, `EDIT`, `SysListView32`, etc. still receive UxTheme styling.
- **Safe self-deletion** — the static trampoline re-reads the window property after every user callback, so `delete this` inside `WndProc` is safe.
- **Move semantics** — handles can be transferred between `Wnd` / `Dlg` instances; the window property is atomically re-bound on move.
- **Character-set agnostic** — a single compile-time switch selects the ANSI (`*A`) or Unicode (`*W`) Win32 variants; the default follows the ambient `UNICODE` / `_UNICODE` macros.
- **Compile-time contract checking** — `static_assert`s catch missing or wrong-signature `WndProc` overrides with readable diagnostics.

## Requirements

- C++14 or later
- Windows SDK (any reasonably modern version)
- Any conforming C++ compiler targeting Windows (MSVC, Clang-cl, MinGW)

## Installation

`wnd.h` is a single header with no external dependencies beyond `<windows.h>` and `<type_traits>` / `<utility>` from the standard library. Copy it anywhere on your include path.

```text
your_project/
└── include/
    └── wnd.h
```

## Character-set selection

| Scenario                                     | Result                        |
| -------------------------------------------- | ----------------------------- |
| `WND_USE_ANSI_API` defined                   | ANSI (`*A`) Win32 variants    |
| `WND_USE_UNICODE_API` defined                | Unicode (`*W`) Win32 variants |
| Both defined                                 | Compile error                 |
| Neither defined, `UNICODE` or `_UNICODE` set | Unicode (`*W`)                |
| Neither defined, no ambient macro            | ANSI (`*A`)                   |

Define the macro before the `#include` or via a compiler flag:

```cpp
#define WND_USE_UNICODE_API
#include "wnd.h"
```

## Quick start

### Wrapping a new window

`CreateHandle` and `AttachHandle` are intentionally `protected`.
Derived classes are expected to expose higher-level wrappers that reflect the semantics of the wrapped control or window type.

```cpp
#include "wnd.h"

class MyEdit : public Wnd<MyEdit>
{
public:
    bool Create(HWND hParentWnd, HINSTANCE hInstance, int id)
    {
        return CreateHandle(
            0,
            TEXT("EDIT"),
            TEXT(""),
            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
            10, 10, 300, 24,
            hParentWnd,
            reinterpret_cast<HMENU>(id),
            hInstance,
            nullptr);
    }

    bool WndProc(Msg& msg, LRESULT& result)
    {
        if (msg.uMsg == WM_CHAR)
        {
            // Handle keystroke…
            result = 0;
            return true;
        }

        return false; // Defer to original WNDPROC
    }
};

// Somewhere in your initialisation code:
MyEdit edit;

edit.Create(
    hParentWnd,
    hInstance,
    IDC_EDIT1);
```

### Attaching to an existing HWND

Likewise, attaching is normally exposed through a derived helper:

```cpp
class MyEdit : public Wnd<MyEdit>
{
public:
    bool Attach(HWND hWnd)
    {
        return AttachHandle(hWnd);
    }

    bool WndProc(Msg& msg, LRESULT& result)
    {
        return false;
    }
};

MyEdit edit;

edit.Attach(hSomeExternalEdit);

// edit.~MyEdit() restores the original WndProc automatically.
```

### Modal dialog

```cpp
class MyDlg : public Dlg<MyDlg>
{
public:
    bool ShowModal(
        HINSTANCE hInstance,
        HWND hOwnerWnd)
    {
        return CreateModal(
            hInstance,
            MAKEINTRESOURCE(IDD_MYDIALOG),
            hOwnerWnd) >= 0;
    }

    bool WndProc(Msg& msg, LRESULT& result)
    {
        if (msg.uMsg == WM_COMMAND &&
            LOWORD(msg.wParam) == IDOK)
        {
            DestroyDlg(IDOK);
            result = IDOK;
            return true;
        }

        return false;
    }
};

MyDlg dlg;

dlg.ShowModal(
    hInstance,
    hOwnerWnd);
```

### Modeless dialog

```cpp
class MyDlg : public Dlg<MyDlg>
{
public:
    bool Create(
        HINSTANCE hInstance,
        HWND hOwnerWnd)
    {
        return CreateDlg(
            hInstance,
            MAKEINTRESOURCE(IDD_MYDIALOG),
            hOwnerWnd);
    }

    bool WndProc(Msg& msg, LRESULT& result)
    {
        return false;
    }
};

MyDlg dlg;

dlg.Create(
    hInstance,
    hOwnerWnd);

ShowWindow(dlg.Handle(), SW_SHOW);

// dlg.~Dlg() calls DestroyWindow automatically.
```

## API reference

### `struct Msg`

Plain aggregate passed by reference to every `WndProc` call.

| Field    | Type     | Description              |
| -------- | -------- | ------------------------ |
| `uMsg`   | `UINT`   | Win32 message identifier |
| `wParam` | `WPARAM` | Message-specific WPARAM  |
| `lParam` | `LPARAM` | Message-specific LPARAM  |

---

## `Wnd<TDerived>`

The low-level window-management APIs are intentionally `protected`.
Derived classes are expected to expose domain-specific wrappers around them.

### Protected members

| Member               | Description                                              |
| -------------------- | -------------------------------------------------------- |
| `CreateHandle(…)`    | Creates a new `HWND` and subclasses it. Returns `bool`.  |
| `AttachHandle(HWND)` | Subclasses an externally-created window. Returns `bool`. |
| `DefWndProc(Msg&)`   | Forwards to the original (pre-subclass) `WNDPROC`.       |

### Public members

| Member              | Description                                                  |
| ------------------- | ------------------------------------------------------------ |
| `Handle()`          | Returns the wrapped `HWND`, or `NULL` when unbound.          |
| `IsDestroyed()`     | Returns `true` after `WM_NCDESTROY` has been observed.       |
| `SendMessageA/W(…)` | Synchronous send.                                            |
| `PostMessageA/W(…)` | Asynchronous post.                                           |
| destructor          | Restores original `WNDPROC`; calls `DestroyWindow` if owned. |

`Wnd` is non-copyable. Move construction transfers the `HWND` binding.

---

## `Dlg<TDerived>` — extends `Wnd<TDerived>`

Dialog creation APIs are also intentionally `protected`.

### Protected members

| Member           | Description                                                 |
| ---------------- | ----------------------------------------------------------- |
| `CreateDlg(…)`   | Creates a modeless dialog. Returns `bool`.                  |
| `CreateModal(…)` | Creates and runs a modal dialog. Blocks; returns `INT_PTR`. |

### Public members

| Member                | Description                                              |
| --------------------- | -------------------------------------------------------- |
| `IsModal()`           | Returns `true` when created via `CreateModal`.           |
| `DestroyDlg(INT_PTR)` | Calls `EndDialog` (modal) or `DestroyWindow` (modeless). |
| destructor            | Calls `EndDialog(0)` or `DestroyWindow` as appropriate.  |

---

## Implementing `WndProc`

```cpp
bool WndProc(Msg& msg, LRESULT& result)
{
    // Return true:
    //   The message is fully handled.
    //   'result' becomes the returned LRESULT.

    // Return false:
    //   Dispatch falls through to DefWndProc().
}
```

For dialogs the mapping follows DLGPROC convention:

- Returning `true` sets `DWLP_MSGRESULT` to `result` and returns `TRUE`
- Returning `false` returns `FALSE` so the dialog manager performs default processing

## Design notes

### CBT hook — binding before the first message

`Wnd::CreateHandle` installs a thread-local `WH_CBT` hook that fires on `HCBT_CREATEWND`, which precedes `WM_NCCREATE`.

This lets the binding be established before the first window-procedure message is dispatched, without registering a custom window class.

The hook self-unregisters immediately after the first `HCBT_CREATEWND`, so nested `CreateWindow` calls inside `WM_NCCREATE` or `WM_CREATE` are unaffected.

### Window property table — per-HWND instance storage

The `Wnd*` back-pointer is stored as a window property (`SetProp` / `GetProp`) keyed by a global atom.

This avoids:

- a global `map<HWND, Wnd*>`
- explicit locking
- global lifetime coordination

Atom-based lookup is effectively a single integer comparison.

### Dialog binding — `WM_INITDIALOG` smuggling

The dialog manager owns `GWLP_WNDPROC`, so the CBT strategy cannot be used for dialogs.

Instead, `CreateDlg` / `CreateModal` wrap the caller-supplied `dwInitParam` in a small `_CreateParam` structure and pass a pointer to it as the actual dialog init parameter.

`StaticDlgProc` unwraps the structure during the first `WM_INITDIALOG`, binds the `Dlg*`, then restores the caller's original `lParam` before dispatching to user code.

As a result, the user's `WndProc` sees exactly the same `lParam` value they originally supplied.

## Thread safety

Win32 windows are thread-affine.

A `Wnd` or `Dlg` instance must be:

- created
- used
- destroyed

on the thread that owns the underlying `HWND`.

Cross-thread destruction is flagged with `OutputDebugString` in debug builds.

Release builds intentionally skip teardown work rather than invoking undefined behaviour through `SetWindowLongPtr` or `DestroyWindow` on the wrong thread.

## License

See `LICENSE` for terms.
