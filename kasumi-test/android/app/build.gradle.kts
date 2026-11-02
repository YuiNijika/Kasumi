plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val ksmPackageName = providers.gradleProperty("ksmPackageName")
    .orElse("dev.kasumi.kasumi_test")
    .get()
val ksmVersion = providers.gradleProperty("ksmVersion")
    .orElse("26.11.2-alpha")
    .get()
val ksmCommands = providers.gradleProperty("ksmCommands")
    .orElse("ping,capabilities.query,app.getInfo,device.getInfo,fs.readText,fs.writeText,fs.exists,fs.remove,fs.readDir,fs.mkdir,fs.stat,dialog.message,dialog.confirm,dialog.openFile,dialog.saveFile,clipboard.readText,clipboard.writeText,notification.show,permission.request,event.subscribe,event.unsubscribe,window.close,shell.openExternal,http.download,app.exit")
    .get()
    .split(',')
    .filter { it.isNotBlank() }
val ksmCommandsLiteral = ksmCommands.joinToString(
    prefix = "{",
    postfix = "}",
    separator = ", "
) { command -> "\"$command\"" }
val releaseKeystore = System.getenv("KASUMI_ANDROID_KEYSTORE")
    ?.takeIf { it.isNotBlank() }
    ?.let { file(it) }
    ?.takeIf { it.isFile }
val releaseStorePassword = System.getenv("KASUMI_ANDROID_KEYSTORE_PASSWORD")
val releaseKeyAlias = System.getenv("KASUMI_ANDROID_KEY_ALIAS")
val releaseKeyPassword = System.getenv("KASUMI_ANDROID_KEY_PASSWORD")
val hasReleaseSigning =
    releaseKeystore != null &&
        !releaseStorePassword.isNullOrBlank() &&
        !releaseKeyAlias.isNullOrBlank() &&
        !releaseKeyPassword.isNullOrBlank()

android {
    namespace = "dev.kasumi.kasumi_test"
    compileSdk = 36

    defaultConfig {
        applicationId = ksmPackageName
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = ksmVersion
        buildConfigField("String[]", "KSM_COMMANDS", ksmCommandsLiteral)
    }

    signingConfigs {
        if (hasReleaseSigning) {
            create("release") {
                storeFile = releaseKeystore
                storePassword = releaseStorePassword
                keyAlias = releaseKeyAlias
                keyPassword = releaseKeyPassword
                enableV1Signing = true
                enableV2Signing = true
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            if (hasReleaseSigning) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    buildFeatures {
        buildConfig = true
    }

    sourceSets {
        getByName("main") {
            assets.srcDir("../../frontend/dist")
        }
    }
}

dependencies {
    implementation("dev.kasumi:kasumi:26.11.2-alpha")
}
