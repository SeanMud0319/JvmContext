#include <jni.h>
#include <jvmti.h>
#include <cstdlib>
#include <cstring>

struct JPLISAgentStruct;

struct JPLISEnvironment {
    jvmtiEnv *mJVMTIEnv;
    JPLISAgentStruct *mAgent;
    jboolean mIsRetransformer;
};

struct JPLISAgentStruct {
    JavaVM *mJVM;
    JPLISEnvironment mNormalEnvironment;
    JPLISEnvironment mRetransformEnvironment;
    jobject mInstrumentationImpl;
    jmethodID mPremainCaller;
    jmethodID mAgentmainCaller;
    jmethodID mTransform;
    jboolean mRedefineAvailable;
    jboolean mRedefineAdded;
    jboolean mNativeMethodPrefixAvailable;
    jboolean mNativeMethodPrefixAdded;
    jboolean mRetransformAvailable;
    jboolean mRetransformAdded;
};

using JPLISAgent = JPLISAgentStruct;

extern "C" {
JNIEXPORT jobject JNICALL
Java_top_nontage_jvmcontext_JvmContext_getInstrumentationImpl(JNIEnv *env, jclass clazz) {
    JavaVM *vm = nullptr;
    if (env->GetJavaVM(&vm) != JNI_OK) return nullptr;

    jvmtiEnv *jvmti = nullptr;
    vm->GetEnv(reinterpret_cast<void **>(&jvmti), JVMTI_VERSION_1_2);
    if (!jvmti) return nullptr;

    jvmtiCapabilities caps{};
    caps.can_redefine_classes = 1;
    caps.can_retransform_classes = 1;
    caps.can_set_native_method_prefix = 1;
    jvmti->AddCapabilities(&caps);

    auto *agent = static_cast<JPLISAgent *>(std::calloc(1, sizeof(JPLISAgent)));
    if (!agent) return nullptr;

    agent->mJVM = vm;
    agent->mNormalEnvironment.mJVMTIEnv = jvmti;
    agent->mNormalEnvironment.mAgent = agent;
    agent->mNormalEnvironment.mIsRetransformer = JNI_FALSE;

    agent->mRetransformEnvironment.mJVMTIEnv = jvmti;
    agent->mRetransformEnvironment.mAgent = agent;
    agent->mRetransformEnvironment.mIsRetransformer = JNI_TRUE;

    agent->mRedefineAvailable = JNI_TRUE;
    agent->mRetransformAvailable = JNI_TRUE;

    const jclass implClass = env->FindClass("sun/instrument/InstrumentationImpl");
    if (env->ExceptionCheck()) return nullptr;

    jobject inst = nullptr;

    jmethodID ctor = env->GetMethodID(implClass, "<init>", "(JZZZ)V");

    if (!env->ExceptionCheck()) {
        inst = env->NewObject(implClass, ctor, reinterpret_cast<jlong>(agent), JNI_TRUE, JNI_TRUE, JNI_TRUE);
    } else {
        env->ExceptionClear();
        ctor = env->GetMethodID(implClass, "<init>", "(JZZ)V");
        if (!env->ExceptionCheck()) {
            inst = env->NewObject(implClass, ctor, reinterpret_cast<jlong>(agent), JNI_TRUE, JNI_TRUE);
        }
    }

    if (!inst) {
        std::free(agent);
        return nullptr;
    }

    agent->mInstrumentationImpl = env->NewGlobalRef(inst);

    const jclass clsClazz = env->FindClass("java/lang/Class");

    if (const jmethodID getModule = env->GetMethodID(clsClazz, "getModule", "()Ljava/lang/Module;");
        getModule != nullptr && !env->ExceptionCheck()) {
        const jobject instrumentModule = env->CallObjectMethod(implClass, getModule);

        if (const jobject callerModule = env->CallObjectMethod(clazz, getModule); instrumentModule && callerModule) {
#ifdef JVMTI_VERSION_9
            jvmti->AddModuleOpens(instrumentModule, "sun.instrument", callerModule);
#endif
        }
    } else {
        env->ExceptionClear();
    }

    return inst;
}
}
