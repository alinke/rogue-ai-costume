// The preview's C entry points (../../../../../ios-jacket/JacketRemote/JacketPreview.h)
// for Kotlin: PreviewEngine.kt.
#include <jni.h>

#include "JacketPreview.h"

#define JNI(name) Java_org_pixelcade_arcadejacket_PreviewEngine_##name

extern "C" {

JNIEXPORT jint JNICALL JNI(count)(JNIEnv*, jobject) { return jp_count(); }

// out: part, side, t for LED i.
JNIEXPORT void JNICALL JNI(pixel)(JNIEnv* env, jobject, jint i, jfloatArray out) {
  int part = 0, side = 0;
  float t = 0;
  jp_pixel(i, &part, &side, &t);
  jfloat v[3] = {(jfloat)part, (jfloat)side, t};
  env->SetFloatArrayRegion(out, 0, 3, v);
}

JNIEXPORT void JNICALL JNI(command)(JNIEnv*, jobject, jint cmd, jint a, jint b, jint c) {
  jp_command((uint8_t)cmd, (uint8_t)a, (uint8_t)b, (uint8_t)c);
}

JNIEXPORT void JNICALL JNI(music)(JNIEnv*, jobject, jint bass, jint mid, jint treble, jint level, jint beat) {
  jp_music((uint8_t)bass, (uint8_t)mid, (uint8_t)treble, (uint8_t)level, (uint8_t)beat);
}

JNIEXPORT void JNICALL JNI(heart)(JNIEnv*, jobject, jint bpm) { jp_heart((uint8_t)bpm); }

JNIEXPORT void JNICALL JNI(render)(JNIEnv* env, jobject, jintArray out) {
  jsize n = env->GetArrayLength(out);
  jint* p = env->GetIntArrayElements(out, nullptr);
  if (n >= jp_count()) jp_render((uint32_t*)p);
  env->ReleaseIntArrayElements(out, p, 0);
}

}  // extern "C"
