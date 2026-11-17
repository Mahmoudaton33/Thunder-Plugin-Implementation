// Include the class declaration (HelloWorld.h). It in turn includes Module.h
// and with it the whole Thunder API (Core, PluginHost, JSON, logging...).
// Every member function defined in this file must match a declaration there.
#include "HelloWorld.h"

// Open Thunder's top-level namespace. Everything in Thunder (Core,
// PluginHost, Plugin, Logging...) lives in it, so inside it we can write
// Plugin::Metadata instead of WPEFramework::Plugin::Metadata.
namespace WPEFramework {

// Anonymous namespace: anything declared here is visible only inside this
// .cpp file (internal linkage). This prevents the symbol "metadata" from
// clashing with an identically named object in another file or plugin.
namespace {
    // Tells Thunder the plugin version and which subsystems it needs.
    //
    // A global object of type Plugin::Metadata<Plugin::HelloWorld>
    // (plugins/MetaData.h). Because it is global, its constructor runs when
    // Thunder dlopen()s the .so, before any plugin code is called.
    // The constructor:
    //   1. packs the version as (major << 16) | (minor << 8) | patch,
    //   2. sets the service name from typeid(HelloWorld), which gives
    //      "HelloWorld". This must match "classname" in HelloWorld.json,
    //   3. stores the three subsystem lists below,
    //   4. sets Core::System::ROOT_META_DATA = this (the slot created by
    //      MODULE_NAME_DECLARATION in Module.cpp), so Thunder can read the
    //      metadata through ModuleServiceMetadata(),
    //   5. registers a factory with Core::ServiceAdministrator that can
    //      create HelloWorld objects by name.
    // Its destructor (at dlclose) unregisters the factory and clears
    // ROOT_META_DATA. "static" is redundant inside an anonymous namespace
    // but harmless.
    // The template argument says which class this metadata describes and
    // creates.
    static Plugin::Metadata<Plugin::HelloWorld> metadata(
        // Version (Major, Minor, Patch)
        // 1.0.0: shown in Controller status. The major number is also the
        // ".1." in JSON-RPC method names, e.g. HelloWorld.1.sayHello.
        1, 0, 0,
        // Preconditions: subsystems that must be up before activation
        // Empty list. Examples you could add: subsystem::PLATFORM,
        // subsystem::NETWORK. Note that HelloWorld.json still adds
        // precondition ["Platform"] from HelloWorld.conf.in.
        {},
        // Terminations: subsystems that deactivate the plugin when they go down
        // Empty list: nothing forces this plugin down.
        {},
        // Controls: subsystems this plugin reports as up
        // Empty list: this plugin is not responsible for any subsystem.
        {}
    // End of the constructor call and of the object declaration.
    );
// Close the anonymous namespace.
}

// Open WPEFramework::Plugin, where Thunder plugins conventionally live. It
// must match the namespace used in HelloWorld.h, or the definitions below
// would not match the declarations.
namespace Plugin {

    // Makes the class loadable by name ("HelloWorld" in HelloWorld.json).
    //
    // Expands (core/Services.h) to:
    //   static Core::ServiceMetadata<HelloWorld> ServiceMetadata_HelloWorld(1, 0, 0);
    // a second global whose constructor also registers a HelloWorld factory
    // with Core::ServiceAdministrator.
    // NOTE: this is the older (Thunder R2) registration style. On Thunder
    // 4.2 the Plugin::Metadata object above already registers the class, so
    // HelloWorld is registered twice. That is harmless:
    // ServiceAdministrator::Instantiate() uses the first matching entry,
    // which is the Metadata object, because globals in one file are
    // constructed in the order they appear. This line is redundant and
    // could be removed.
    SERVICE_REGISTRATION(HelloWorld, 1, 0, 0);

