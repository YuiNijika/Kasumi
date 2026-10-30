package dev.kasumi.app.bridge

import dev.kasumi.app.generated.KSMGeneratedContract

object KSMPermission {
    fun request(permission: String): Boolean {
        require(permission in KSMGeneratedContract.permissions) {
            "Unknown Kasumi permission: $permission"
        }
        return KSM.requireHostServices().requestPermission(permission)
    }
}
