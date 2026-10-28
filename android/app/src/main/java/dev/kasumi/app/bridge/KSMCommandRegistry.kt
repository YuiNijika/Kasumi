package dev.kasumi.app.bridge

import dev.kasumi.app.generated.KSMGeneratedContract

object KSMCommandRegistry {
    val permissions: List<String> = KSMGeneratedContract.permissions

    val supportedCommands: List<String> = KSMGeneratedContract.androidCommands
}
