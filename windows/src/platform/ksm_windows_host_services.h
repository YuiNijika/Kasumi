#pragma once

#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

#include "ksm_host_services.h"
#include "ksm_url_policy.h"
#include "ksm_version.h"

class KSMFileSystem;

class KSMWindowsHostServices final : public KSMHostServices {
public:
    explicit KSMWindowsHostServices(
        HWND window,
        std::vector<std::string> allowed_external_origins = {},
        std::shared_ptr<KSMFileSystem> file_system = nullptr,
        std::string app_version = KASUMI_VERSION_STRING,
        std::string app_id = {}
    );
    KSMWindowsHostServices(
        HWND window,
        std::vector<std::string> allowed_external_origins,
        std::vector<std::string> allowed_http_origins,
        std::shared_ptr<KSMFileSystem> file_system = nullptr,
        std::string app_version = KASUMI_VERSION_STRING,
        std::string app_id = {}
    );
    ~KSMWindowsHostServices() override;

    std::string AppVersion() const override;
    std::string AppId() const override;
    std::string Language() const override;
    bool ShowMessage(const std::string& title, const std::string& message) override;
    bool Confirm(const std::string& title, const std::string& message) override;
    std::optional<std::string> OpenFile(
        const std::vector<std::string>& filters
    ) override;
    std::optional<std::string> SaveFile(
        const std::string& suggested_name
    ) override;
    std::optional<std::string> ReadPickedText(
        const std::string& path
    ) override;
    bool WritePickedText(
        const std::string& path,
        const std::string& content
    ) override;
    bool PickedExists(const std::string& path) override;
    std::optional<KSMFileMetadata> PickedStat(
        const std::string& path
    ) override;
    bool RemovePicked(const std::string& path) override;
    std::optional<std::string> ReadClipboardText() override;
    bool WriteClipboardText(const std::string& text) override;
    bool ShowNotification(
        const std::string& title,
        const std::string& body
    ) override;
    bool SetWindowTitle(const std::string& title) override;
    bool MinimizeWindow() override;
    bool MaximizeWindow() override;
    bool CloseWindow() override;
    bool ExitApp() override;
    std::optional<KSMHttpDownloadResult> Download(
        const std::string& url,
        const std::string& path,
        bool overwrite,
        const std::string& request_id
    ) override;
    bool IsExternalUrlAllowed(const std::string& url) const override;
    bool OpenExternal(const std::string& url) override;
    bool OpenPath(const std::string& path) override;
    bool ShowInFolder(const std::string& path) override;
    bool IsPrimaryInstance() const override;
    bool FocusMainWindow() override;
    bool TrayShow() override;
    bool TrayHide() override;
    bool RegisterGlobalShortcut(const std::string& shortcut) override;
    bool UnregisterGlobalShortcut(const std::string& shortcut) override;
    bool CheckUpdate(
        bool& available,
        std::string& version
    ) override;
    bool DownloadUpdate(const std::string& version) override;
    bool SetWindowSize(const KSMWindowSize& size) override;
    std::optional<KSMDeviceInfo> GetDeviceInfo() const override;
    std::optional<KSMOperatingSystemInfo>
    GetOperatingSystemInfo() const override;
    bool WriteLog(
        const std::string& level,
        const std::string& target,
        const std::string& message
    ) override;
    void CancelRequest(const std::string& request_id) override;

#if defined(KASUMI_ENABLE_TEST_HOOKS)
    void SetDownloadTestGate(std::function<void()> gate) {
        std::lock_guard lock(download_test_gate_mutex_);
        download_test_gate_ = std::move(gate);
    }
#endif

private:
    struct DownloadState {
        std::mutex mutex;
        HINTERNET session = nullptr;
        HINTERNET connect = nullptr;
        HINTERNET request = nullptr;
        std::atomic<bool> cancelled{false};
    };

    std::optional<std::string> RegisterPicked(
        const std::filesystem::path& path
    );
    std::optional<std::filesystem::path> ResolvePicked(
        const std::string& path
    ) const;
    void CloseDownloadHandles(const std::shared_ptr<DownloadState>& state);
    void SetDownloadHandles(
        const std::shared_ptr<DownloadState>& state,
        HINTERNET session,
        HINTERNET connect,
        HINTERNET request
    );
    bool IsDownloadCancelled(
        const std::shared_ptr<DownloadState>& state
    ) const;

    HWND window_;
    std::shared_ptr<KSMFileSystem> file_system_;
    std::string app_version_;
    std::string app_id_;
    std::set<KSMUrlPolicy::Origin> allowed_external_origins_;
    std::set<KSMUrlPolicy::Origin> allowed_http_origins_;
    mutable std::mutex picked_mutex_;
    std::unordered_map<std::string, std::filesystem::path> picked_files_;
    std::uint64_t next_picked_id_ = 1;
    std::mutex picker_mutex_;
    mutable std::mutex download_mutex_;
    std::unordered_map<std::string, std::shared_ptr<DownloadState>> downloads_;
#if defined(KASUMI_ENABLE_TEST_HOOKS)
    std::mutex download_test_gate_mutex_;
    std::function<void()> download_test_gate_;
#endif
};
