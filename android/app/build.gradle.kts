plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
}

// Build number from GitHub Actions; local builds count as 1.
val buildNumber = (System.getenv("GITHUB_RUN_NUMBER") ?: "1").toInt()
// Release keystore (PKCS12) is provided by CI; without it the debug key is used.
val keystorePath: String? = System.getenv("FMA_KEYSTORE")

android {
    namespace = "ch.sideffects.findmyanything"
    compileSdk = 37

    defaultConfig {
        applicationId = "ch.sideffects.findmyanything"
        minSdk = 26
        targetSdk = 36
        versionCode = buildNumber
        versionName = "1.0.$buildNumber"
    }

    signingConfigs {
        create("findmyanything") {
            if (keystorePath != null) {
                storeFile = file(keystorePath)
                storePassword = System.getenv("FMA_KS_PASS")
                keyAlias = "findmyanything"
                keyPassword = System.getenv("FMA_KS_PASS")
                storeType = "pkcs12"
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = if (keystorePath != null) {
                signingConfigs.getByName("findmyanything")
            } else {
                signingConfigs.getByName("debug")
            }
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildFeatures {
        compose = true
    }
}

kotlin {
    jvmToolchain(17)
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.material3)
    implementation(libs.kotlinx.coroutines.android)
    implementation(libs.pebblekit)
}
