//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2022 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_JAVASCRIPT_V8_HEADER_INCLUDED
#define CHOC_JAVASCRIPT_V8_HEADER_INCLUDED

#include "choc_javascript.h"

/**
    This file contains the implementation of choc::javascript::createV8Context().

    To use this V8 wrapper, you'll need to:
     - Make sure this header file is included by at least one of your source files.
     - Make sure the V8 header folder is on your include path.
     - Link the appropriate V8 static libraries into your binary.
     - Use choc::javascript::createV8Context() to create a V8-based context.
*/



//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

#include "../platform/choc_DisableAllWarnings.h"

#include "libplatform/libplatform.h"
#include "v8-context.h"
#include "v8-initialization.h"
#include "v8-isolate.h"
#include "v8-local-handle.h"
#include "v8-primitive.h"
#include "v8-script.h"
#include "v8-function.h"
#include "v8-container.h"
#include "v8-proxy.h"
#include "v8-exception.h"
#include "v8-template.h"
#include "v8-typed-array.h"

#include "../platform/choc_ReenableAllWarnings.h"


namespace choc::javascript
{

struct V8Initialiser
{
    V8Initialiser()
    {
        platform = v8::platform::NewDefaultPlatform();
        v8::V8::InitializePlatform (platform.get());
        v8::V8::Initialize();
    }

    ~V8Initialiser()
    {
        v8::V8::Dispose();
        v8::V8::DisposePlatform();
    }

    std::unique_ptr<v8::Platform> platform;

    static V8Initialiser& get()
    {
        static V8Initialiser i;
        return i;
    }
};

//==============================================================================
struct V8Context  : public Context::Pimpl
{
    V8Context()
    {
        (void) V8Initialiser::get();

        createParams.array_buffer_allocator = v8::ArrayBuffer::Allocator::NewDefaultAllocator();
        isolate = v8::Isolate::New (createParams);
        isolate->SetData (0, this);

        isolate->SetHostImportModuleDynamicallyCallback (+[] (v8::Local<v8::Context> c,
                                                              v8::Local<v8::Data> /*hostOptions*/,
                                                              v8::Local<v8::Value> /*resourceName*/,
                                                              v8::Local<v8::String> specifier,
                                                              v8::Local<v8::FixedArray> /*importAssertions*/)
        {
            return static_cast<V8Context*> (c->GetIsolate()->GetData (0))
                ->loadDynamicModule (c, specifier);
        });

        v8::Isolate::Scope isolateScope (isolate);
        v8::HandleScope handleScope (isolate);

        context.Reset (isolate, v8::Context::New (isolate));
    }

    ~V8Context() override
    {
        context.Clear();

        if (isolate != nullptr)
            isolate->Dispose();

        delete createParams.array_buffer_allocator;
    }

    choc::value::Value evaluateExpression (const std::string& code) override
    {
        v8::Isolate::Scope isolateScope (isolate);
        v8::HandleScope handleScope (isolate);
        auto localContext = context.Get (isolate);
        v8::Context::Scope contextScope (localContext);
        v8::TryCatch tryCatch (isolate);

        auto handleError = [&]
        {
            auto exception = getExceptionMessage (tryCatch, localContext);

            if (exception.empty())
                exception = "Unknown error";

            throw choc::javascript::Error (exception);
        };

        auto source = stringToV8 (code);
        v8::MaybeLocal<v8::Value> result;

        auto script = v8::Script::Compile (localContext, source);

        if (script.IsEmpty())
            handleError();

        result = script.ToLocalChecked()->Run (localContext);

        if (result.IsEmpty())
            handleError();

        return v8ToChoc (localContext, result.ToLocalChecked());
    }

    void run (const std::string& code, Context::ReadModuleContentFn* resolveModule, Context::CompletionHandler handleResult) override
    {
        v8::Isolate::Scope isolateScope (isolate);
        v8::HandleScope handleScope (isolate);
        auto localContext = context.Get (isolate);
        v8::Context::Scope contextScope (localContext);
        v8::TryCatch tryCatch (isolate);

        auto handleError = [&]
        {
            currentModuleResolver = nullptr;
            auto exception = getExceptionMessage (tryCatch, localContext);

            if (exception.empty())
                exception = "Unknown error";

            if (handleResult)
                handleResult (exception, {});
        };

        auto source = stringToV8 (code);
        v8::MaybeLocal<v8::Value> result;

        if (resolveModule != nullptr)
        {
            currentModuleResolver = resolveModule;

            auto mod = loadModule (localContext, source);

            if (mod.IsEmpty())
                return handleError();

            result = mod.ToLocalChecked()->Evaluate (localContext);
        }
        else
        {
            auto script = v8::Script::Compile (localContext, source);

            if (script.IsEmpty())
                return handleError();

            result = script.ToLocalChecked()->Run (localContext);
        }

        if (result.IsEmpty())
            return handleError();

        if (handleResult)
            handleResult ({}, v8ToChoc (localContext, result.ToLocalChecked()));
    }

