package top.nontage.jvmcontext;

import java.lang.instrument.Instrumentation;
import java.io.*;
import java.nio.file.Files;
import java.util.jar.*;

public class JvmContext {
    private static Instrumentation instrumentation;
    private static boolean isNativeLoaded = false;
    private static final String PROP_KEY = "top.nontage.jvmcontext.inst";

    static {
        loadNativeLibrary();
    }

    public static Instrumentation getInstrumentation() {
        if (instrumentation != null) return instrumentation;

        instrumentation = (Instrumentation) System.getProperties().get(PROP_KEY);
        if (instrumentation != null) return instrumentation;

        synchronized (JvmContext.class) {
            instrumentation = (Instrumentation) System.getProperties().get(PROP_KEY);
            if (instrumentation != null) return instrumentation;

            try {
                File currentJar = new File(JvmContext.class.getProtectionDomain()
                        .getCodeSource()
                        .getLocation()
                        .toURI());

                File agentJar = createTemporaryAgentJar(currentJar);

                int result = forceLoadAgent(agentJar.getAbsolutePath());
                if (result != 0) throw new RuntimeException("Native forceLoadAgent failed: " + result);

                long start = System.currentTimeMillis();
                while (System.currentTimeMillis() - start < 5000) {
                    instrumentation = (Instrumentation) System.getProperties().remove(PROP_KEY);
                    if (instrumentation != null) {
                        try {
                            agentJar.delete();
                        } catch (Exception ignored) {}
                        return instrumentation;
                    }
                    Thread.sleep(50);
                }
                throw new RuntimeException("Timeout waiting for Agent initialization.");
            } catch (Exception e) {
                throw new RuntimeException("Failed to force load instrumentation", e);
            }
        }
    }

    private static File createTemporaryAgentJar(File currentJar) throws IOException {
        File tempJar = File.createTempFile("jvm_agent_proxy", ".jar");

        Manifest manifest = new Manifest();
        Attributes attrs = manifest.getMainAttributes();
        attrs.put(Attributes.Name.MANIFEST_VERSION, "1.0");
        attrs.put(new Attributes.Name("Agent-Class"), AgentProxy.class.getName());
        attrs.put(new Attributes.Name("Can-Retransform-Classes"), "true");
        attrs.put(new Attributes.Name("Can-Redefine-Classes"), "true");
        attrs.put(new Attributes.Name("Boot-Class-Path"), currentJar.getAbsolutePath().replace("\\", "/"));

        try (JarOutputStream jos = new JarOutputStream(new FileOutputStream(tempJar), manifest)) {
            jos.putNextEntry(new JarEntry("META-INF/"));
            jos.closeEntry();
        }
        return tempJar;
    }

    public static class AgentProxy {
        public static void agentmain(String args, Instrumentation inst) {
            System.getProperties().put(PROP_KEY, inst);
        }
    }

    private native static int forceLoadAgent(String jarPath);

    private static synchronized void loadNativeLibrary() {
        if (isNativeLoaded) return;
        String os = System.getProperty("os.name").toLowerCase();
        String ext = os.contains("win") ? ".dll" : (os.contains("mac") ? ".dylib" : ".so");
        String folder = os.contains("win") ? "win32-x86-64" : (os.contains("mac") ? "darwin-x86-64" : "linux-x86-64");
        String resourcePath = "/" + folder + "/libJvmContext" + ext;

        try (InputStream in = JvmContext.class.getResourceAsStream(resourcePath)) {
            if (in == null) {
                isNativeLoaded = true;
                return;
            }
            File tempLib = File.createTempFile("jvm_ctx_native_", ext);
            tempLib.deleteOnExit();
            Files.copy(in, tempLib.toPath(), java.nio.file.StandardCopyOption.REPLACE_EXISTING);
            System.load(tempLib.getAbsolutePath());
            isNativeLoaded = true;
        } catch (Exception e) {
            if (!isNativeLoaded) throw new RuntimeException("Native library init failed: " + e.getMessage());
        }
    }
}