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

    JPLISAgent *agent = nullptr;
    if (jvmti->Allocate(sizeof(JPLISAgent), reinterpret_cast<unsigned char **>(&agent)) != JVMTI_ERROR_NONE) return nullptr;
    std::memset(agent, 0, sizeof(JPLISAgent));

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
    if (env->ExceptionCheck()) {
        jvmti->Deallocate(reinterpret_cast<unsigned char *>(agent));
        return nullptr;
    }

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
        jvmti->Deallocate(reinterpret_cast<unsigned char *>(agent));
        return nullptr;
    }

    agent->mInstrumentationImpl = env->NewGlobalRef(inst);

    const jclass clsClazz = env->FindClass("java/lang/Class");
    if (clsClazz != nullptr && !env->ExceptionCheck()) {
        jmethodID getModule = env->GetMethodID(clsClazz, "getModule", "()Ljava/lang/Module;");
        if (getModule != nullptr && !env->ExceptionCheck()) {
            jobject instrumentModule = env->CallObjectMethod(inst, getModule);
            jobject callerModule = env->CallObjectMethod(clazz, getModule);

            if (instrumentModule && callerModule && !env->ExceptionCheck()) {
#ifdef JVMTI_VERSION_9
                jvmti->AddModuleOpens(instrumentModule, "sun.instrument", callerModule);
#endif
            }
        }
    }

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }

    return inst;
}
}