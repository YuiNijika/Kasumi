plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

val kasumiVersion = rootProject.extra["kasumiVersion"] as String
val ksmPackageName = providers.gradleProperty("ksmPackageName")
    .orElse("dev.kasumi.example")
    .get()
val ksmVersion = providers.gradleProperty("ksmVersion")
    .orElse(kasumiVersion)
    .get()
fun csvProperty(name: String): List<String> =
    providers.gradleProperty(name)
        .orElse("")
        .get()
        .split(',')
        .map { value -> value.trim() }
        .filter { value -> value.isNotEmpty() }

fun stringArrayLiteral(values: List<String>): String =
    values.joinToString(prefix = "{", postfix = "}", separator = ", ") { value ->
        val escaped = value
            .replace("\\", "\\\\")
            .replace("\"", "\\\"")
        "\"$escaped\""
    }

val ksmCommands = csvProperty("ksmCommands")
val ksmPermissions = csvProperty("ksmPermissions")
val ksmShellAllowedOrigins = csvProperty("ksmShellAllowedOrigins")
val ksmHttpAllowedOrigins = csvProperty("ksmHttpAllowedOrigins")
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
    namespace = "dev.kasumi.example"
    compileSdk = 36

    defaultConfig {
        applicationId = ksmPackageName
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = ksmVersion
        buildConfigField(
            "String[]",
            "KSM_COMMANDS",
            stringArrayLiteral(ksmCommands)
        )
        buildConfigField(
            "String[]",
            "KSM_PERMISSIONS",
            stringArrayLiteral(ksmPermissions)
        )
        buildConfigField(
            "String[]",
            "KSM_SHELL_ALLOWED_ORIGINS",
            stringArrayLiteral(ksmShellAllowedOrigins)
        )
        buildConfigField(
            "String[]",
            "KSM_HTTP_ALLOWED_ORIGINS",
            stringArrayLiteral(ksmHttpAllowedOrigins)
        )
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
            assets.srcDir("../../showcase")
        }
    }
}

dependencies {
    implementation("dev.kasumi:kasumi:$kasumiVersion")
}
