#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <mutex>

#include <projectM-4/playlist.h>
#include <projectM-4/projectM.h>

#define LOG_TAG "projectm-android"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Global state
static projectm_handle g_pm = nullptr;
static projectm_playlist_handle g_playlist = nullptr;
static std::mutex g_lock;

// Helper: build a stable C-string array for projectM properties.
static std::vector<std::string> g_path_storage;

static void set_preset_search_path(const std::string& assetRoot) {
    // Assets are extracted by ProjectMApplication into <cacheDir>/projectM/...
    // That directory contains: config.inp, fonts/, presets/...
    const std::string presetsDir = assetRoot + "/presets";

    g_path_storage.clear();
    g_path_storage.push_back(presetsDir);

    for (auto& s : g_path_storage) {
        projectm_playlist_add_path(g_playlist, s.c_str(), true, false);
        ALOGI("Preset search path: %s", presetsDir.c_str());
    }

}

static void ensure_initialized(int width, int height, const std::string& assetRoot) {
    if (g_pm) return;

    g_pm = projectm_create();
    if (!g_pm) {
        ALOGE("projectm_create() failed (GL context not ready?)");
        return;
    }

    projectm_set_window_size(g_pm, width, height);

    // Basic config.
    projectm_set_aspect_correction(g_pm, true);
    projectm_set_hard_cut_enabled(g_pm, true);
    projectm_set_soft_cut_duration(g_pm, 10);
    projectm_set_hard_cut_duration(g_pm, 10);
    projectm_set_hard_cut_sensitivity(g_pm, 1.0);
    projectm_set_beat_sensitivity(g_pm, 0.5);
    projectm_set_preset_duration(g_pm, 15);

    // Playlist is the easiest way to do preset navigation with projectM 4.
    g_playlist = projectm_playlist_create(g_pm);

    set_preset_search_path(assetRoot);

    if (g_playlist) {
        projectm_playlist_set_shuffle(g_playlist, false);
        projectm_playlist_set_position(g_playlist, 0, true);
    } else {
        ALOGE("projectm_playlist_create() failed");
    }

    ALOGI("Initialized projectM (%dx%d)", width, height);
}

extern "C" {

// Java: libprojectMJNIWrapper.onSurfaceCreated(int w,int h,String assetPath)
JNIEXPORT void JNICALL
Java_com_github_projectm_1android_libprojectMJNIWrapper_onSurfaceCreated(
        JNIEnv* env, jclass, jint window_width, jint window_height, jstring assetPath) {
    const char* pathChars = env->GetStringUTFChars(assetPath, nullptr);
    std::string assetRoot = pathChars ? pathChars : "";
    env->ReleaseStringUTFChars(assetPath, pathChars);

    std::lock_guard<std::mutex> lk(g_lock);
    ensure_initialized((int)window_width, (int)window_height, assetRoot);
}

JNIEXPORT void JNICALL
Java_com_github_projectm_1android_libprojectMJNIWrapper_onSurfaceChanged(
        JNIEnv*, jclass, jint width, jint height) {
    std::lock_guard<std::mutex> lk(g_lock);
    if (!g_pm) return;
    projectm_set_window_size(g_pm, (int)width, (int)height);
}

JNIEXPORT void JNICALL
Java_com_github_projectm_1android_libprojectMJNIWrapper_onDrawFrame(
        JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(g_lock);
    if (!g_pm) return;

    // Render one frame into the current GL framebuffer.
    projectm_opengl_render_frame(g_pm);
}

JNIEXPORT void JNICALL
Java_com_github_projectm_1android_libprojectMJNIWrapper_addPCM(
        JNIEnv* env, jclass, jshortArray pcm_data, jshort nsamples) {
    std::lock_guard<std::mutex> lk(g_lock);
    if (!g_pm) return;

    jsize len = env->GetArrayLength(pcm_data);
    if (len <= 0) return;

    // nsamples is passed in as "bufferSize" by AudioThread; treat it as frames.
    // AudioThread records mono int16 PCM.

    jboolean isCopy = JNI_FALSE;
    jshort* data = env->GetShortArrayElements(pcm_data, &isCopy);
    if (!data) return;

    // Guard: never read beyond actual array length.
    const int frames = (int)std::min<jsize>(len, (jsize)nsamples);

    projectm_pcm_add_int16(g_pm, reinterpret_cast<const int16_t*>(data), frames, PROJECTM_MONO);

    env->ReleaseShortArrayElements(pcm_data, data, JNI_ABORT);
}

JNIEXPORT void JNICALL
Java_com_github_projectm_1android_libprojectMJNIWrapper_nextPreset(
        JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lk(g_lock);
    if (!g_playlist) return;
    projectm_playlist_play_next(g_playlist, true);
}

} // extern "C"
