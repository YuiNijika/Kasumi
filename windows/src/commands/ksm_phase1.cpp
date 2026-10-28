#include "ksm_phase1.h"

#include "../platform/ksm_host_services.h"
#include "ksm_filesystem.h"
#include "../generated/contract.h"

#include <utility>

KSM::PingResult KSM::Phase1::Ping() {
    return KSM::PingResult{.pong = true};
}

const KSM::Capabilities& KSM::Get() {
    static const KSM::Capabilities capabilities{
        .platform = KSM::Platform::Windows,
        .commands = KSMGeneratedContract::WindowsCommands(),
        .permissions = KSMGeneratedContract::Permissions()
    };
    return capabilities;
}

KSM::Capabilities KSM::Phase1::QueryCapabilities() {
    return KSM::Get();
}

KSM::Fs::Fs(std::shared_ptr<KSMFileSystem> file_system)
    : file_system_(std::move(file_system)) {
}

std::optional<std::string> KSM::Fs::ReadText(const std::string& path) const {
    return file_system_ ? file_system_->ReadText(path) : std::nullopt;
}

bool KSM::Fs::WriteText(
    const std::string& path,
    const std::string& content
) const {
    return file_system_ && file_system_->WriteText(path, content);
}

bool KSM::Fs::Exists(const std::string& path) const {
    return file_system_ && file_system_->Exists(path);
}

bool KSM::Fs::Remove(const std::string& path) const {
    return file_system_ && file_system_->Remove(path);
}

KSM::Dialog::Dialog(std::shared_ptr<KSMHostServices> host_services)
    : host_services_(std::move(host_services)) {
}

bool KSM::Dialog::Message(
    const std::string& title,
    const std::string& message
) const {
    return host_services_ && host_services_->ShowMessage(title, message);
}

bool KSM::Dialog::Confirm(
    const std::string& title,
    const std::string& message
) const {
    return host_services_ && host_services_->Confirm(title, message);
}

std::optional<std::string> KSM::Dialog::OpenFile(
    const std::vector<std::string>& filters
) const {
    return host_services_
        ? host_services_->OpenFile(filters)
        : std::nullopt;
}

std::optional<std::string> KSM::Dialog::SaveFile(
    const std::string& suggested_name
) const {
    return host_services_
        ? host_services_->SaveFile(suggested_name)
        : std::nullopt;
}
