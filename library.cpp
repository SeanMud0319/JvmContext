#include <jni.h>
#include <string>
#include <iostream>

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

        if (!hLib) {
            char javaPath[MAX_PATH];
            if (GetModuleFileNameA(NULL, javaPath, MAX_PATH)) {
                std::string pathStr = javaPath;
                size_t lastSlash = pathStr.find_last_of("\\/");
                if (lastSlash != std::string::npos) {
                    std::string fullPath = pathStr.substr(0, lastSlash + 1) + LIB_NAME;
                    hLib = LoadLibraryA(fullPath.c_str());

                    if (!hLib) {
                        size_t secondLast = pathStr.substr(0, lastSlash).find_last_of("\\/");
                        std::string jrePath = pathStr.substr(0, secondLast + 1) + "jre\\bin\\" + LIB_NAME;
                        hLib = LoadLibraryA(jrePath.c_str());
                    }
                }
            }
        }

        if (!hLib) {
            DWORD err = GetLastError();
            std::cout << "[Native Error] Failed to load instrument.dll. WinErr: " << err << std::endl;
            return -2;
        }
#else
        hLib = dlopen(LIB_NAME, RTLD_LAZY);
        if (!hLib) {
            std::cerr << "[Native Error] dlopen failed: " << dlerror() << std::endl;
            return -2;
        }
#endif

        const auto pAgentOnAttach = reinterpret_cast<Agent_OnAttach_t>(GET_FUNC(hLib, "Agent_OnAttach"));
        if (!pAgentOnAttach) {
            return -3;
        }

        const char* path = env->GetStringUTFChars(jarPath, nullptr);
        const jint result = pAgentOnAttach(vm, const_cast<char *>(path), nullptr);
        env->ReleaseStringUTFChars(jarPath, path);

        return result;
    }
}