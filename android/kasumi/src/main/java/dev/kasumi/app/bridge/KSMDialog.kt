package dev.kasumi.app.bridge

object KSMDialog {
    fun message(title: String, message: String): Boolean {
        if (!KSM.isCommandEnabled("dialog.message")) {
            return false
        }
        return KSM.requireHostServices().showMessage(title, message)
    }

    fun confirm(title: String, message: String): Boolean {
        if (!KSM.isCommandEnabled("dialog.confirm")) {
            return false
        }
        return KSM.requireHostServices().confirm(title, message)
    }

    fun openFile(filters: List<String> = emptyList()): String? {
        if (!KSM.isCommandEnabled("dialog.openFile")) {
            return null
        }
        return KSM.requireHostServices().openFile(filters)
    }

    fun saveFile(suggestedName: String = ""): String? {
        if (!KSM.isCommandEnabled("dialog.saveFile")) {
            return null
        }
        return KSM.requireHostServices().saveFile(suggestedName)
    }
}
