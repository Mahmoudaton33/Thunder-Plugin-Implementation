// Include the module header. It defines MODULE_NAME (Plugin_HelloWorld, the
// fallback in Module.h, because CMake does not pass -DMODULE_NAME) and pulls
// in the whole Thunder plugin API through <plugins/plugins.h>, including the
// MODULE_NAME_DECLARATION macro used below. Every .cpp in a Thunder module
// must include Module.h first so that all files agree on the module name.
#include "Module.h"

// Give this shared library (.so) its identity as a Thunder module.
// The macro (Thunder 4.2, core/SystemInfo.h) expands into an extern "C"
// block inside WPEFramework::Core::System that defines:
//   - ROOT_META_DATA = nullptr : a slot that the Plugin::Metadata object in
//     HelloWorld.cpp fills with "this" when the library is loaded.
//   - MODULE_NAME = "Plugin_HelloWorld" : the module name as a C string,
//     used by logging, tracing and ServiceMetadata::Module().
//   - ModuleBuildRef() : returns the build identifier. BUILD_REFERENCE is
//     not set by this plugin's CMake, so the Thunder default
//     "engineering_build_for_debug_purpose_only" is returned.
//   - ModuleServiceMetadata() : returns ROOT_META_DATA. After dlopen()ing
//     the .so, Thunder calls it to read the plugin version and its
//     precondition/termination/control subsystems.
// extern "C" turns off C++ name mangling, so the framework can find these
// symbols in the loaded library under fixed names.
// There is no ';' after the macro because its expansion ends with '}'.
// Without this line the library would fail to link or load as a plugin.
MODULE_NAME_DECLARATION(BUILD_REFERENCE)