    // Default constructor (declared in HelloWorld.h). The factory registered
    // above calls it when Thunder creates the plugin. Only do cheap,
    // failure-free setup here; real work goes in Initialize(), which can
    // report errors.
    HelloWorld::HelloWorld()
        // Construct the JSONRPC base class. It holds the method table
        // (name -> handler), the event-subscriber table, and the built-in
        // "register"/"unregister"/"exists" methods. IPlugin is a pure
        // interface with no state, so it needs no initialiser.
        : PluginHost::JSONRPC()
        // No IShell yet. A null pointer means "not initialised";
        // Initialize() asserts this.
        , _service(nullptr)
        // Default-construct the Core::CriticalSection (recursive mutex) that
        // protects _greeting and _count against concurrent JSON-RPC calls
        // from Thunder's worker threads.
        , _adminLock()
        // Empty greeting; the real value is read from the config in
        // Initialize().
        , _greeting()
        // Number of sayHello calls served so far.
        , _count(0)
        // These initialisers are listed in the same order as the members are
        // declared in the header. C++ always initialises in declaration
        // order, so matching it avoids -Wreorder warnings.
    // Constructor body: empty, because everything is done in the
    // initialiser list.
    {
    // End of the constructor.
    }

    // Destructor generated by the compiler (= default). It destroys
    // _greeting, _adminLock and the JSONRPC base. Nothing needs manual
    // release here, because Deinitialize() has already released _service.
    // It is declared "override" in the header because IPlugin has a virtual
    // destructor.
    HelloWorld::~HelloWorld() = default;

    // IPlugin::Initialize. Thunder calls this once when it activates the
    // plugin: at boot (autostart=true) or on Controller.1.activate.
    // "service" is the plugin's shell, its link back to the framework
    // (config, callsign, paths, subsystems, other plugins...).
    // Return value: an empty string means success; any text is an error
    // message, and the plugin stays deactivated.
    const string HelloWorld::Initialize(PluginHost::IShell* service)
    // Start of the function body.
    {
        // Debug-only check (ASSERT compiles to nothing in release builds)
        // that the framework passed a valid shell pointer.
        ASSERT(service != nullptr);
        // Debug-only check that Initialize() has not already run without a
        // matching Deinitialize().
        ASSERT(_service == nullptr);

        // Keep the shell pointer so other methods (e.g. Deinitialize) can
        // use it later.
        _service = service;
        // IShell is reference-counted. Because we keep the pointer beyond
        // this call, we take our own reference so the shell stays alive.
        // The matching Release() is in Deinitialize().
        _service->AddRef();

        // Create the Config helper (HelloWorld.h): a Core::JSON::Container
        // with one field, Greeting, bound to the JSON key "greeting" and
        // defaulting to "Hello".
        Config config;
        // ConfigLine() returns the "configuration" object from
        // /etc/WPEFramework/plugins/HelloWorld.json as JSON text, e.g.
        // {"greeting":"Hello"}. FromString() parses it into the container.
        // Unknown keys are ignored. A missing "greeting" keeps the default.
        config.FromString(_service->ConfigLine());
        // Copy the parsed value into the member, because "config" is a local
        // that is destroyed when this function returns. No lock is needed
        // yet: the JSON-RPC methods are not registered, so no other thread
        // can touch _greeting.
        _greeting = config.Greeting.Value();

        // Register the JSON-RPC method "sayHello" in the JSONRPC base's
        // dispatch table. <JsonObject, JsonObject> are the input/output
        // types: Thunder parses "params" into a JsonObject, calls
        // this->SayHello(), then serialises the response JsonObject as
        // "result". _T() is Thunder's string-literal macro (plain "..." on
        // Linux). Clients call it as HelloWorld.1.sayHello.
        Register<JsonObject, JsonObject>(_T("sayHello"), &HelloWorld::SayHello, this);
        // Register "getGreeting" -> HelloWorld::GetGreeting (same mechanism).
        Register<JsonObject, JsonObject>(_T("getGreeting"), &HelloWorld::GetGreeting, this);
        // Register "setGreeting" -> HelloWorld::SetGreeting (same mechanism).
        // Registering here rather than in the constructor means the methods
        // are reachable only while the plugin is active and configured.
        Register<JsonObject, JsonObject>(_T("setGreeting"), &HelloWorld::SetGreeting, this);

        // Write a log line in the "Startup" category, which can be enabled or
        // disabled at runtime. The second argument is in its own parentheses
        // because the macro pastes it after a constructor:
        //   Logging::Startup __data__ (format, args...);
        // This is how a printf-style argument list passes through a macro.
        // c_str() gives the const char* that %s expects.
        SYSLOG(Logging::Startup, (_T("HelloWorld: initialized with greeting '%s'"), _greeting.c_str()));

        // An empty string means success. Any text is an error and Thunder
        // will not activate the plugin.
        // Return an empty string: success. Thunder marks the plugin
        // Activated and starts routing JSON-RPC calls to it.
        return string();
    // End of Initialize().
    }

