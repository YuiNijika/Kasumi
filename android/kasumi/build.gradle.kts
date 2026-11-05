plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
    id("maven-publish")
}

val kasumiVersion = rootProject.extra["kasumiVersion"] as String
val generatedVersionDirectory = layout.buildDirectory.dir(
    "generated/kasumiVersion"
)
val generatedVersionFile = generatedVersionDirectory.map {
    it.file("dev/kasumi/app/internal/KSMVersionMetadata.kt")
}
val generateVersionSource = tasks.register("generateKasumiVersionSource") {
    inputs.property("version", kasumiVersion)
    outputs.file(generatedVersionFile)
    doLast {
        val outputFile = generatedVersionFile.get().asFile
        outputFile.parentFile.mkdirs()
        outputFile.writeText(
            """
            package dev.kasumi.app.internal

            internal object KSMVersionMetadata {
                const val VERSION = "$kasumiVersion"
            }
            """.trimIndent() + "\n"
        )
    }
}

group = "dev.kasumi"
version = kasumiVersion

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

    sourceSets.getByName("main").kotlin.srcDir(
        generatedVersionDirectory.get().asFile
    )

    publishing {
        singleVariant("release")
    }
}

tasks.named("preBuild").configure {
    dependsOn(generateVersionSource)
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
    api(libs.androidx.core.ktx)
    api(libs.androidx.documentfile)
    api(libs.androidx.media)
    api(libs.androidx.media3.exoplayer)
    api(libs.androidx.media3.session)
    api("androidx.webkit:webkit:1.14.0")
    api("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20240303")
}
