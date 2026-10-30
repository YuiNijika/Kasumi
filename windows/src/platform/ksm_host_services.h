#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ksm_file_metadata.h"

struct KSMHttpDownloadResult {
    std::string path;
    std::uint64_t bytes;
};

struct KSMWindowSize {
    int width;
    int height;
};

struct KSMDeviceInfo {
    std::string platform;
    std::string os_version;
    std::string model;
    std::string manufacturer;
    std::string app_version;
    std::string language;
    int screen_width;
    int screen_height;
    double density;
    std::string device_name;
    std::string timezone;
    std::string architecture;
};

class KSMHostException final : public std::runtime_error {
public:
    KSMHostException(std::string code, const std::string& message)
        : std::runtime_error(message), code_(std::move(code)) {
    }

    const std::string& code() const noexcept {
        return code_;
    }

private:
    std::string code_;
};

class KSMHostServices {
public:
    virtual ~KSMHostServices() = default;

    virtual std::string AppVersion() const = 0;
    virtual std::string Language() const = 0;
    virtual bool ShowMessage(const std::string& title, const std::string& message) = 0;
    virtual bool Confirm(const std::string& title, const std::string& message) = 0;
    virtual std::optional<std::string> OpenFile(
        const std::vector<std::string>& filters
    ) = 0;
    virtual std::optional<std::string> SaveFile(
        const std::string& suggested_name
    ) = 0;
    virtual std::optional<std::string> ReadPickedText(
        const std::string& path
    ) = 0;
    virtual bool WritePickedText(
        const std::string& path,
        const std::string& content
    ) = 0;
    virtual bool PickedExists(const std::string& path) = 0;
    virtual std::optional<KSMFileMetadata> PickedStat(
        const std::string& path
    ) = 0;
    virtual bool RemovePicked(const std::string& path) = 0;
    virtual std::optional<std::string> ReadClipboardText() = 0;
    virtual bool WriteClipboardText(const std::string& text) = 0;
    virtual bool ShowNotification(
        const std::string& title,
        const std::string& body
    ) = 0;
    virtual bool SetWindowTitle(const std::string& title) = 0;
    virtual bool MinimizeWindow() = 0;
    virtual bool MaximizeWindow() = 0;
    virtual bool CloseWindow() = 0;
    virtual bool ExitApp() = 0;
    virtual std::optional<KSMHttpDownloadResult> Download(
        const std::string& url,
        const std::string& path,
        bool overwrite,
        const std::string& request_id
    ) {
        (void)url;
        (void)path;
        (void)overwrite;
        (void)request_id;
        return std::nullopt;
    }
    virtual bool IsExternalUrlAllowed(const std::string& url) const {
        (void)url;
        return false;
    }
    virtual bool OpenExternal(const std::string& url) {
        (void)url;
        return false;
    }
    virtual bool SetWindowSize(const KSMWindowSize& size) {
        (void)size;
        return false;
    }
    virtual std::optional<KSMDeviceInfo> GetDeviceInfo() const {
        return std::nullopt;
    }
    virtual void CancelRequest(const std::string& request_id) {
        (void)request_id;
    }
};
