package top.nontage.jvmcontext.test;

import top.nontage.jvmcontext.JvmContext;
import org.junit.jupiter.api.Test;
import java.lang.instrument.Instrumentation;

import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

public class JvmContextTest {

    @Test
    public void testGetInstrumentation() {
        Instrumentation inst = JvmContext.getInstrumentation();
        assertNotNull(inst, "Instrumentation instance should not be null!");
        int loadedClassesCount = inst.getAllLoadedClasses().length;
        assertTrue(loadedClassesCount > 0, "Should have loaded classes in the JVM");
        System.out.println("Test passed! Loaded classes count: " + loadedClassesCount);
    }
}