    void pushObjectOrArray (const choc::value::ValueView& v) override
    {
        currentCallContext->functionArgs.emplace_back (chocToV8 (currentCallContext->localContext, v));
    }

    void pushArg (std::string_view s) override  { currentCallContext->functionArgs.emplace_back (stringToV8 (s)); }
    void pushArg (int32_t v) override           { currentCallContext->functionArgs.emplace_back (v8::Int32::New (isolate, v)); }
    void pushArg (uint32_t v) override          { currentCallContext->functionArgs.emplace_back (v8::Uint32::New (isolate, static_cast<int32_t> (v))); }
    void pushArg (int64_t v) override           { currentCallContext->functionArgs.emplace_back (v8::Number::New (isolate, v)); }
    void pushArg (double v) override            { currentCallContext->functionArgs.emplace_back (v8::Number::New (isolate, v)); }
    void pushArg (bool v) override              { currentCallContext->functionArgs.emplace_back (v8::Boolean::New (isolate, v)); }

    void prepareForCall (std::string_view name, uint32_t numArgs) override
    {
        currentCallContext = std::make_unique<CurrentCallContext> (*this);
        currentCallContext->functionName = stringToV8 (name, v8::NewStringType::kInternalized);
        currentCallContext->functionArgs.reserve (numArgs);
    }

    choc::value::Value performCall() override
    {
        choc::value::Value result;
        auto functionObject = currentCallContext->localContext->Global()->Get (currentCallContext->localContext,
                                                                               currentCallContext->functionName).ToLocalChecked();

        if (functionObject->IsFunction())
        {
            auto function = v8::Local<v8::Function>::Cast (functionObject);

            auto resultValue = function->Call (currentCallContext->localContext, v8::Null (isolate),
                                               static_cast<int> (currentCallContext->functionArgs.size()),
                                               currentCallContext->functionArgs.data());

            if (! resultValue.IsEmpty())
                result = v8ToChoc (currentCallContext->localContext, resultValue.ToLocalChecked());
        }

        currentCallContext.reset();
        return result;
    }

    void registerFunction (const std::string& name, Context::NativeFunction fn) override
    {
        v8::Isolate::Scope isolateScope (isolate);
        v8::HandleScope handleScope (isolate);
        auto localContext = context.Get (isolate);
        v8::Context::Scope contextScope (localContext);

        ++nextNativeFunctionHandle;
        nativeFunctions[nextNativeFunctionHandle] = std::move (fn);

        auto global = localContext->Global();
        auto ft = v8::FunctionTemplate::New (isolate, staticCallback, v8::Int32::New (isolate, nextNativeFunctionHandle));
        (void) global->Set (localContext, stringToV8 (name), ft->GetFunction (localContext).ToLocalChecked());
    }

    static void staticCallback (const v8::FunctionCallbackInfo<v8::Value>& info)
    {
        static_cast<V8Context*> (info.GetIsolate()->GetData (0))->callback (info);
    }

    void callback (const v8::FunctionCallbackInfo<v8::Value>& info)
    {
        v8::HandleScope handleScope (isolate);
        auto localContext = context.Get (isolate);
        v8::Context::Scope contextScope (localContext);
        auto data = info.Data()->Int32Value (localContext);

        if (data.IsJust())
        {
            if (auto fn = nativeFunctions.find (data.ToChecked()); fn != nativeFunctions.end())
            {
                auto numArgs = info.Length();
                std::vector<choc::value::Value> args;
                args.reserve (static_cast<size_t> (numArgs));

                for (int i = 0; i < numArgs; ++i)
                    args.emplace_back (v8ToChoc (localContext, info[i]));

                auto result = fn->second (ArgumentList { args.data(), args.size() });

                if (! result.isVoid())
                    info.GetReturnValue().Set (chocToV8 (localContext, result));
            }
        }
    }