    // IPlugin::Deinitialize. Thunder calls this on deactivation
    // (Controller.1.deactivate, a termination subsystem going down, or
    // framework shutdown). It must undo everything Initialize() did, in
    // reverse order.
    void HelloWorld::Deinitialize(PluginHost::IShell* service)
    // Start of the function body.
    {
        // Debug-only check that the shell being torn down is the one we
        // received in Initialize().
        ASSERT(_service == service);

        // Remove "sayHello" from the dispatch table, so new requests get
        // "Unknown method". This is done first so no new calls arrive while
        // the rest of the plugin is torn down.
        Unregister(_T("sayHello"));
        // Remove "getGreeting".
        Unregister(_T("getGreeting"));
        // Remove "setGreeting".
        Unregister(_T("setGreeting"));

        // Give back the shell reference taken by AddRef() in Initialize().
        // Without it the shell would leak.
        _service->Release();
        // Clear the pointer so it can't be used after release. This also
        // resets the plugin so the Initialize() assert passes if it is
        // activated again.
        _service = nullptr;

        // Log the shutdown in the "Shutdown" category. There are no format
        // arguments, but the inner parentheses are still required by the
        // macro.
        SYSLOG(Logging::Shutdown, (_T("HelloWorld: deinitialized")));
    // End of Deinitialize().
    }

    // IPlugin::Information: the last pure-virtual method of IPlugin. The
    // framework can call it for a free-form, plugin-specific status string.
    // It is const because it must not change the plugin. It must be defined,
    // even if it returns nothing, or HelloWorld would be abstract and could
    // not be created.
    string HelloWorld::Information() const
    // Start of the function body.
    {
        // Not used for JSON-RPC; may return an empty string.
        // Return no extra information.
        return string();
    // End of Information().
    }

    // HelloWorld.1.sayHello {"name":"Mahmoud"}  ->  {"message":"Hello, Mahmoud!","count":1}
    //
    // JSON-RPC handler registered as "sayHello". Thunder calls it on one of
    // its worker threads, so several calls can run at the same time.
    // "parameters": the request's parsed "params" (read-only).
    // "response": an empty object we fill in; Thunder sends it as "result".
    // Return value: a Thunder error code. Core::ERROR_NONE (0) means
    // success; anything else becomes a JSON-RPC error.
    // JsonObject is Core::JSON::VariantContainer: a dynamic JSON object.
    uint32_t HelloWorld::SayHello(const JsonObject& parameters, JsonObject& response)
    // Start of the function body.
    {
        // Validate the input: does the request contain a "name" key?
        // (Thunder style compares with "== false" explicitly instead of
        // using "!".)
        if (parameters.HasLabel(_T("name")) == false) {
            // Put "success": false in the response. NOTE: when an error code
            // is returned, Thunder 4.2 throws away the response object and
            // sends an "error" object instead (code 30, "ERROR_BAD_REQUEST"),
            // so the client never sees this field.
            response[_T("success")] = false;
            // Reject the request with error code 30 (bad request).
            return Core::ERROR_BAD_REQUEST;
        // End of the validation block.
        }

        // Read the "name" value as a string. operator[] returns the stored
        // JSON variant and String() gives its text. This is safe because
        // HasLabel() confirmed the key exists. A const local copy is kept
        // because the value is used both in the message and in the event.
        const string name = parameters[_T("name")].String();

        // Lock the mutex: another thread may be running sayHello or
        // setGreeting at the same moment. Without the lock, reading
        // _greeting or incrementing _count would be a data race.
        _adminLock.Lock();
        // Build the reply text, e.g. "Hello" + ", " + "Mahmoud" + "!"
        // gives "Hello, Mahmoud!". _greeting is read while the lock is held,
        // so a concurrent setGreeting can't change it halfway through.
        const string message = _greeting + _T(", ") + name + _T("!");
        // Increment the shared counter and copy the new value into a local,
        // still under the lock. The local copy is used after unlocking, so
        // it must report this call's number, not one another thread has
        // changed since.
        const uint32_t count = ++_count;
        // Unlock as early as possible to keep the critical section short.
        // (Core::SafeSyncType<Core::CriticalSection> is an RAII alternative
        // that unlocks automatically, even if an exception is thrown.)
        _adminLock.Unlock();

        // Add "message": "Hello, Mahmoud!" to the result. operator[] on a
        // non-const JsonObject creates the key if it doesn't exist.
        response[_T("message")] = message;
        // Add "count": N (stored as a JSON number).
        response[_T("count")] = count;
        // Add "success": true (the usual RDK-services convention).
        response[_T("success")] = true;

        // Event to every client that registered for "onGreeted".
        // Create an empty JSON object for the event payload.
        JsonObject event;
        // Put the greeted name in the payload.
        event[_T("name")] = name;
        // Put this call's counter value (the local copy) in the payload.
        event[_T("count")] = count;
        // Send the JSON-RPC event "onGreeted" to every subscribed client
        // (inherited from PluginHost::JSONRPC). A client subscribes with
        //   HelloWorld.1.register {"event":"onGreeted","id":"myclient"}
        // and then receives method "myclient.onGreeted" with this payload.
        // This is called after Unlock() on purpose: sending can be slow and
        // takes the dispatcher's own lock, so holding _adminLock here would
        // block other callers and risk deadlock.
        Notify(_T("onGreeted"), event);

        // Success. Thunder replies with the "result" object built above.
        return Core::ERROR_NONE;
    // End of SayHello().
    }

