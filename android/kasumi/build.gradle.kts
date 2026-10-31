plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
    id("maven-publish")
}

group = "dev.kasumi"
version = "0.1.0"

android {
    namespace = "dev.kasumi.app"
    compileSdk = 36

    defaultConfig {
        minSdk = 24
        consumerProguardFiles("consumer-rules.pro")
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    kotlinOptions {
        jvmTarget = "17"
    }

    publishing {
        singleVariant("release")
    }
}

afterEvaluate {
    publishing {
        publications {
            register<MavenPublication>("release") {
                artifactId = "kasumi"
                from(components["release"])
            }
        }
        repositories {
            maven {
                name = "local"
                url = uri("$rootDir/kasumi/build/repo")
            }
        }
    }
}

dependencies {
    api("androidx.core:core-ktx:1.13.1")
    api("androidx.webkit:webkit:1.14.0")
    api("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20240303")
}
