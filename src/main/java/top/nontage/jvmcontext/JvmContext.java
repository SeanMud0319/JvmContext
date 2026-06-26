package top.nontage.jvmcontext;

import java.lang.instrument.Instrumentation;
import java.io.*;
import java.nio.file.Files;
import java.util.jar.*;

public class JvmContext {
    private static Instrumentation instrumentation;
    private static boolean isNativeLoaded = false;
    private static final String PROP_KEY = "top.nontage.jvmcontext.inst";

    public static Instrumentation getInstrumentation() {
        if (instrumentation != null) return instrumentation;

        instrumentation = (Instrumentation) System.getProperties().get(PROP_KEY);
        if (instrumentation != null) {
            System.getProperties().remove(PROP_KEY);
            return instrumentation;
        }

        synchronized (JvmContext.class) {
            instrumentation = (Instrumentation) System.getProperties().get(PROP_KEY);
            if (instrumentation != null) {
                System.getProperties().remove(PROP_KEY);
                return instrumentation;
            }

            try {
                if (!isNativeLoaded) {
                    loadNativeLibrary();
                }

                File agentJar = createTemporaryAgentJar();
                try {
                    int result = forceLoadAgent(agentJar.getAbsolutePath());
                    if (result != 0) throw new RuntimeException("Native forceLoadAgent failed: " + result);

                    long start = System.currentTimeMillis();
                    while (System.currentTimeMillis() - start < 5000) {
                        instrumentation = (Instrumentation) System.getProperties().remove(PROP_KEY);
                        if (instrumentation != null) {
                            return instrumentation;
                        }
                        Thread.sleep(50);
                    }
                    throw new RuntimeException("Timeout waiting for Agent initialization.");
                } finally {
                    if (agentJar.exists()) {
                        agentJar.delete();
                    }
                }
            } catch (Exception e) {
                throw new RuntimeException("Failed to force load instrumentation", e);
            }
        }
    }

    private static File createTemporaryAgentJar() throws IOException {
        File tempJar = File.createTempFile("jvm_agent_proxy", ".jar");

        Manifest manifest = new Manifest();
        Attributes attrs = manifest.getMainAttributes();
        attrs.put(Attributes.Name.MANIFEST_VERSION, "1.0");
        attrs.put(new Attributes.Name("Agent-Class"), AgentProxy.class.getName());
        attrs.put(new Attributes.Name("Can-Retransform-Classes"), "true");
        attrs.put(new Attributes.Name("Can-Redefine-Classes"), "true");

        try (FileOutputStream fos = new FileOutputStream(tempJar);
             JarOutputStream jos = new JarOutputStream(fos, manifest)) {

            String className = AgentProxy.class.getName().replace('.', '/') + ".class";
            jos.putNextEntry(new JarEntry(className));

            try (InputStream is = JvmContext.class.getClassLoader().getResourceAsStream(className)) {
                if (is == null) throw new IOException("Cannot find bytecode for " + className);
                byte[] buffer = new byte[8192];
                int n;
                while ((n = is.read(buffer)) != -1) {
                    jos.write(buffer, 0, n);
                }
            }
            jos.closeEntry();
            jos.finish();
            fos.getFD().sync();
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
        File tempDir = new File(System.getProperty("java.io.tmpdir"));
        File[] oldLibs = tempDir.listFiles((dir, name) -> name.startsWith("jvm_ctx_native_") && name.endsWith(".dll"));
        if (oldLibs != null) {
            for (File old : oldLibs) {
                // noinspection ResultOfMethodCallIgnored
                old.delete();
            }
        }
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