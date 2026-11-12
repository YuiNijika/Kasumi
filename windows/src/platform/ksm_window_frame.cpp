#include "ksm_window_frame.h"

#include <commctrl.h>
#include <windowsx.h>

#include <algorithm>

namespace {

constexpr UINT_PTR kFrameSubclassId = 0x4B46;
constexpr UINT_PTR kChildSubclassId = 0x4B47;
constexpr wchar_t kFramelessProperty[] = L"KasumiWindowFrameless";
constexpr wchar_t kMinSizeProperty[] = L"KasumiWindowMinSize";
constexpr wchar_t kMaxSizeProperty[] = L"KasumiWindowMaxSize";
constexpr wchar_t kLeftButtonProperty[] = L"KasumiWindowLeftButton";
constexpr wchar_t kResizePollIntervalProperty[] =
    L"KasumiWindowResizePollInterval";
constexpr UINT kStartDragMessage = WM_APP + 0x4B46;
constexpr UINT kStartResizeMessage = WM_APP + 0x4B48;
constexpr UINT_PTR kResizePollTimerId = 0x4B49;
constexpr int kMinWindowWidth = 320;
constexpr int kMinWindowHeight = 240;
constexpr int kResizeBorderDip = 8;
constexpr int kCornerRadiusDip = 12;
constexpr UINT kResizePollFastMs = 16;
constexpr UINT kResizePollIdleMs = 120;
constexpr UINT kResizePollSlowMs = 250;
constexpr USHORT kRawMouseUsagePage = 0x01;
constexpr USHORT kRawMouseUsage = 0x02;
constexpr DWORD kDwmWindowCornerPreference = 33;
constexpr int kDwmWindowCornerDoNotRound = 1;
constexpr int kDwmWindowCornerRound = 2;
constexpr wchar_t kRoundedRegionProperty[] = L"KasumiRoundedRegion";

using DwmSetWindowAttributeFn = HRESULT(WINAPI *)(
    HWND,
    DWORD,
    LPCVOID,
    DWORD
);

int ResizeBorder(HWND window) {
  const UINT dpi = GetDpiForWindow(window);
  return (std::max)(4, MulDiv(kResizeBorderDip, static_cast<int>(dpi), 96));
}

bool IsWindowMaximized(HWND window) {
  return IsZoomed(window) ||
      (GetWindowLongPtrW(window, GWL_STYLE) & WS_MAXIMIZE) != 0;
}

DwmSetWindowAttributeFn DwmSetWindowAttributeProc() {
  static const auto proc = []() -> DwmSetWindowAttributeFn {
    HMODULE module = GetModuleHandleW(L"dwmapi.dll");
    if (!module) {
      module = LoadLibraryW(L"dwmapi.dll");
    }
    if (!module) {
      return nullptr;
    }
    return reinterpret_cast<DwmSetWindowAttributeFn>(
        GetProcAddress(module, "DwmSetWindowAttribute")
    );
  }();
  return proc;
}

void ClearRoundedRegion(HWND window) {
  if (GetPropW(window, kRoundedRegionProperty) == nullptr) {
    return;
  }
  SetWindowRgn(window, nullptr, TRUE);
  RemovePropW(window, kRoundedRegionProperty);
}

bool ApplyDwmCorners(HWND window, bool square) {
  const auto set_attribute = DwmSetWindowAttributeProc();
  if (!set_attribute) {
    return false;
  }
  const int preference = square
      ? kDwmWindowCornerDoNotRound
      : kDwmWindowCornerRound;
  return SUCCEEDED(
      set_attribute(
          window,
          kDwmWindowCornerPreference,
          &preference,
          sizeof(preference)
      )
  );
}

void ApplyRoundedCorners(HWND window) {
  if (!KSMWindowFrame::IsFrameless(window)) {
    return;
  }
  const bool square = IsWindowMaximized(window) || IsIconic(window);
  if (ApplyDwmCorners(window, square)) {
    ClearRoundedRegion(window);
    return;
  }
  if (square) {
    ClearRoundedRegion(window);
    return;
  }

  RECT bounds{};
  if (!GetWindowRect(window, &bounds)) {
    return;
  }
  const int width = bounds.right - bounds.left;
  const int height = bounds.bottom - bounds.top;
  if (width <= 0 || height <= 0) {
    return;
  }
  const int radius = MulDiv(
      kCornerRadiusDip,
      static_cast<int>(GetDpiForWindow(window)),
      96
  );
  HRGN region = CreateRoundRectRgn(
      0,
      0,
      width + 1,
      height + 1,
      radius * 2,
      radius * 2
  );
  if (!region) {
    return;
  }
  if (SetWindowRgn(window, region, TRUE) == 0) {
    DeleteObject(region);
    return;
  }
  SetPropW(
      window,
      kRoundedRegionProperty,
      reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1))
  );
}

