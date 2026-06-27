#include <jni.h>
#include <string>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#define LIB_NAME "instrument.dll"
#define GET_FUNC GetProcAddress
typedef HMODULE LIBRARY_HANDLE;
#else
#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>
#define LIB_NAME "libinstrument.so"
#define GET_FUNC dlsym
typedef void* LIBRARY_HANDLE;
#endif

typedef jint (JNICALL *Agent_OnAttach_t)(JavaVM *, char *, void *);

static bool debugMode = false;

extern "C" {
    JNIEXPORT void JNICALL
    Java_top_nontage_jvmcontext_JvmContext_setDebugMode(JNIEnv *env, jclass clazz, jboolean debug) {
        debugMode = (debug == JNI_TRUE);
        if (debugMode) {
            std::cout << "[Native] Debug mode enabled" << std::endl;
        }
    }
}

void logMessage(const std::string& msg) {
    if (debugMode) {
        std::cout << msg << std::endl;
    }
}

void logError(const std::string& msg) {
    if (debugMode) {
        std::cerr << msg << std::endl;
    }
}

std::string getCurrentJvmPath() {
    std::string result = "";
#ifdef _WIN32
    char modulePath[MAX_PATH];
    if (GetModuleFileNameA(NULL, modulePath, MAX_PATH)) {
        std::string pathStr = modulePath;
        size_t lastSlash = pathStr.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            result = pathStr.substr(0, lastSlash);
        }
    }
#else
    char exePath[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
    if (len != -1) {
        exePath[len] = '\0';
        std::string pathStr = exePath;
        size_t lastSlash = pathStr.find_last_of('/');
        if (lastSlash != std::string::npos) {
            result = pathStr.substr(0, lastSlash);
        }
    }
#endif
    return result;
}

std::string getJavaHome() {
    const char* javaHome = std::getenv("JAVA_HOME");
    if (javaHome) {
        return std::string(javaHome);
    }
    return "";
}

