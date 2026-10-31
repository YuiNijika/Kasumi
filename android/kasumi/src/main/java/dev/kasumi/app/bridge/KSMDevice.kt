package dev.kasumi.app.bridge

object KSMDevice {
    fun getInfo(): KSMDeviceInfo? {
        if (!KSM.isCommandEnabled("device.getInfo")) {
            return null
        }
        return KSM.requireHostServices().deviceInfo()
    }
}
