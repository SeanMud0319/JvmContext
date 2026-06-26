# JvmContext

A lightweight utility that provides access to the JVM's `Instrumentation` instance without manual agent configuration.


## Installation
You need to compile it first.

### Build from Source

```bash
git clone https://github.com/SeanMud0319/JvmContext.git
cd JvmContext
mvn clean install
```

### Gradle

```gradle
repositories {
    mavenLocal()
}

dependencies {
    implementation("top.nontage:jvm-context:1.0.0")
}
```

### Maven

```xml
<dependency>
    <groupId>top.nontage</groupId>
    <artifactId>jvm-context</artifactId>
    <version>1.0.0</version>
</dependency>
```


## Quick Start

```java
Instrumentation inst = JvmContext.getInstrumentation();
```


## How It Works

`JvmContext` obtains the `Instrumentation` instance by using native JVMTI to dynamically attach an agent to the running JVM.

1. Native code locates the JVM's internal `instrument` library (`instrument.dll` / `libinstrument.so`)
2. Calls `Agent_OnAttach` to load a temporary agent JAR
3. The agent stores the `Instrumentation` instance in a system property
4. `JvmContext` retrieves and returns it


## Example

```java
import top.nontage.jvmcontext.JvmContext;
import java.lang.instrument.Instrumentation;

public class Example {
    public static void main(String[] args) {
        Instrumentation inst = JvmContext.getInstrumentation();
        
        System.out.println("Loaded classes: " + inst.getAllLoadedClasses().length);
        System.out.println("Object size: " + inst.getObjectSize(new Object()));
    }
}
```


## Requirements

- Java 8 or higher
- Windows / Linux (x86-64)
- How about macOS? (I don't have Mac, so I can't compile for Mac)


## License

Apache License 2.0