    choc::value::Value v8ToChoc (v8::Local<v8::Context> c, const v8::Local<v8::Value>& value)
    {
        if (value->IsProxy())
            return v8ToChoc (c, v8::Local<v8::Proxy>::Cast (value)->GetTarget());

        if (value->IsString())   return choc::value::createString (v8StringToStd (value));
        if (value->IsInt32())    return choc::value::createInt32 (value->Int32Value (c).ToChecked());
        if (value->IsBoolean())  return choc::value::createBool (value->BooleanValue (isolate));
        if (value->IsNumber())   return choc::value::createFloat64 (value->NumberValue (c).ToChecked());
        if (value->IsBigInt())   return choc::value::createInt64 (value->ToBigInt (c).ToLocalChecked()->Int64Value());

        if (value->IsArray())
        {
            auto a = v8::Local<v8::Array>::Cast (value);
            auto len = a->Length();

            return choc::value::createArray (len, [&] (uint32_t index)
            {
                auto item = a->Get (c, index);

                if (item.IsEmpty())
                    return choc::value::Value();

                return v8ToChoc (c, item.ToLocalChecked());
            });
        }

        if (value->IsArrayBuffer() || value->IsArrayBufferView())
        {
            auto a = v8::Local<v8::TypedArray>::Cast (value);
            auto len = static_cast<uint32_t> (a->Length());

            return choc::value::createArray (len, [&] (uint32_t index)
            {
                auto item = a->Get (c, index);

                if (item.IsEmpty())
                    return choc::value::Value();

                return v8ToChoc (c, item.ToLocalChecked());
            });
        }

        if (value->IsObject())
        {
            auto o = value->ToObject (c).ToLocalChecked();
            auto propNames = o->GetPropertyNames (c);
            std::string className;

            if (propNames.IsEmpty())
                return {};

            std::vector<std::string> propKeys;
            std::vector<v8::Local<v8::Value>> propValues;

            {
                auto iterator = [&] (v8::Local<v8::Value> key)
                {
                    auto v = o->Get (c, key);
                    auto nameString = v8StringToStd (key);

                    if (nameString == objectNameAttribute)
                    {
                        if (! v.IsEmpty())
                            className = v8StringToStd (v.ToLocalChecked());
                    }
                    else
                    {
                        propKeys.emplace_back (v8StringToStd (key));

                        if (v.IsEmpty())
                            propValues.push_back ({});
                        else
                            propValues.emplace_back (v.ToLocalChecked());
                    }
                };

                using IteratorType = decltype (iterator);

                propNames.ToLocalChecked()->Iterate (c, +[] (uint32_t, v8::Local<v8::Value> key, void* i)
                {
                    (*static_cast<IteratorType*> (i)) (key);
                    return v8::Array::CallbackResult::kContinue;
                },
                std::addressof (iterator));
            }

            auto result = choc::value::createObject (className);

            for (size_t i = 0; i < propKeys.size(); ++i)
            {
                const auto& keyName = propKeys[i];
                CHOC_ASSERT (! keyName.empty());

                if (! propValues[i].IsEmpty())
                    result.setMember (keyName, v8ToChoc (c, propValues[i]));
                else
                    result.setMember (keyName, choc::value::Value());
            }

            return result;
        }

        CHOC_ASSERT (value->IsUndefined() || value->IsNull());
        return {};
    }

    v8::Local<v8::Value> chocToV8 (v8::Local<v8::Context> c, const choc::value::ValueView& v)
    {
        if (v.isInt32())    return v8::Int32::New (isolate, v.get<int32_t>());
        if (v.isInt64())    return v8::Number::New (isolate, v.get<int64_t>());
        if (v.isFloat())    return v8::Number::New (isolate, v.get<double>());
        if (v.isBool())     return v8::Boolean::New (isolate, v.getBool());
        if (v.isString())   return stringToV8 (v.getString());

        if (v.isArray() || v.isVector())
        {
            auto a = v8::Array::New (isolate, static_cast<int> (v.size()));

            if (a.IsEmpty())
                return v8::Local<v8::Array>();

            uint32_t index = 0;

            for (auto element : v)
                (void) a->Set (c, index++, chocToV8 (c, element));

            return a;
        }

        if (v.isObject())
        {
            auto o = v8::Object::New (isolate);

            if (auto className = v.getObjectClassName(); ! className.empty())
                (void) o->Set (c, stringToV8 (objectNameAttribute), stringToV8 (className));

            v.visitObjectMembers ([&] (std::string_view name, const choc::value::ValueView& value)
            {
                (void) o->Set (c, stringToV8 (name), chocToV8 (c, value));
            });

            return o;
        }

        CHOC_ASSERT (v.isVoid());
        return v8::Undefined (isolate);
    }

    v8::Local<v8::String> stringToV8 (std::string_view s, v8::NewStringType type = v8::NewStringType::kNormal)
    {
        return v8::String::NewFromUtf8 (isolate, s.data(), type, static_cast<int> (s.length())).ToLocalChecked();
    }

    std::string v8StringToStd (v8::String& s)
    {
        std::string result;
        auto len = s.Utf8Length (isolate);
        result.resize (static_cast<std::string::size_type> (len));
        s.WriteUtf8 (isolate, result.data(), len);
        return result;
    }

    std::string v8StringToStd (const v8::Local<v8::Value>& v)
    {
        v8::String::Utf8Value str (isolate, v);
        return *str;
    }

