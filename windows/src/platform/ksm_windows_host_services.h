#pragma once

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <unordered_map>

#include "ksm_host_services.h"

class KSMWindowsHostServices final : public KSMHostServices {
public:
    explicit KSMWindowsHostServices(HWND window);

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

private:
    std::optional<std::string> RegisterPicked(
        const std::filesystem::path& path
    );
    std::optional<std::filesystem::path> ResolvePicked(
        const std::string& path
    ) const;

    HWND window_;
    mutable std::mutex picked_mutex_;
    std::unordered_map<std::string, std::filesystem::path> picked_files_;
    std::uint64_t next_picked_id_ = 1;
};