LPCWSTR ResizeCursor(int hit_test) {
  switch (hit_test) {
  case HTTOPLEFT:
  case HTBOTTOMRIGHT:
    return IDC_SIZENWSE;
  case HTTOPRIGHT:
  case HTBOTTOMLEFT:
    return IDC_SIZENESW;
  case HTLEFT:
  case HTRIGHT:
    return IDC_SIZEWE;
  case HTTOP:
  case HTBOTTOM:
    return IDC_SIZENS;
  default:
    return IDC_ARROW;
  }
}

LRESULT HitTestFramelessWindow(HWND window, LPARAM lparam) {
  if (IsWindowMaximized(window)) {
    return HTCLIENT;
  }
  if ((GetWindowLongPtrW(window, GWL_STYLE) & WS_THICKFRAME) == 0) {
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

bool IsResizeAvailable(HWND window) {
  if (!window || !IsWindow(window) || !KSMWindowFrame::IsFrameless(window)) {
    return false;
  }
  if ((GetWindowLongPtrW(window, GWL_STYLE) & WS_THICKFRAME) == 0) {
    return false;
  }
  return !IsWindowMaximized(window) && !IsIconic(window);
}

int HitTestForDirection(KSMWindowResizeDirection direction) {
  switch (direction) {
  case KSMWindowResizeDirection::Top:
    return HTTOP;
  case KSMWindowResizeDirection::Bottom:
    return HTBOTTOM;
  case KSMWindowResizeDirection::Left:
    return HTLEFT;
  case KSMWindowResizeDirection::Right:
    return HTRIGHT;
  case KSMWindowResizeDirection::TopLeft:
    return HTTOPLEFT;
  case KSMWindowResizeDirection::TopRight:
    return HTTOPRIGHT;
  case KSMWindowResizeDirection::BottomLeft:
    return HTBOTTOMLEFT;
  case KSMWindowResizeDirection::BottomRight:
    return HTBOTTOMRIGHT;
  }
  return HTCLIENT;
}

void StartResizeWithHitTest(HWND window, int hit_test) {
  if (!IsResizeAvailable(window) || hit_test == HTCLIENT) {
    return;
  }
  if (GetCapture() == window) {
    return;
  }
  POINT cursor{};
  if (!GetCursorPos(&cursor)) {
    RECT bounds{};
    if (!GetWindowRect(window, &bounds)) {
      return;
    }
    cursor = POINT{
        bounds.left + (bounds.right - bounds.left) / 2,
        bounds.top + (bounds.bottom - bounds.top) / 2
    };
  }
  ReleaseCapture();
  SendMessageW(
      window,
      WM_NCLBUTTONDOWN,
      static_cast<WPARAM>(hit_test),
      MAKELPARAM(cursor.x, cursor.y)
  );
}

void StartResizeOnWindowThread(
    HWND window,
    KSMWindowResizeDirection direction
) {
  StartResizeWithHitTest(window, HitTestForDirection(direction));
}

void UpdateResizePollTimer(HWND window, UINT interval) {
  if (!window || !IsWindow(window)) {
    return;
  }
  const INT_PTR current = reinterpret_cast<INT_PTR>(
      GetPropW(window, kResizePollIntervalProperty)
  );
  if (current == static_cast<INT_PTR>(interval)) {
    return;
  }
  if (
      SetTimer(window, kResizePollTimerId, interval, nullptr) != 0
  ) {
    SetPropW(
        window,
        kResizePollIntervalProperty,
        reinterpret_cast<HANDLE>(static_cast<INT_PTR>(interval))
    );
  }
}

void PollResizeEdge(HWND window) {
  if (!IsResizeAvailable(window) || !IsWindowVisible(window)) {
    return;
  }
  if (GetForegroundWindow() != window) {
    return;
  }

  RECT bounds{};
  if (!GetWindowRect(window, &bounds)) {
    return;
  }
  POINT cursor{};
  if (!GetCursorPos(&cursor)) {
    return;
  }
  const bool inside =
      cursor.x >= bounds.left && cursor.x < bounds.right &&
      cursor.y >= bounds.top && cursor.y < bounds.bottom;
  const bool left_down =
      (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  if (!inside) {
    if (left_down) {
      SetPropW(
          window,
          kLeftButtonProperty,
          reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1))
      );
    } else {
      RemovePropW(window, kLeftButtonProperty);
    }
    UpdateResizePollTimer(window, kResizePollIdleMs);
    return;
  }

  UpdateResizePollTimer(window, kResizePollFastMs);
  const LRESULT hit_test = HitTestFramelessWindow(
      window,
      MAKELPARAM(cursor.x, cursor.y)
  );
  if (hit_test != HTCLIENT) {
    SetCursor(LoadCursorW(nullptr, ResizeCursor(static_cast<int>(hit_test))));
  }

  const bool was_down =
      GetPropW(window, kLeftButtonProperty) != nullptr;
  if (
      left_down && !was_down && hit_test != HTCLIENT &&
      GetCapture() != window
  ) {
    StartResizeWithHitTest(window, static_cast<int>(hit_test));
  }
  if (left_down) {
    SetPropW(
        window,
        kLeftButtonProperty,
        reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1))
    );
  } else {
    RemovePropW(window, kLeftButtonProperty);
  }
}