    std::string getExceptionMessage (v8::TryCatch& tryCatch, v8::Local<v8::Context> localContext)
    {
        std::ostringstream result;
        v8::Local<v8::Message> message = tryCatch.Message();

        auto desc = v8StringToStd (tryCatch.Exception());

        if (desc.empty())
            return {};

        auto filename = v8StringToStd (message->GetScriptOrigin().ResourceName());
        int lineNumber = message->GetLineNumber (localContext).FromJust();
        int startCol = message->GetStartColumn (localContext).FromJust();
        int endCol = message->GetEndColumn (localContext).FromJust();

        if (! filename.empty())
            result << filename << ":" << lineNumber << ":" << startCol << ": ";

        result << desc << std::endl;

        if (auto sourceLine = v8StringToStd (message->GetSourceLine (localContext).ToLocalChecked()); ! sourceLine.empty())
        {
            result << sourceLine << std::endl;

            if (endCol > startCol)
                result << std::string (static_cast<size_t> (startCol), ' ')
                       << std::string (static_cast<size_t> (endCol - startCol), '^') << std::endl;
        }

        v8::Local<v8::Value> stackTrace;

        if (tryCatch.StackTrace (localContext).ToLocal (&stackTrace)
             && stackTrace->IsString()
             && stackTrace.As<v8::String>()->Length() > 0)
        {
            if (auto st = v8StringToStd (stackTrace); ! st.empty())
                result << std::endl << "Stack trace:" << std::endl << st << std::endl;
        }

        return result.str();
    }

    v8::MaybeLocal<v8::Module> loadModule (v8::Local<v8::Context> localContext, v8::Local<v8::String> source)
    {
        v8::ScriptOrigin origin ({}, 0, 0, false, -1, {}, false, false, true);
        v8::ScriptCompiler::Source src (source, origin);
        auto mod = v8::ScriptCompiler::CompileModule (isolate, &src);

        if (mod.IsEmpty())
            return {};

        auto ok = mod.ToLocalChecked()->InstantiateModule (localContext, +[] (v8::Local<v8::Context> c, v8::Local<v8::String> specifier,
                                                                              v8::Local<v8::FixedArray>, v8::Local<v8::Module>)
        {
            return static_cast<V8Context*> (c->GetIsolate()->GetData (0))
                ->loadStaticModule (c, specifier);
        });

        if (ok.IsNothing() || ! ok.ToChecked())
            return {};

        return mod;
    }

    v8::MaybeLocal<v8::Module> loadStaticModule (v8::Local<v8::Context> c,
                                                 v8::Local<v8::String> specifier)
    {
        if (currentModuleResolver != nullptr)
        {
            auto path = v8StringToStd (specifier);
            auto maybeCode = (*currentModuleResolver) (path);

            if (maybeCode)
                return loadModule (c, stringToV8 (*maybeCode));
        }

        return {};
    }

    v8::MaybeLocal<v8::Promise> loadDynamicModule (v8::Local<v8::Context> c,
                                                   v8::Local<v8::String> specifier)
    {
        auto mod = loadStaticModule (c, specifier);

        if (mod.IsEmpty())
            return {};

        auto result = mod.ToLocalChecked()->Evaluate (c);

        if (result.IsEmpty())
            return {};

        auto ns = mod.ToLocalChecked()->GetModuleNamespace();

        auto resolver = v8::Promise::Resolver::New (c).ToLocalChecked();
        v8::MaybeLocal<v8::Promise> promise (resolver->GetPromise());
        (void) resolver->Resolve (c, ns);
        return promise;
    }

    void pumpMessageLoop() override
    {
        v8::platform::PumpMessageLoop (V8Initialiser::get().platform.get(), isolate);
        isolate->PerformMicrotaskCheckpoint();
    }

    //==============================================================================
    v8::Isolate::CreateParams createParams;
    v8::Isolate* isolate = {};
    v8::UniquePersistent<v8::Context> context;
    Context::ReadModuleContentFn* currentModuleResolver = {};
    std::unordered_map<int32_t, Context::NativeFunction> nativeFunctions;
    int32_t nextNativeFunctionHandle = 0;

    struct CurrentCallContext
    {
        CurrentCallContext (V8Context& c)
            : isolateScope (c.isolate),
              handleScope (c.isolate),
              localContext (c.context.Get (c.isolate)),
              contextScope (localContext)
        {
        }

        v8::Local<v8::String> functionName;
        std::vector<v8::Local<v8::Value>> functionArgs;

        v8::Isolate::Scope isolateScope;
        v8::HandleScope handleScope;
        v8::Local<v8::Context> localContext;
        v8::Context::Scope contextScope;
    };

    std::unique_ptr<CurrentCallContext> currentCallContext;

    static constexpr const char* objectNameAttribute = "_objectName";
};

inline Context createV8Context()
{
    return Context (std::make_unique<V8Context>());
}

} // namespace choc::javascript

#endif // CHOC_JAVASCRIPT_V8_HEADER_INCLUDED
