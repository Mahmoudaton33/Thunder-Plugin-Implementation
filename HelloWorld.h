#pragma once

#include "Module.h"

namespace WPEFramework {
namespace Plugin {

    class HelloWorld : public PluginHost::IPlugin, public PluginHost::JSONRPC {
    private:
        // Reads the "configuration" object of HelloWorld.json.
        class Config : public Core::JSON::Container {
        public:
            Config(const Config&) = delete;
            Config& operator=(const Config&) = delete;

            Config()
                : Core::JSON::Container()
                , Greeting(_T("Hello"))
            {
                Add(_T("greeting"), &Greeting);
            }
            ~Config() override = default;

        public:
            Core::JSON::String Greeting;
        };

    public:
        HelloWorld(const HelloWorld&) = delete;
        HelloWorld& operator=(const HelloWorld&) = delete;

        HelloWorld();
        ~HelloWorld() override;

        BEGIN_INTERFACE_MAP(HelloWorld)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
        END_INTERFACE_MAP

        // PluginHost::IPlugin
        const string Initialize(PluginHost::IShell* service) override;
        void Deinitialize(PluginHost::IShell* service) override;
        string Information() const override;

    private:
        // JSON-RPC methods
        uint32_t SayHello(const JsonObject& parameters, JsonObject& response);
        uint32_t GetGreeting(const JsonObject& parameters, JsonObject& response);
        uint32_t SetGreeting(const JsonObject& parameters, JsonObject& response);

    private:
        PluginHost::IShell* _service;
        mutable Core::CriticalSection _adminLock;
        string _greeting;
        uint32_t _count;
    };

} // namespace Plugin
} // namespace WPEFramework
