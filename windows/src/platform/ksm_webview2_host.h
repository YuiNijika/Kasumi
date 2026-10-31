#pragma once

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class KSMFileSystem;
class KSMWebResourceProvider;

class KSMWebView2Host {
public:
  using InitializationCallback = std::function<void(bool, const std::string &)>;

  KSMWebView2Host(HWND window, std::shared_ptr<KSMFileSystem> file_system,
                  std::vector<std::string> allowed_external_origins,
                  std::filesystem::path web_root,
                  std::filesystem::path user_data_folder,
                  bool smoke_test = false,
                  std::shared_ptr<KSMWebResourceProvider> web_resources = {});
  ~KSMWebView2Host();

  KSMWebView2Host(const KSMWebView2Host &) = delete;
  KSMWebView2Host &operator=(const KSMWebView2Host &) = delete;

  bool Initialize(InitializationCallback callback = {});
  void Close();

  std::uint64_t window_id() const noexcept;

  static bool IsRuntimeAvailable();
  static std::string RuntimeVersion();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
