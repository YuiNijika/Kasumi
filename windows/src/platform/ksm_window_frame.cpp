#include "ksm_window_frame.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>

namespace {

constexpr UINT_PTR kFrameSubclassId = 0x4B46;
constexpr UINT_PTR kChildSubclassId = 0x4B47;
constexpr wchar_t kFramelessProperty[] = L"KasumiWindowFrameless";
constexpr int kMinWindowWidth = 320;
constexpr int kMinWindowHeight = 240;
constexpr int kResizeBorderDip = 8;

int ResizeBorder(HWND window) {
  const UINT dpi = GetDpiForWindow(window);
  return std::max(4, MulDiv(kResizeBorderDip, static_cast<int>(dpi), 96));
}

LRESULT HitTestFramelessWindow(HWND window, LPARAM lparam) {
  if (IsZoomed(window)) {
    return HTCLIENT;
  }

  RECT bounds{};
  if (!GetWindowRect(window, &bounds)) {
    return HTCLIENT;
  }

  const POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
  const int border = ResizeBorder(window);
  const bool is_left =
      point.x >= bounds.left && point.x < bounds.left + border;
  const bool is_right =
      point.x < bounds.right && point.x >= bounds.right - border;
  const bool is_top =
      point.y >= bounds.top && point.y < bounds.top + border;
  const bool is_bottom =
      point.y < bounds.bottom && point.y >= bounds.bottom - border;

  if (is_top && is_left) {
    return HTTOPLEFT;
  }
  if (is_top && is_right) {
    return HTTOPRIGHT;
  }
  if (is_bottom && is_left) {
    return HTBOTTOMLEFT;
  }
  if (is_bottom && is_right) {
    return HTBOTTOMRIGHT;
  }
  if (is_top) {
    return HTTOP;
  }
  if (is_bottom) {
    return HTBOTTOM;
  }
  if (is_left) {
    return HTLEFT;
  }
  if (is_right) {
    return HTRIGHT;
  }
  return HTCLIENT;
}

void ApplyFramelessStyle(HWND window) {
  LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
  style &= ~static_cast<LONG_PTR>(WS_CAPTION);
  style |= WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
  SetWindowLongPtrW(window, GWL_STYLE, style);
  SetWindowPos(
      window,
      nullptr,
      0,
      0,
      0,
      0,
      SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
          SWP_NOACTIVATE
  );
}

} // namespace

bool KSMWindowFrame::Configure(HWND window, bool decorations) {
  if (!window || decorations || IsFrameless(window)) {
    return window != nullptr;
  }

  if (
      !SetPropW(
          window,
          kFramelessProperty,
          reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1))
      )
  ) {
    return false;
  }
  if (
      !SetWindowSubclass(
          window,
          &KSMWindowFrame::SubclassProc,
          kFrameSubclassId,
          reinterpret_cast<DWORD_PTR>(window)
      )
  ) {
    RemovePropW(window, kFramelessProperty);
    return false;
  }

  ApplyFramelessStyle(window);
  return true;
}

bool KSMWindowFrame::IsFrameless(HWND window) {
  return window && GetPropW(window, kFramelessProperty) != nullptr;
}

void KSMWindowFrame::RefreshResizeHitTesting(HWND window) {
  if (!IsFrameless(window) || IsZoomed(window)) {
    return;
  }

  for (HWND child = GetWindow(window, GW_CHILD); child != nullptr;
       child = GetWindow(child, GW_HWNDNEXT)) {
    if (
        GetWindowSubclass(
            child,
            &KSMWindowFrame::ChildSubclassProc,
            kChildSubclassId,
            nullptr
        )
    ) {
      continue;
    }
    SetWindowSubclass(
        child,
        &KSMWindowFrame::ChildSubclassProc,
        kChildSubclassId,
        reinterpret_cast<DWORD_PTR>(window)
    );
  }
}

LRESULT CALLBACK KSMWindowFrame::ChildSubclassProc(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam,
    UINT_PTR,
    DWORD_PTR reference
) {
  const HWND parent = reinterpret_cast<HWND>(reference);
  if (message == WM_NCHITTEST && IsFrameless(parent)) {
    const LRESULT hit_test = HitTestFramelessWindow(parent, lparam);
    if (hit_test != HTCLIENT) {
      return hit_test;
    }
  }
  if (message == WM_NCDESTROY) {
    RemoveWindowSubclass(
        window,
        &KSMWindowFrame::ChildSubclassProc,
        kChildSubclassId
    );
  }
  return DefSubclassProc(window, message, wparam, lparam);
}

LRESULT CALLBACK KSMWindowFrame::SubclassProc(
    HWND window,
    UINT message,
    WPARAM wparam,
    LPARAM lparam,
    UINT_PTR,
    DWORD_PTR
) {
  if (!IsFrameless(window)) {
    return DefSubclassProc(window, message, wparam, lparam);
  }

  switch (message) {
  case WM_NCCALCSIZE:
    if (wparam == TRUE) {
      auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(lparam);
      if (IsZoomed(window)) {
        MONITORINFO monitor_info{};
        monitor_info.cbSize = sizeof(monitor_info);
        const HMONITOR monitor =
            MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
        if (GetMonitorInfoW(monitor, &monitor_info)) {
          params->rgrc[0] = monitor_info.rcWork;
        }
      }
      return 0;
    }
    break;
  case WM_GETMINMAXINFO: {
    auto *info = reinterpret_cast<MINMAXINFO *>(lparam);
    info->ptMinTrackSize.x = kMinWindowWidth;
    info->ptMinTrackSize.y = kMinWindowHeight;
    return 0;
  }
  case WM_NCHITTEST:
    return HitTestFramelessWindow(window, lparam);
  case WM_NCDESTROY:
    RemoveWindowSubclass(window, &KSMWindowFrame::SubclassProc, kFrameSubclassId);
    RemovePropW(window, kFramelessProperty);
    break;
  default:
    break;
  }

  return DefSubclassProc(window, message, wparam, lparam);
}