bool RegisterResizeRawInput(HWND window) {
  RAWINPUTDEVICE device{};
  device.usUsagePage = kRawMouseUsagePage;
  device.usUsage = kRawMouseUsage;
  device.dwFlags = 0;
  device.hwndTarget = window;
  return RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
}

void UpdateResizeCursor(HWND window) {
  POINT cursor{};
  if (!GetCursorPos(&cursor)) {
    return;
  }
  const LRESULT hit_test = HitTestFramelessWindow(
      window,
      MAKELPARAM(cursor.x, cursor.y)
  );
  if (hit_test != HTCLIENT) {
    SetCursor(LoadCursorW(nullptr, ResizeCursor(static_cast<int>(hit_test))));
  }
}

bool StartResizeFromRawInput(HWND window) {
  if (GetForegroundWindow() != window) {
    return false;
  }
  POINT cursor{};
  if (!GetCursorPos(&cursor)) {
    return false;
  }
  const LRESULT hit_test = HitTestFramelessWindow(
      window,
      MAKELPARAM(cursor.x, cursor.y)
  );
  if (hit_test == HTCLIENT) {
    return false;
  }
  ReleaseCapture();
  SendMessageW(
      window,
      WM_NCLBUTTONDOWN,
      static_cast<WPARAM>(hit_test),
      MAKELPARAM(cursor.x, cursor.y)
  );
  return true;
}

