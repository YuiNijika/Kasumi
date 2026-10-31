plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val ksmPackageName = providers.gradleProperty("ksmPackageName")
    .orElse("dev.kasumi.example")
    .get()
val ksmVersion = providers.gradleProperty("ksmVersion")
    .orElse("0.1.0")
    .get()
val ksmCommands = providers.gradleProperty("ksmCommands")
    .orElse("")
    .get()
    .split(',')
    .filter { it.isNotBlank() }
val ksmCommandsLiteral = ksmCommands.joinToString(
    prefix = "{",
    postfix = "}",
    separator = ", "
) { command -> "\"$command\"" }

android {
    namespace = "dev.kasumi.example"
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
            assets.srcDir("../../showcase")
        }
    }
}

dependencies {
    implementation("dev.kasumi:kasumi:0.1.0")
}
