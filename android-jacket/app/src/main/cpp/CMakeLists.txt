# Demo mode's preview: the jacket's looks code, shared with the iPhone app.
cmake_minimum_required(VERSION 3.22)
project(jacketpreview CXX)

set(IOS ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../ios-jacket)
add_library(jacketpreview SHARED preview_jni.cpp ${IOS}/JacketRemote/JacketPreview.cpp)
target_include_directories(jacketpreview PRIVATE ${IOS}/PreviewStubs ${IOS}/JacketRemote)
# 16 KB memory pages (Android 15+ devices; required by Google Play).
target_link_options(jacketpreview PRIVATE "-Wl,-z,max-page-size=16384")