void ApplyFramelessStyle(HWND window, bool resizable) {
  LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
  style &= ~static_cast<LONG_PTR>(WS_CAPTION);
  style |= WS_SYSMENU | WS_MINIMIZEBOX;
  if (resizable) {
    style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
  } else {
    style &= ~static_cast<LONG_PTR>(WS_THICKFRAME | WS_MAXIMIZEBOX);
  }
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

bool StartDragOnWindowThread(HWND window) {
  if (!window || !IsWindow(window)) {
    return false;
  }
  POINT cursor{};
  GetCursorPos(&cursor);
  ReleaseCapture();
  SendMessageW(
      window,
      WM_NCLBUTTONDOWN,
      HTCAPTION,
      MAKELPARAM(cursor.x, cursor.y)
  );
  return true;
}

void ApplyConfiguredTrackLimits(HWND window, MINMAXINFO *info) {
  const auto min_size = reinterpret_cast<INT_PTR>(
      GetPropW(window, kMinSizeProperty)
  );
  info->ptMinTrackSize.x = min_size == 0
      ? kMinWindowWidth
      : LOWORD(static_cast<DWORD>(min_size));
  info->ptMinTrackSize.y = min_size == 0
      ? kMinWindowHeight
      : HIWORD(static_cast<DWORD>(min_size));

  const auto max_size = reinterpret_cast<INT_PTR>(
      GetPropW(window, kMaxSizeProperty)
  );
  const int max_width = max_size == 0
      ? 0
      : LOWORD(static_cast<DWORD>(max_size));
  const int max_height = max_size == 0
      ? 0
      : HIWORD(static_cast<DWORD>(max_size));
  if (max_width > 0) {
    info->ptMaxTrackSize.x = max_width;
    if (info->ptMaxSize.x > max_width) {
      info->ptMaxSize.x = max_width;
    }
  }
  if (max_height > 0) {
    info->ptMaxTrackSize.y = max_height;
    if (info->ptMaxSize.y > max_height) {
      info->ptMaxSize.y = max_height;
    }
  }
}

} // namespace

bool KSMWindowFrame::Configure(
    HWND window,
    bool decorations,
    bool resizable,
    int min_width,
    int min_height,
    int max_width,
    int max_height
) {
  if (!window) {
    return false;
  }
  const bool frameless = !decorations;
  if (frameless && IsFrameless(window)) {
    return true;
  }

  const int clamped_min_width = (std::max)(
      1,
      (std::min)(min_width > 0 ? min_width : kMinWindowWidth, 0xFFFF)
  );
  const int clamped_min_height = (std::max)(
      1,
      (std::min)(min_height > 0 ? min_height : kMinWindowHeight, 0xFFFF)
  );
  const int clamped_max_width = max_width > 0
      ? (std::max)(clamped_min_width, (std::min)(max_width, 0xFFFF))
      : 0;
  const int clamped_max_height = max_height > 0
      ? (std::max)(clamped_min_height, (std::min)(max_height, 0xFFFF))
      : 0;
  if (
      !SetPropW(
          window,
          kMinSizeProperty,
          reinterpret_cast<HANDLE>(
              static_cast<INT_PTR>(
                  MAKELONG(
                      clamped_min_width,
                      clamped_min_height
                  )
              )
          )
      )
  ) {
    return false;
  }
  if (
      !SetPropW(
          window, kMaxSizeProperty,
          reinterpret_cast<HANDLE>(
              static_cast<INT_PTR>(
                  MAKELONG(clamped_max_width, clamped_max_height)
              )
          )
      )
  ) {
    RemovePropW(window, kMinSizeProperty);
    return false;
  }
  if (frameless) {
    if (
        !SetPropW(
            window,
            kFramelessProperty,
            reinterpret_cast<HANDLE>(static_cast<INT_PTR>(1))
        )
    ) {
      RemovePropW(window, kMaxSizeProperty);
      RemovePropW(window, kMinSizeProperty);
      return false;
    }
  } else {
    RemovePropW(window, kFramelessProperty);
  }
  DWORD_PTR existing_reference = 0;
  if (
      !GetWindowSubclass(
          window,
          &KSMWindowFrame::SubclassProc,
          kFrameSubclassId,
          &existing_reference
      )
  ) {
    if (
        !SetWindowSubclass(
            window,
            &KSMWindowFrame::SubclassProc,
            kFrameSubclassId,
            reinterpret_cast<DWORD_PTR>(window)
        )
    ) {
      RemovePropW(window, kMaxSizeProperty);
      RemovePropW(window, kMinSizeProperty);
      RemovePropW(window, kFramelessProperty);
      return false;
    }
  }

  if (frameless) {
    ApplyFramelessStyle(window, resizable);
    SetClassLongPtrW(window, GCLP_HBRBACKGROUND, 0);
    RefreshResizeHitTesting(window);
    ApplyRoundedCorners(window);
  }
  if (frameless) {
    if (resizable) {
      UpdateResizePollTimer(window, kResizePollSlowMs);
    } else {
      KillTimer(window, kResizePollTimerId);
      RemovePropW(window, kResizePollIntervalProperty);
    }
  }
  return true;
}

