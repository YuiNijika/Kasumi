#pragma once

#include <windows.h>

class KSMWindowFrame final {
public:
  KSMWindowFrame() = delete;

  static bool Configure(HWND window, bool decorations);
  static bool IsFrameless(HWND window);
  static void RefreshResizeHitTesting(HWND window);

private:
  static LRESULT CALLBACK ChildSubclassProc(
      HWND window,
      UINT message,
      WPARAM wparam,
      LPARAM lparam,
      UINT_PTR subclass_id,
      DWORD_PTR reference
  );
  static LRESULT CALLBACK SubclassProc(
      HWND window,
      UINT message,
      WPARAM wparam,
      LPARAM lparam,
      UINT_PTR subclass_id,
      DWORD_PTR reference
  );
};
