import java.io.File
import java.util.Properties

plugins {
    id("com.android.application")
    id("com.android.legacy-kapt")
    id("org.jetbrains.kotlin.plugin.compose")
    id("androidx.room")
}

val releaseSigningProperties = Properties()
val releaseSigningPropertiesFile = rootProject.file("keystore.properties")
if (releaseSigningPropertiesFile.isFile) {
    releaseSigningPropertiesFile.inputStream().use(releaseSigningProperties::load)
}

fun releaseSigningValue(propertyName: String, environmentName: String): String? =
    providers.environmentVariable(environmentName).orNull
        ?: releaseSigningProperties.getProperty(propertyName)

val releaseStoreFile =
    releaseSigningValue("storeFile", "BIKE_RELEASE_STORE_FILE")
val releaseStorePassword =
    releaseSigningValue("storePassword", "BIKE_RELEASE_STORE_PASSWORD")
val releaseKeyAlias =
    releaseSigningValue("keyAlias", "BIKE_RELEASE_KEY_ALIAS")
val releaseKeyPassword =
    releaseSigningValue("keyPassword", "BIKE_RELEASE_KEY_PASSWORD")
val releaseSigningConfigured = listOf(
    releaseStoreFile,
    releaseStorePassword,
    releaseKeyAlias,
    releaseKeyPassword,
).all { !it.isNullOrBlank() }

android {
    namespace = "com.diybikecomputer.companion"
    compileSdk = 36

    defaultConfig {
        applicationId = "com.diybikecomputer.companion"
        minSdk = 26
        targetSdk = 36
        versionCode = 2
        versionName = "2.1.1"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        vectorDrawables.useSupportLibrary = true
    }

    val releaseSigningConfig = if (releaseSigningConfigured) {
        signingConfigs.create("release") {
            storeFile = rootProject.file(requireNotNull(releaseStoreFile))
            storePassword = requireNotNull(releaseStorePassword)
            keyAlias = requireNotNull(releaseKeyAlias)
            keyPassword = requireNotNull(releaseKeyPassword)
            enableV1Signing = true
            enableV2Signing = true
            enableV3Signing = true
            enableV4Signing = true
        }
    } else {
        null
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = releaseSigningConfig
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        compose = true
        buildConfig = true
    }

    packaging {
        resources.excludes += "/META-INF/{AL2.0,LGPL2.1}"
    }

    testOptions {
        unitTests.isIncludeAndroidResources = true
    }
}

val verifyReleaseSigning by tasks.registering {
    inputs.property("releaseSigningConfigured", releaseSigningConfigured)
    inputs.property("releaseStoreFile", releaseStoreFile.orEmpty())
    doLast {
        check(inputs.properties["releaseSigningConfigured"] == true) {
            "Release signing is not configured. Copy keystore.properties.example to " +
                "keystore.properties or provide the BIKE_RELEASE_* environment variables."
        }
        val storePath = inputs.properties["releaseStoreFile"] as String
        check(File(storePath).isFile) {
            "Release keystore does not exist: $storePath"
        }
    }
}

tasks.configureEach {
    if (name == "preReleaseBuild") {
        dependsOn(verifyReleaseSigning)
    }
}

room {
    schemaDirectory("$projectDir/schemas")
}

kapt {
    arguments {
        arg("room.schemaLocation", "$projectDir/schemas")
        arg("room.incremental", "true")
    }
}

dependencies {
    val composeBom = platform("androidx.compose:compose-bom:2026.06.00")
    implementation(composeBom)
    androidTestImplementation(composeBom)

    implementation("androidx.activity:activity-compose:1.13.0")
    implementation("androidx.compose.material:material-icons-extended")
    implementation("androidx.compose.material3:material3")
    implementation("androidx.compose.ui:ui")
    implementation("androidx.compose.ui:ui-tooling-preview")
    implementation("androidx.lifecycle:lifecycle-runtime-ktx:2.10.0")
    implementation("androidx.lifecycle:lifecycle-runtime-compose:2.10.0")
    implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.10.0")
    implementation("androidx.room:room-runtime:2.8.4")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")

    kapt("androidx.room:room-compiler:2.8.4")

    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20250517")
    androidTestImplementation("androidx.test.ext:junit:1.3.0")
    androidTestImplementation("androidx.compose.ui:ui-test-junit4")
    debugImplementation("androidx.compose.ui:ui-tooling")
    debugImplementation("androidx.compose.ui:ui-test-manifest")
}
