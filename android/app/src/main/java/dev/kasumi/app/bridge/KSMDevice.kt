package dev.kasumi.app.bridge

object KSMDevice {
    fun getInfo(): KSMDeviceInfo? {
        return KSM.requireHostServices().deviceInfo()
    }
}
