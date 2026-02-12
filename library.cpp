#include <jni.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
#define LIB_NAME "instrument.dll"
#define GET_FUNC GetProcAddress
typedef HMODULE LIBRARY_HANDLE;
#else
#include <dlfcn.h>
#define LIB_NAME "libinstrument.so"
#define GET_FUNC dlsym
typedef void* LIBRARY_HANDLE;
#endif

typedef jint (JNICALL *Agent_OnAttach_t)(JavaVM *, char *, void *);

extern "C" {
    JNIEXPORT jint JNICALL
    Java_top_nontage_jvmcontext_JvmContext_forceLoadAgent(JNIEnv *env, jclass clazz, jstring jarPath) {
        JavaVM *vm = nullptr;
        if (env->GetJavaVM(&vm) != JNI_OK) return -1;

        LIBRARY_HANDLE hLib = nullptr;

#ifdef _WIN32
        hLib = GetModuleHandleA(LIB_NAME);
        if (!hLib) hLib = LoadLibraryA(LIB_NAME);
#else
        hLib = dlopen(LIB_NAME, RTLD_LAZY);
#endif

        if (!hLib) return -2;

        const auto pAgentOnAttach = reinterpret_cast<Agent_OnAttach_t>(GET_FUNC(hLib, "Agent_OnAttach"));
        if (!pAgentOnAttach) return -3;

        const char* path = env->GetStringUTFChars(jarPath, nullptr);

        const jint result = pAgentOnAttach(vm, const_cast<char *>(path), nullptr);

        env->ReleaseStringUTFChars(jarPath, path);
        return result;
    }
}