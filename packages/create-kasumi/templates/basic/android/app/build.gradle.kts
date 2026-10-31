plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val ksmPackageName = providers.gradleProperty("ksmPackageName")
    .orElse("__ANDROID_PACKAGE_NAME__")
    .get()
val ksmVersion = providers.gradleProperty("ksmVersion")
    .orElse("__KASUMI_VERSION__")
    .get()
val ksmCommands = providers.gradleProperty("ksmCommands")
    .orElse("__ANDROID_COMMANDS_CSV__")
    .get()
    .split(',')
    .filter { it.isNotBlank() }
val ksmCommandsLiteral = ksmCommands.joinToString(
    prefix = "{",
    postfix = "}",
    separator = ", "
) { command -> "\"$command\"" }

android {
    namespace = "__ANDROID_PACKAGE_NAME__"
    compileSdk = 36

    defaultConfig {
        applicationId = ksmPackageName
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = ksmVersion
        buildConfigField("String[]", "KSM_COMMANDS", ksmCommandsLiteral)
    }

    buildTypes {
        release {
            isMinifyEnabled = false
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
    implementation("dev.kasumi:kasumi:__KASUMI_VERSION__")
}