bool KSMWindowFrame::IsFrameless(HWND window) {
  return window && GetPropW(window, kFramelessProperty) != nullptr;
}

bool KSMWindowFrame::StartDrag(HWND window) {
  if (!window || !IsWindow(window)) {
    return false;
  }
  if (GetCurrentThreadId() == GetWindowThreadProcessId(window, nullptr)) {
    return StartDragOnWindowThread(window);
  }
  if (IsFrameless(window)) {
    return PostMessageW(window, kStartDragMessage, 0, 0) != FALSE;
  }
  POINT cursor{};
  GetCursorPos(&cursor);
  return PostMessageW(
      window,
      WM_NCLBUTTONDOWN,
      HTCAPTION,
      MAKELPARAM(cursor.x, cursor.y)
  ) != FALSE;
}

bool KSMWindowFrame::StartResize(
    HWND window,
    KSMWindowResizeDirection direction
) {
  if (!IsResizeAvailable(window) || GetCapture() == window) {
    return false;
  }
  return PostMessageW(
      window,
      kStartResizeMessage,
      static_cast<WPARAM>(direction),
      0
  ) != FALSE;
}

void KSMWindowFrame::RefreshResizeHitTesting(HWND window) {
  if (!IsFrameless(window)) {
    return;
  }
  RegisterResizeRawInput(window);
  SubclassDescendants(window, window);
}

