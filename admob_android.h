// JNI calls into io.velo.admob.AdBridge, the Google AdMob bridge for Android.
// Native game thread never blocks on Java: init/show/retry post to the UI thread,
// while ready/poll read volatile atomic flags.

#if defined(__ANDROID__)
#include <jni.h>
#include <stdio.h>
#include <android/native_activity.h>

static JNIEnv* velo_admob_env(const ANativeActivity* act) {
	if (act == NULL || act->vm == NULL) return NULL;
	JavaVM* vm = act->vm;
	JNIEnv* env = NULL;
	if ((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_6) != JNI_OK) {
		if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK) return NULL;
	}
	return env;
}

static jclass velo_admob_class(JNIEnv* env, const ANativeActivity* act) {
	static jclass cached = NULL;
	if (cached != NULL) return cached;
	jclass act_cls = (*env)->GetObjectClass(env, act->clazz);
	jmethodID get_loader = (*env)->GetMethodID(env, act_cls, "getClassLoader", "()Ljava/lang/ClassLoader;");
	jobject loader = (*env)->CallObjectMethod(env, act->clazz, get_loader);
	jclass loader_cls = (*env)->GetObjectClass(env, loader);
	jmethodID load_class = (*env)->GetMethodID(env, loader_cls, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
	jstring name = (*env)->NewStringUTF(env, "io.velo.admob.AdBridge");
	jclass cls = (jclass)(*env)->CallObjectMethod(env, loader, load_class, name);
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		jstring fallback_name = (*env)->NewStringUTF(env, "io.velo.ads.AdBridge");
		cls = (jclass)(*env)->CallObjectMethod(env, loader, load_class, fallback_name);
		if ((*env)->ExceptionCheck(env)) {
			(*env)->ExceptionClear(env);
			cls = NULL;
		}
		(*env)->DeleteLocalRef(env, fallback_name);
	}
	if (cls != NULL) cached = (jclass)(*env)->NewGlobalRef(env, cls);
	(*env)->DeleteLocalRef(env, name);
	(*env)->DeleteLocalRef(env, loader_cls);
	(*env)->DeleteLocalRef(env, loader);
	(*env)->DeleteLocalRef(env, act_cls);
	return cached;
}

static void velo_admob_call_activity_2strings(const void* activity, const char* method, const char* s1, const char* s2) {
	const ANativeActivity* act = (const ANativeActivity*)activity;
	JNIEnv* env = velo_admob_env(act);
	if (env == NULL) return;
	jclass cls = velo_admob_class(env, act);
	if (cls == NULL) return;
	jmethodID m = (*env)->GetStaticMethodID(env, cls, method, "(Landroid/app/Activity;Ljava/lang/String;Ljava/lang/String;)V");
	if ((*env)->ExceptionCheck(env)) {
		(*env)->ExceptionClear(env);
		// Try 1-string version for legacy AdBridge: init(Activity, String)
		m = (*env)->GetStaticMethodID(env, cls, method, "(Landroid/app/Activity;Ljava/lang/String;)V");
		if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return; }
		jstring js1 = (*env)->NewStringUTF(env, s1 ? s1 : "");
		(*env)->CallStaticVoidMethod(env, cls, m, act->clazz, js1);
		(*env)->DeleteLocalRef(env, js1);
		if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
		return;
	}
	jstring js1 = (*env)->NewStringUTF(env, s1 ? s1 : "");
	jstring js2 = (*env)->NewStringUTF(env, s2 ? s2 : "");
	(*env)->CallStaticVoidMethod(env, cls, m, act->clazz, js1, js2);
	(*env)->DeleteLocalRef(env, js1);
	(*env)->DeleteLocalRef(env, js2);
	if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}

static void velo_admob_call_activity_void(const void* activity, const char* method) {
	const ANativeActivity* act = (const ANativeActivity*)activity;
	JNIEnv* env = velo_admob_env(act);
	if (env == NULL) return;
	jclass cls = velo_admob_class(env, act);
	if (cls == NULL) return;
	jmethodID m = (*env)->GetStaticMethodID(env, cls, method, "(Landroid/app/Activity;)V");
	if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return; }
	(*env)->CallStaticVoidMethod(env, cls, m, act->clazz);
	if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
}

static int velo_admob_call_bool(const void* activity, const char* method) {
	const ANativeActivity* act = (const ANativeActivity*)activity;
	JNIEnv* env = velo_admob_env(act);
	if (env == NULL) return 0;
	jclass cls = velo_admob_class(env, act);
	if (cls == NULL) return 0;
	jmethodID m = (*env)->GetStaticMethodID(env, cls, method, "()Z");
	if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
	jboolean r = (*env)->CallStaticBooleanMethod(env, cls, m);
	if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
	return r ? 1 : 0;
}

static int velo_admob_call_int(const void* activity, const char* method) {
	const ANativeActivity* act = (const ANativeActivity*)activity;
	JNIEnv* env = velo_admob_env(act);
	if (env == NULL) return 0;
	jclass cls = velo_admob_class(env, act);
	if (cls == NULL) return 0;
	jmethodID m = (*env)->GetStaticMethodID(env, cls, method, "()I");
	if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
	int r = (int)(*env)->CallStaticIntMethod(env, cls, m);
	if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
	return r;
}

// Public C Interface
static inline void velo_admob_init(const void* activity, const char* rewarded_unit_id, const char* interstitial_unit_id) {
	velo_admob_call_activity_2strings(activity, "init", rewarded_unit_id, interstitial_unit_id);
}

static inline int velo_admob_is_rewarded_ready(const void* activity) {
	int r = velo_admob_call_bool(activity, "readyRewarded");
	if (!r) {
		// Fallback for legacy AdBridge method name "ready"
		r = velo_admob_call_bool(activity, "ready");
	}
	return r;
}

static inline void velo_admob_retry_rewarded(const void* activity) {
	velo_admob_call_activity_void(activity, "retryRewarded");
}

static inline void velo_admob_show_rewarded(const void* activity) {
	velo_admob_call_activity_void(activity, "showRewarded");
}

static inline int velo_admob_poll_rewarded(const void* activity) {
	int r = velo_admob_call_int(activity, "pollRewardedResult");
	if (r == 0) {
		r = velo_admob_call_int(activity, "pollResult");
	}
	return r;
}

static inline int velo_admob_is_interstitial_ready(const void* activity) {
	return velo_admob_call_bool(activity, "readyInterstitial");
}

static inline void velo_admob_retry_interstitial(const void* activity) {
	velo_admob_call_activity_void(activity, "retryInterstitial");
}

static inline void velo_admob_show_interstitial(const void* activity) {
	velo_admob_call_activity_void(activity, "showInterstitial");
}

static inline int velo_admob_poll_interstitial(const void* activity) {
	return velo_admob_call_int(activity, "pollInterstitialResult");
}

#endif