    // HelloWorld.1.getGreeting  ->  {"greeting":"Hello"}
    //
    // JSON-RPC handler registered as "getGreeting". It needs the same
    // signature as the others (registered as <JsonObject, JsonObject>) but
    // takes no input. VARIABLE_IS_NOT_USED expands to
    // __attribute__((unused)), which suppresses the "unused parameter"
    // warning.
    uint32_t HelloWorld::GetGreeting(const JsonObject& parameters VARIABLE_IS_NOT_USED, JsonObject& response)
    // Start of the function body.
    {
        // Lock: setGreeting may be changing _greeting on another thread.
        _adminLock.Lock();
        // Copy the current greeting into the result while the lock is held,
        // so the copy is consistent.
        response[_T("greeting")] = _greeting;
        // Release the mutex.
        _adminLock.Unlock();
        // Add the success flag. It touches no shared data, so it is outside
        // the lock.
        response[_T("success")] = true;
        // Success. The reply is {"greeting":"Hello","success":true}.
        return Core::ERROR_NONE;
    // End of GetGreeting().
    }

    // HelloWorld.1.setGreeting {"greeting":"Ahlan"}  ->  {"success":true}
    //
    // JSON-RPC handler registered as "setGreeting". Changes the greeting
    // word at runtime.
    uint32_t HelloWorld::SetGreeting(const JsonObject& parameters, JsonObject& response)
    // Start of the function body.
    {
        // Validation: reject the request if the "greeting" key is missing OR
        // its value is an empty string (which would give ", Mahmoud!").
        // "||" short-circuits, so operator[] is evaluated only if the key
        // exists.
        if ((parameters.HasLabel(_T("greeting")) == false) || parameters[_T("greeting")].String().empty()) {
            // Same as in SayHello: the client never sees this field, because
            // the error return replaces the result.
            response[_T("success")] = false;
            // Reject the request with error code 30 (bad request).
            return Core::ERROR_BAD_REQUEST;
        // End of the validation block.
        }

        // Lock before writing the shared _greeting, so readers (SayHello,
        // GetGreeting) never see a half-written string.
        _adminLock.Lock();
        // Store the new greeting; from now on sayHello uses it (e.g.
        // "Ahlan, Mahmoud!"). This is in memory only. It is not written back
        // to HelloWorld.json, so after a plugin restart the greeting returns
        // to the configured value.
        _greeting = parameters[_T("greeting")].String();
        // Release the mutex.
        _adminLock.Unlock();

        // Build the result {"success":true}.
        response[_T("success")] = true;
        // Success.
        return Core::ERROR_NONE;
    // End of SetGreeting().
    }

// Close namespace Plugin.
} // namespace Plugin
// Close namespace WPEFramework.
} // namespace WPEFramework
