#pragma once

#include <optional>
#include <string>
#include <vector>

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
};
