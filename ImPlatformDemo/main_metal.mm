// The demo of the Metal configurations (Xcode): ImPlatform.h includes ImPlatform_gfx_metal.mm into the demo, which needs
// Objective-C++ with ARC. Compiling main.cpp through this file gives it that from the extension, while Dear ImGui's C++
// backends stay C++ (under ARC, imgui_impl_glfw.cpp's casts of Cocoa objects do not compile).
#include "main.cpp"