void KSMWindowFrame::SubclassDescendants(HWND frame, HWND parent) {
  for (HWND child = GetWindow(parent, GW_CHILD); child != nullptr;
       child = GetWindow(child, GW_HWNDNEXT)) {
    if (GetWindowThreadProcessId(child, nullptr) == GetCurrentThreadId()) {
      DWORD_PTR existing_reference = 0;
      if (
          !GetWindowSubclass(
              child,
              &KSMWindowFrame::ChildSubclassProc,
              kChildSubclassId,
              &existing_reference
          )
      ) {
        SetWindowSubclass(
            child,
            &KSMWindowFrame::ChildSubclassProc,
            kChildSubclassId,
            reinterpret_cast<DWORD_PTR>(frame)
        );
      }
    }
    SubclassDescendants(frame, child);
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
  if (message == WM_NCLBUTTONDOWN && IsFrameless(parent)) {
    SendMessageW(parent, message, wparam, lparam);
    return 0;
  }
  if (message == WM_PARENTNOTIFY && LOWORD(wparam) == WM_CREATE) {
    RefreshResizeHitTesting(parent);
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
  const bool frameless = IsFrameless(window);
  if (!frameless && message != WM_GETMINMAXINFO) {
    return DefSubclassProc(window, message, wparam, lparam);
  }

  switch (message) {
  case WM_ERASEBKGND:
    return 1;
  case WM_PAINT: {
    PAINTSTRUCT paint{};
    BeginPaint(window, &paint);
    EndPaint(window, &paint);
    return 0;
  }
  case kStartDragMessage:
    StartDragOnWindowThread(window);
    return 0;
  case kStartResizeMessage: {
    const WPARAM direction = wparam;
    if (direction <= static_cast<WPARAM>(
            KSMWindowResizeDirection::BottomRight
        )) {
      StartResizeOnWindowThread(
          window,
          static_cast<KSMWindowResizeDirection>(direction)
      );
    }
    return 0;
  }
  case WM_NCCALCSIZE:
    if (wparam == TRUE) {
      auto *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(lparam);
      if (IsWindowMaximized(window)) {
        MONITORINFO monitor_info{};
        monitor_info.cbSize = sizeof(monitor_info);
        const HMONITOR monitor =
            MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
        if (GetMonitorInfoW(monitor, &monitor_info)) {
          params->rgrc[0] = monitor_info.rcWork;
        }
      } else if (params->lppos) {
        params->rgrc[0] = RECT{
            params->lppos->x,
            params->lppos->y,
            params->lppos->x + params->lppos->cx,
            params->lppos->y + params->lppos->cy,
        };
      } else {
        RECT window_rect{};
        if (GetWindowRect(window, &window_rect)) {
          params->rgrc[0] = window_rect;
        }
      }
      return 0;
    }
    break;
  case WM_GETMINMAXINFO: {
    auto *info = reinterpret_cast<MINMAXINFO *>(lparam);
    if (!frameless) {
      const LRESULT result =
          DefSubclassProc(window, message, wparam, lparam);
      ApplyConfiguredTrackLimits(window, info);
      return result;
    }
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    const HMONITOR monitor =
        MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
    if (GetMonitorInfoW(monitor, &monitor_info)) {
      info->ptMaxPosition.x =
          monitor_info.rcWork.left - monitor_info.rcMonitor.left;
      info->ptMaxPosition.y =
          monitor_info.rcWork.top - monitor_info.rcMonitor.top;
      info->ptMaxSize.x =
          monitor_info.rcWork.right - monitor_info.rcWork.left;
      info->ptMaxSize.y =
          monitor_info.rcWork.bottom - monitor_info.rcWork.top;
    }
    ApplyConfiguredTrackLimits(window, info);
    return 0;
  }
  case WM_INPUT: {
    RAWINPUT raw{};
    UINT size = sizeof(raw);
    if (
        GetRawInputData(
            reinterpret_cast<HRAWINPUT>(lparam),
            RID_INPUT,
            &raw,
            &size,
            sizeof(RAWINPUTHEADER)
        ) != size ||
        raw.header.dwType != RIM_TYPEMOUSE
    ) {
      break;
    }
    const RAWMOUSE &mouse = raw.data.mouse;
    if ((mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) != 0) {
      if (StartResizeFromRawInput(window)) {
        return 0;
      }
    } else if (mouse.lLastX != 0 || mouse.lLastY != 0) {
      UpdateResizeCursor(window);
    }
    break;
  }
  case WM_TIMER:
    if (wparam == kResizePollTimerId) {
      PollResizeEdge(window);
      return 0;
    }
    break;
  case WM_SIZE:
    ApplyRoundedCorners(window);
    break;
  case WM_DPICHANGED:
    ApplyRoundedCorners(window);
    break;
  case WM_ACTIVATE:
    if (LOWORD(wparam) != WA_INACTIVE) {
      RefreshResizeHitTesting(window);
      if (IsResizeAvailable(window)) {
        UpdateResizePollTimer(window, kResizePollFastMs);
      }
    } else {
      UpdateResizePollTimer(window, kResizePollSlowMs);
    }
    break;
  case WM_PARENTNOTIFY:
    if (LOWORD(wparam) == WM_CREATE) {
      RefreshResizeHitTesting(window);
    }
    break;
  case WM_NCHITTEST:
    return HitTestFramelessWindow(window, lparam);
  case WM_NCDESTROY:
    KillTimer(window, kResizePollTimerId);
    RemoveWindowSubclass(
        window,
        &KSMWindowFrame::SubclassProc,
        kFrameSubclassId
    );
    RemovePropW(window, kLeftButtonProperty);
    RemovePropW(window, kResizePollIntervalProperty);
    RemovePropW(window, kMinSizeProperty);
    RemovePropW(window, kRoundedRegionProperty);
    RemovePropW(window, kFramelessProperty);
    break;
  default:
    break;
  }

  return DefSubclassProc(window, message, wparam, lparam);
}
