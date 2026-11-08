# The demo preview's native code calls back into nothing; keep the JNI entry points' class.
-keep class org.pixelcade.arcadejacket.PreviewEngine { native <methods>; }
