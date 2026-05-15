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

```
your_project/
└── include/
    └── wnd.h   ← drop it here
```

## Character-set selection

| Scenario | Result |
|---|---|
| `WND_USE_ANSI_API` defined | ANSI (`*A`) Win32 variants |
| `WND_USE_UNICODE_API` defined | Unicode (`*W`) Win32 variants |
| Both defined | Compile error |
| Neither defined, `UNICODE` or `_UNICODE` set | Unicode (`*W`) |
| Neither defined, no ambient macro | ANSI (`*A`) |

Define the macro before the `#include` or via a compiler flag:

```cpp
#define WND_USE_UNICODE_API
#include "wnd.h"
```

## Quick start

### Wrapping a new window

```cpp
#include "wnd.h"

class MyEdit : public Wnd<MyEdit>
{
public:
    bool WndProc(Msg& msg, LRESULT& result)
    {
        if (msg.uMsg == WM_CHAR)
        {
            // handle keystroke …
            result = 0;
            return true;   // message handled
        }
        return false;      // defer to original WNDPROC
    }
};

// Somewhere in your initialisation code:
MyEdit edit;
edit.CreateHandle(
    0, TEXT("EDIT"), TEXT(""),
    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
    10, 10, 300, 24,
    hParentWnd, reinterpret_cast<HMENU>(IDC_EDIT1),
    hInstance, nullptr);
```

### Attaching to an existing HWND

```cpp
MyEdit edit;
edit.AttachHandle(hSomeExternalEdit);
// edit.~MyEdit() restores the original WndProc automatically.
```

### Modal dialog

```cpp
class MyDlg : public Dlg<MyDlg>
{
public:
    bool WndProc(Msg& msg, LRESULT& result)
    {
        if (msg.uMsg == WM_COMMAND && LOWORD(msg.wParam) == IDOK)
        {
            DestroyDlg(IDOK);
            return true;
        }
        return false;
    }
};

MyDlg dlg;
INT_PTR ret = dlg.CreateModal(hInstance, MAKEINTRESOURCE(IDD_MYDIALOG), hOwnerWnd);
// CreateModal blocks until DestroyDlg / EndDialog is called.
```

### Modeless dialog

```cpp
MyDlg dlg;
dlg.CreateDlg(hInstance, MAKEINTRESOURCE(IDD_MYDIALOG), hOwnerWnd);
ShowWindow(dlg.Handle(), SW_SHOW);
// dlg.~Dlg() calls DestroyWindow automatically.
```

## API reference

### `struct Msg`

Plain aggregate passed by reference to every `WndProc` call.

| Field | Type | Description |
|---|---|---|
| `uMsg` | `UINT` | Win32 message identifier |
| `wParam` | `WPARAM` | Message-specific WPARAM |
| `lParam` | `LPARAM` | Message-specific LPARAM |

### `Wnd<TDerived>`

| Member | Description |
|---|---|
| `CreateHandle(…)` | Creates a new `HWND` and subclasses it. Returns `bool`. |
| `AttachHandle(HWND)` | Subclasses an externally-created window. Returns `bool`. |
| `DefWndProc(Msg&)` | Forwards to the original (pre-subclass) `WNDPROC`. |
| `Handle()` | Returns the wrapped `HWND`, or `NULL` when unbound. |
| `IsDestroyed()` | Returns `true` after `WM_NCDESTROY` has been observed. |
| `SendMessageA/W(…)` | Synchronous send. |
| `PostMessageA/W(…)` | Asynchronous post. |
| destructor | Restores original `WNDPROC`; calls `DestroyWindow` if owned. |

`Wnd` is non-copyable. Move construction transfers the `HWND` binding.

### `Dlg<TDerived>` — extends `Wnd<TDerived>`

| Member | Description |
|---|---|
| `CreateDlg(…)` | Creates a modeless dialog. Returns `bool`. |
| `CreateModal(…)` | Creates and runs a modal dialog. Blocks; returns `INT_PTR`. |
| `DestroyDlg(INT_PTR)` | Calls `EndDialog` (modal) or `DestroyWindow` (modeless). |
| `IsModal()` | Returns `true` when created via `CreateModal`. |
| destructor | Calls `EndDialog(0)` or `DestroyWindow` as appropriate. |

### Implementing `WndProc`

```cpp
bool WndProc(Msg& msg, LRESULT& result)
{
    // Return true  → message is fully handled; result is the LRESULT.
    // Return false → defer to DefWndProc (original procedure or DefWindowProc).
}
```

For dialogs the mapping follows DLGPROC convention: returning `true` sets `DWLP_MSGRESULT` to `result` and returns `TRUE` to the dialog manager; returning `false` returns `FALSE`.

## Design notes

### CBT hook — binding before the first message

`Wnd::CreateHandle` installs a thread-local `WH_CBT` hook that fires on `HCBT_CREATEWND`, which precedes `WM_NCCREATE`. This lets the binding be in place before the very first message the window procedure receives, without registering a custom window class. The hook self-unregisters immediately after the first `HCBT_CREATEWND` so nested `CreateWindow` calls inside `WM_NCCREATE` or `WM_CREATE` are not affected.

### Window property table — per-HWND instance storage

The `Wnd*` back-pointer is stored as a window property (`SetProp`/`GetProp`) keyed by a global atom. This avoids a global `map<HWND, Wnd*>` and the locking it would require; atom-based lookup is a single integer comparison.

### Dialog binding — `WM_INITDIALOG` smuggling

The dialog manager controls `GWLP_WNDPROC`, so the CBT strategy is unavailable. Instead, `CreateDlg` / `CreateModal` wrap the caller-supplied `dwInitParam` in a small `_CreateParam` struct and pass a pointer to it as the actual `dwInitParam`. `StaticDlgProc` unwraps the struct on the first `WM_INITDIALOG`, plants the property, and restores the original value of `lParam` before dispatching to user code — so `WndProc` sees exactly what the caller passed.

## Thread safety

Win32 windows are thread-affine. A `Wnd` or `Dlg` instance must be constructed, used, and destructed on the thread that owns its `HWND`. Cross-thread destruction is flagged with `OutputDebugString` in debug builds; release builds skip the cleanup to avoid undefined behaviour from `SetWindowLongPtr` on the wrong thread.

## License

See `LICENSE` for terms.