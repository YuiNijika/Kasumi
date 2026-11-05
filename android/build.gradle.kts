import groovy.json.JsonSlurper

plugins {
    id("com.android.application") version "8.11.0" apply false
    id("com.android.library") version "8.11.0" apply false
    id("org.jetbrains.kotlin.android") version "2.2.10" apply false
}

val versionMetadata = JsonSlurper().parse(
    rootDir.parentFile.resolve("version.json")
) as Map<*, *>
extra["kasumiVersion"] = versionMetadata["version"] as String