LIBRARY_HANDLE findInstrumentLibrary() {
    LIBRARY_HANDLE hLib = nullptr;
    std::string jvmPath = getCurrentJvmPath();
    std::string javaHome = getJavaHome();

    std::vector<std::string> searchPaths;

#ifdef _WIN32
    logMessage("[Native] OS: Windows");
    logMessage("[Native] Current JVM path: " + jvmPath);

    if (!jvmPath.empty()) {
        searchPaths.push_back(jvmPath + "\\" LIB_NAME);
        size_t pos = jvmPath.find_last_of("\\/");
        if (pos != std::string::npos) {
            std::string parent = jvmPath.substr(0, pos);
            searchPaths.push_back(parent + "\\jre\\bin\\" LIB_NAME);
            searchPaths.push_back(parent + "\\lib\\" LIB_NAME);
        }
        searchPaths.push_back(jvmPath + "\\..\\jre\\bin\\" LIB_NAME);
        searchPaths.push_back(jvmPath + "\\..\\lib\\" LIB_NAME);
    }

    if (!javaHome.empty() && javaHome != jvmPath) {
        logMessage("[Native] JAVA_HOME: " + javaHome + " (different from current JVM)");
        searchPaths.push_back(javaHome + "\\jre\\bin\\" LIB_NAME);
        searchPaths.push_back(javaHome + "\\bin\\" LIB_NAME);
        searchPaths.push_back(javaHome + "\\lib\\" LIB_NAME);
    } else if (!javaHome.empty()) {
        logMessage("[Native] JAVA_HOME: " + javaHome + " (same as current JVM)");
    } else {
        logMessage("[Native] JAVA_HOME not set");
    }
#else
    logMessage("[Native] OS: Linux");
    logMessage("[Native] Current JVM path: " + jvmPath);

    if (!jvmPath.empty()) {
        searchPaths.push_back(jvmPath + "/" LIB_NAME);
        size_t pos = jvmPath.find_last_of('/');
        if (pos != std::string::npos) {
            std::string parent = jvmPath.substr(0, pos);
            searchPaths.push_back(parent + "/jre/lib/amd64/" LIB_NAME);
            searchPaths.push_back(parent + "/jre/lib/" LIB_NAME);
            searchPaths.push_back(parent + "/lib/" LIB_NAME);
            searchPaths.push_back(parent + "/lib/server/" LIB_NAME);
        }
        searchPaths.push_back(jvmPath + "/../jre/lib/amd64/" LIB_NAME);
        searchPaths.push_back(jvmPath + "/../lib/" LIB_NAME);
    }

    if (!javaHome.empty()) {
        logMessage("[Native] JAVA_HOME: " + javaHome);
        searchPaths.push_back(javaHome + "/jre/lib/amd64/" LIB_NAME);
        searchPaths.push_back(javaHome + "/jre/lib/" LIB_NAME);
        searchPaths.push_back(javaHome + "/lib/" LIB_NAME);
        searchPaths.push_back(javaHome + "/lib/server/" LIB_NAME);
    } else {
        logMessage("[Native] JAVA_HOME not set");
    }

    searchPaths.push_back("/usr/lib/jvm/default-java/lib/" LIB_NAME);
#endif

    logMessage("[Native] Searching for " + std::string(LIB_NAME) + " in " + std::to_string(searchPaths.size()) + " paths...");

    for (size_t i = 0; i < searchPaths.size(); i++) {
        const auto& path = searchPaths[i];
        logMessage("[Native] Trying path " + std::to_string(i + 1) + "/" + std::to_string(searchPaths.size()) + ": " + path);

#ifdef _WIN32
        hLib = LoadLibraryExA(path.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (hLib) {
            logMessage("[Native] SUCCESS: Loaded from: " + path);
            return hLib;
        } else {
            DWORD err = GetLastError();
            logError("[Native] FAILED: " + path + " - WinErr: " + std::to_string(err));
        }
#else
        hLib = dlopen(path.c_str(), RTLD_LAZY);
        if (hLib) {
            logMessage("[Native] SUCCESS: Loaded from: " + path);
            return hLib;
        } else {
            logError("[Native] FAILED: " + path + " - " + std::string(dlerror()));
        }
#endif
    }

#ifdef _WIN32
    logMessage("[Native] Trying with SetDllDirectoryA...");
    if (!jvmPath.empty()) {
        SetDllDirectoryA(jvmPath.c_str());
        hLib = LoadLibraryA(LIB_NAME);
        SetDllDirectoryA(NULL);
        if (hLib) {
            logMessage("[Native] SUCCESS: Loaded with SetDllDirectoryA");
            return hLib;
        } else {
            DWORD err = GetLastError();
            logError("[Native] FAILED: SetDllDirectoryA approach - WinErr: " + std::to_string(err));
        }
    }
#endif

    logMessage("[Native] Trying default system path...");
#ifdef _WIN32
    hLib = LoadLibraryA(LIB_NAME);
    if (hLib) {
        logMessage("[Native] SUCCESS: Loaded from system path");
    } else {
        DWORD err = GetLastError();
        logError("[Native Error] Failed to load from system path. WinErr: " + std::to_string(err));
    }
#else
    hLib = dlopen(LIB_NAME, RTLD_LAZY);
    if (hLib) {
        logMessage("[Native] SUCCESS: Loaded from system path");
    } else {
        logError("[Native Error] Failed to load from system path: " + std::string(dlerror()));
    }
#endif

    return hLib;
}

extern "C" {
    JNIEXPORT jint JNICALL
    Java_top_nontage_jvmcontext_JvmContext_forceLoadAgent(JNIEnv *env, jclass clazz, jstring jarPath) {
        logMessage("[Native] forceLoadAgent called");

        JavaVM *vm = nullptr;
        if (env->GetJavaVM(&vm) != JNI_OK) {
            logError("[Native Error] Failed to get JavaVM");
            return -1;
        }
        logMessage("[Native] JavaVM obtained successfully");

        logMessage("[Native] Searching for instrument library...");
        LIBRARY_HANDLE hLib = findInstrumentLibrary();
        if (!hLib) {
            logError("[Native Error] Instrument library not found in any path!");
            return -2;
        }
        logMessage("[Native] Instrument library loaded successfully");

        logMessage("[Native] Looking for Agent_OnAttach function...");
        const auto pAgentOnAttach = reinterpret_cast<Agent_OnAttach_t>(GET_FUNC(hLib, "Agent_OnAttach"));
        if (!pAgentOnAttach) {
            logError("[Native Error] Agent_OnAttach function not found!");
#ifdef _WIN32
            logError("[Native Error] GetLastError: " + std::to_string(GetLastError()));
#else
            logError("[Native Error] dlerror: " + std::string(dlerror()));
#endif
            return -3;
        }

        char addr[32];
        snprintf(addr, sizeof(addr), "%p", (void*)pAgentOnAttach);
        logMessage("[Native] Agent_OnAttach found at: " + std::string(addr));

        const char* path = env->GetStringUTFChars(jarPath, nullptr);
        if (!path) {
            logError("[Native Error] Failed to get JAR path string");
            return -4;
        }
        logMessage("[Native] Agent JAR path: " + std::string(path));

        logMessage("[Native] Calling Agent_OnAttach...");
        const jint result = pAgentOnAttach(vm, const_cast<char *>(path), nullptr);
        logMessage("[Native] Agent_OnAttach returned: " + std::to_string(result));

        env->ReleaseStringUTFChars(jarPath, path);

        if (result != 0) {
            logError("[Native Error] Agent_OnAttach failed with code: " + std::to_string(result));
        } else {
            logMessage("[Native] Agent attached successfully!");
        }

        return result;
    }
}