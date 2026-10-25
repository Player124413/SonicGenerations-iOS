// Sonic Generations for Android - the launcher app around the recompiled game.
//
// The native side (src/main/cpp) builds the ReXGlue SDK from
// ../../ios/rexglue-sdk (the same SDK tree the iOS build uses), the game code
// generated from your default.xex into ../../sonicgenerations/generated, and
// the app itself (the same src/ the iOS build compiles, with the Android
// sections of sonicgenerations_app.h).

plugins {
    id("com.android.application")
}

// Paths into the repository (android/ is the Gradle root).
val repoRoot: java.io.File = rootProject.projectDir.parentFile
val rexPortDir: String = (project.findProperty("rexPortDir") as String?)
    ?: "${rootProject.projectDir}/sonicgenerations"
val rexSdkDir: String = (project.findProperty("rexSdkDir") as String?)
    ?: "$repoRoot/ios/rexglue-sdk"
val rexVersionCode: Int = ((project.findProperty("rexVersionCode") as String?) ?: "1").toInt()
val rexVersionName: String = (project.findProperty("rexVersionName") as String?) ?: "1.0"

android {
    namespace = "com.devz.sonicgenerations"
    compileSdk = 35
    ndkVersion = "27.2.12479018"

    defaultConfig {
        applicationId = "com.devz.sonicgenerations"
        minSdk = 28
        targetSdk = 35
        versionCode = rexVersionCode
        versionName = rexVersionName

        resValue("string", "app_name", "Sonic Generations")
        buildConfigField("String", "GAME_TITLE", "\"Sonic Generations\"")
        buildConfigField("String", "TITLE_ID", "\"53450848\"")
        buildConfigField("String", "PROJECT", "\"sonicgenerations\"")

        ndk { abiFilters += "arm64-v8a" }

        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=c++_static",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DREXSDK_DIR=$rexSdkDir",
                    "-DREX_PORT_DIR=$rexPortDir",
                    "-DREX_APP_NAME=sonicgenerations",
                )
                cppFlags += listOf("-std=c++23")
            }
        }
    }

    buildFeatures { buildConfig = true }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.31.1"
        }
    }

    sourceSets {
        getByName("main") {
            assets.srcDirs("src/main/assets")
        }
    }

    signingConfigs {
        // Deterministic debug-style key so an APK can be installed over the
        // previous one (a fresh random debug key per build would force an
        // uninstall each time). Create android/ci-release.jks to use it.
        create("ci") {
            val ks = file("${rootProject.projectDir}/ci-release.jks")
            if (ks.exists()) {
                storeFile = ks
                storePassword = "rexauto"
                keyAlias = "rexauto"
                keyPassword = "rexauto"
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            if (file("${rootProject.projectDir}/ci-release.jks").exists()) {
                signingConfig = signingConfigs.getByName("ci")
            }
        }
        debug { isJniDebuggable = true }
    }

    packaging {
        jniLibs { useLegacyPackaging = true }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
