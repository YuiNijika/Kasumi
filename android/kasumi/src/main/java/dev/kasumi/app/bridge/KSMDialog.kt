package dev.kasumi.app.bridge

object KSMDialog {
    fun message(title: String, message: String): Boolean {
        return KSM.requireHostServices().showMessage(title, message)
    }

    fun confirm(title: String, message: String): Boolean {
        return KSM.requireHostServices().confirm(title, message)
    }

    fun openFile(filters: List<String> = emptyList()): String? {
        return KSM.requireHostServices().openFile(filters)
    }

    fun saveFile(suggestedName: String = ""): String? {
        return KSM.requireHostServices().saveFile(suggestedName)
    }
}
