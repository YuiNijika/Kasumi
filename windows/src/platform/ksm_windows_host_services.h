#pragma once

#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>

#include "ksm_host_services.h"
#include "ksm_url_policy.h"

class KSMFileSystem;

class KSMWindowsHostServices final : public KSMHostServices {
public:
    explicit KSMWindowsHostServices(
        HWND window,
        std::vector<std::string> allowed_external_origins = {},
        std::shared_ptr<KSMFileSystem> file_system = nullptr
    );

    std::string AppVersion() const override;
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
    bool SetWindowSize(const KSMWindowSize& size) override;
    std::optional<KSMDeviceInfo> GetDeviceInfo() const override;
    void CancelRequest(const std::string& request_id) override;

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
    std::set<KSMUrlPolicy::Origin> allowed_external_origins_;
    mutable std::mutex picked_mutex_;
    std::unordered_map<std::string, std::filesystem::path> picked_files_;
    std::uint64_t next_picked_id_ = 1;
    mutable std::mutex download_mutex_;
    std::unordered_map<std::string, std::shared_ptr<DownloadState>> downloads_;
};
