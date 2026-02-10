package top.nontage.jvmcontext;

import java.lang.instrument.Instrumentation;
import java.io.*;

public class JvmContext {
    private static Instrumentation instrumentation;

    static {
        loadNativeLibrary();
    }

    private static void loadNativeLibrary() {
        String os = System.getProperty("os.name").toLowerCase();
        String arch = System.getProperty("os.arch").toLowerCase();
        String folder;
        String extension;

        if (os.contains("win")) {
            folder = "win32-x86-64";
            extension = ".dll";
        } else if (os.contains("mac")) {
            folder = "darwin-x86-64";
            extension = ".dylib";
        } else {
            folder = "linux-x86-64";
            extension = ".so";
        }

        String libName = "libJvmContext" + extension;
        String resourcePath = "/" + folder + "/" + libName;

        try {
            InputStream in = JvmContext.class.getResourceAsStream(resourcePath);
            if (in == null) throw new FileNotFoundException("Library not found: " + resourcePath);

            File tempLib = File.createTempFile("jvm_ctx_", extension);
            tempLib.deleteOnExit();

            try (FileOutputStream out = new FileOutputStream(tempLib)) {
                byte[] buf = new byte[8192];
                int n;
                while ((n = in.read(buf)) != -1) out.write(buf, 0, n);
            }
            System.load(tempLib.getAbsolutePath());
        } catch (Exception e) {
            throw new RuntimeException("Native library init failed: " + e.getMessage());
        }
    }

    private native static Instrumentation getInstrumentationImpl();

    public static Instrumentation getInstrumentation() {
        if (instrumentation == null) {
            instrumentation = getInstrumentationImpl();
        }
        return instrumentation;
    }
}