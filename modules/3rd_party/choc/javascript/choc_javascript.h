//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Clean Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2021 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_JAVASCRIPT_HEADER_INCLUDED
#define CHOC_JAVASCRIPT_HEADER_INCLUDED

#include <stdexcept>
#include "../containers/choc_Value.h"
#include "../text/choc_JSON.h"

/**
    A simple javascript engine (currently using duktape internally)
*/
namespace choc::javascript
{
    /// This is thrown by any javascript functions that need to report an error
    struct Error  : public std::runtime_error
    {
        Error (const std::string& error) : std::runtime_error (error) {}
    };

    //==============================================================================
    /// Helper class to hold and provide access to the arguments in a javascript
    /// function callback.
    struct ArgumentList
    {
        const choc::value::Value* args;
        size_t numArgs;

        /// Returns the number of arguments
        size_t size() const noexcept                                        { return numArgs; }
        /// Returns true if there are no arguments
        bool empty() const noexcept                                         { return numArgs == 0; }

        /// Returns an argument, or a nullptr if the index is out of range.
        const choc::value::Value* operator[] (size_t index) const;

        /// Gets an argument as a primitive type (or a string).
        /// If the index is out of range or the object isn't a suitable type,
        /// then the default value provided will be returned instead.
        template <typename PrimitiveType>
        PrimitiveType get (size_t argIndex, PrimitiveType defaultValue = {}) const;

        /// Standard iterator
        const choc::value::Value* begin() const noexcept                    { return args; }
        const choc::value::Value* end() const noexcept                      { return args + numArgs; }
    };

    //==============================================================================
    /**
        An execution context which you use for running javascript code.

        Couldn't be much simpler to use one of these: just create a Context, add any
        native bindings that you need using registerFunction(), and then use evaluate()
        or invoke() to execute code with it.

        The context isn't thread safe, so it's up to the caller to handle thread
        synchronisation issues.
    */
    struct Context
    {
        Context();
        ~Context();

        /// Evaluates the given chunk of javascript.
        /// If there are any parse errors, this will throw a choc::javascript::Error exception.
        choc::value::Value evaluate (std::string_view javascriptCode);

        /// Attempts to invoke a global function with no arguments.
        /// Any parse errors will throw a choc::javascript::Error exception.
        choc::value::Value invoke (std::string_view functionName);

        /// Attempts to invoke a global function with the arguments provided.
        /// The arguments can be primitives, strings, choc::value::ValueView or
        /// choc::value::Value types.
        /// Any parse errors will throw a choc::javascript::Error exception.
        template <typename... Args>
        choc::value::Value invoke (std::string_view functionName, Args&&... args);

        /// Attempts to invoke a global function with an array of arguments.
        /// The objects in the argument list can be primitives, strings, choc::value::ValueView
        /// or choc::value::Value types.
        /// Any parse errors will throw a choc::javascript::Error exception.
        template <typename ArgList>
        choc::value::Value invokeWithArgList (std::string_view functionName, const ArgList& args);

        /// This is the prototype for a lambda which can be bound as a javascript function.
        using NativeFunction = std::function<choc::value::Value(ArgumentList)>;

        /// Binds a lambda function to a global name so that javascript code can invoke it.
        void registerFunction (std::string_view name, NativeFunction fn);

    private:
        struct Pimpl;
        std::unique_ptr<Pimpl> pimpl;

        void prepareForCall (std::string_view, size_t);
        choc::value::Value performCall (size_t);
        void addValueArgument (const choc::value::ValueView&);
        void addDoubleArgument (double);
        void addBoolArgument (bool);
        void addStringArgument (std::string_view);
        template <typename Type> void pushValue (const Type&);
    };
}


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

namespace choc::javascript
{

template <typename Type>
void Context::pushValue (const Type& v)
{
    if constexpr (std::is_same<Type, choc::value::Value>::value)
        addValueArgument (v.getView());
    else if constexpr (std::is_same<Type, choc::value::ValueView>::value)
        addValueArgument (v);
    else if constexpr (std::is_same<Type, bool>::value)
        addBoolArgument (v);
    else if constexpr (std::is_integral<Type>::value || std::is_floating_point<Type>::value)
        addDoubleArgument (static_cast<double> (v));
    else
        addStringArgument (std::string_view (v));
}

inline choc::value::Value Context::invoke (std::string_view functionName)
{
    prepareForCall (functionName, 0);
    return performCall (0);
}

template <typename... Args>
choc::value::Value Context::invoke (std::string_view functionName, Args&&... args)
{
    prepareForCall (functionName, sizeof...(args));
    (pushValue (std::forward<Args> (args)), ...);
    return performCall (sizeof...(args));
}

template <typename ArgList>
choc::value::Value Context::invokeWithArgList (std::string_view functionName, const ArgList& args)
{
    prepareForCall (functionName, args.size());

    for (auto& arg : args)
        pushValue (arg);

    return performCall (args.size());
}

template <typename PrimitiveType>
PrimitiveType ArgumentList::get (size_t index, PrimitiveType defaultValue) const
{
    if (auto a = (*this)[index])
        return a->getWithDefault<PrimitiveType> (defaultValue);

    return defaultValue;
}

} // namespace choc::javascript

#endif // CHOC_JAVASCRIPT_HEADER_INCLUDED

//==============================================================================
/// In order to avoid pulling in the whole of the dependencies, you should set this
/// macro to 1 in one of your compile units
#if CHOC_JAVASCRIPT_IMPLEMENTATION

#include "../platform/choc_Platform.h"

#if CHOC_WINDOWS
 #include <windows.h>
#else
 #include <cmath>
 #include <climits>
 #include <ctime>
 #include <cstddef>
 #include <cstdarg>
 #include <cstring>
 #include <ctime>
#endif

#include <csetjmp>

#if CHOC_LINUX || CHOC_OSX
 #include <sys/time.h>
#endif

namespace choc::javascript
{

namespace duktape
{
 using std::signbit;
 using std::fpclassify;
 using std::isnan;
 using std::isinf;
 using std::isfinite;
 #include "../platform/choc_DisableAllWarnings.h"
 #include "duktape/duktape.c.inc"
 #include "../platform/choc_ReenableAllWarnings.h"
}

//==============================================================================
struct Context::Pimpl
{
    Pimpl() : context (duktape::duk_create_heap (nullptr, nullptr, nullptr, nullptr, fatalError))
    {
        CHOC_ASSERT (context != nullptr);
    }

    ~Pimpl()
    {
        duk_destroy_heap (context);
    }

    void reset()
    {
        while (duk_get_top (context))
            duk_remove (context, duk_get_top_index (context));
    }

    static void fatalError (void*, const char* message)    { throw Error (message); }

    void throwError()
    {
        std::string message = duk_safe_to_string (context, -1);
        reset();
        throw Error (message);
    }

    void evalString (std::string_view code)
    {
        if (duk_peval_lstring (context, code.data(), code.length()) != DUK_EXEC_SUCCESS)
            throwError();
    }

    choc::value::Value evaluate (std::string_view javascriptCode)
    {
        evalString (javascriptCode);
        auto result = readValue (context, -1);
        duk_pop (context);
        return result;
    }

    void prepareForCall (std::string_view functionName, size_t numArgs)
    {
        evalString (functionName);

        if (! duk_is_function (context, -1))
            throw Error ("No such function");

        duk_require_stack_top (context, (duktape::duk_idx_t) numArgs);
    }

    choc::value::Value performCall (size_t numArgs)
    {
        if (duk_pcall (context, (duktape::duk_idx_t) numArgs) != DUK_EXEC_SUCCESS)
            throwError();

        auto returnValue = readValue (context, -1);
        duk_pop (context);
        return returnValue;
    }

    void registerFunction (std::string_view name, NativeFunction fn)
    {
        duk_push_global_object (context);

        registeredFunctions.push_back (std::make_unique<RegisteredFunction> (std::move (fn), *this));
        registeredFunctions.back()->pushToStack (context);

        duk_put_prop_lstring (context, -2, name.data(), name.length());
        duk_pop (context);
    }

    static constexpr const char* objectNameAttribute = "_objectName";

    void pushValue (const choc::value::ValueView& v)    { pushValue (context, v); }
    void pushValue (double value)                       { duk_push_number (context, value); }
    void pushValue (bool value)                         { duk_push_boolean (context, value); }
    void pushValue (std::string_view value)             { pushValue (context, value); }

    static void pushValue (duktape::duk_context* ctx, std::string_view value)
    {
        if (choc::text::isValidCESU8 (value))
            return (void) duk_push_lstring (ctx, value.data(), value.length());

        auto cesu8 = choc::text::convertUTF8ToCESU8 (choc::text::UTF8Pointer (std::string (value).data()));
        duk_push_lstring (ctx, cesu8.data(), cesu8.length());
    }

    static void pushValue (duktape::duk_context* ctx, const choc::value::ValueView& v)
    {
        if (v.isInt() || v.isFloat())   return duk_push_number (ctx, v.get<double>());
        if (v.isBool())                 return duk_push_boolean (ctx, v.getBool());
        if (v.isString())               return pushValue (ctx, v.getString());

        if (v.isArray() || v.isVector())
        {
            auto arrayIndex = duk_push_array (ctx);
            duktape::duk_uarridx_t elementIndex = 0;

            for (const auto& element : v)
            {
                pushValue (ctx, element);
                duk_put_prop_index (ctx, arrayIndex, elementIndex++);
            }

            return;
        }

        if (v.isObject())
        {
            auto objectIndex = duk_push_object (ctx);

            auto className = v.getObjectClassName();

            if (! className.empty())
            {
                duk_push_lstring (ctx, className.data(), className.length());
                duk_put_prop_string (ctx, objectIndex, objectNameAttribute);
            }

            v.visitObjectMembers ([&] (std::string_view name, const choc::value::ValueView& value)
            {
                pushValue (ctx, value);
                duk_put_prop_lstring (ctx, objectIndex, name.data(), name.length());
            });

            return;
        }

        CHOC_ASSERT (v.isVoid()); // types like vectors aren't currently supported
        return duk_push_undefined (ctx);
    }

    static choc::value::Value readValue (duktape::duk_context* ctx, duktape::duk_idx_t index)
    {
        switch (duk_get_type (ctx, index))
        {
            case DUK_TYPE_NULL:      return {};
            case DUK_TYPE_UNDEFINED: return {};
            case DUK_TYPE_BOOLEAN:   return choc::value::createBool (static_cast<bool> (duk_get_boolean (ctx, index)));
            case DUK_TYPE_NUMBER:    return choc::value::createFloat64 (duk_get_number (ctx, index));

            case DUK_TYPE_STRING:
            {
                if (auto s = duk_get_string (ctx, index))
                {
                    choc::text::UTF8Pointer cesu8 (s);

                    if (choc::text::containsSurrogatePairs (cesu8))
                        return choc::value::createString (choc::text::convertSurrogatePairsToUTF8 (cesu8));

                    return choc::value::createString (std::string_view (s));
                }

                return choc::value::createString ({});
            }

            case DUK_TYPE_OBJECT:
            case DUK_TYPE_LIGHTFUNC:
            {
                if (duk_is_array (ctx, index))
                {
                    return choc::value::createArray (static_cast<uint32_t> (duk_get_length (ctx, index)),
                                                     [&] (uint32_t i) -> choc::value::Value
                    {
                        duk_get_prop_index (ctx, index, static_cast<duktape::duk_uarridx_t> (i));
                        auto element = readValue (ctx, -1);
                        duk_pop (ctx);
                        return element;
                    });
                }

                if (duk_is_function (ctx, index) || duk_is_lightfunc (ctx, index))
                    throw Error ("Cannot handle 'function' object type");

                // Handle an object
                duk_enum (ctx, index, DUK_ENUM_OWN_PROPERTIES_ONLY);

                bool anyRemaining = duk_next (ctx, -1, 1);
                std::string_view name;

                // look for an object name attribute as the first field
                if (anyRemaining
                     && duk_get_type (ctx, -2) == static_cast<duktape::duk_int_t> (DUK_TYPE_STRING)
                     && duk_to_string (ctx, -2) == std::string_view (objectNameAttribute))
                {
                    name = duk_to_string (ctx, -1);
                    duk_pop_2 (ctx);
                    anyRemaining = duk_next (ctx, -1, 1);
                }

                auto object = choc::value::createObject (name);

                while (anyRemaining)
                {
                    object.addMember (duk_to_string (ctx, -2), readValue (ctx, -1));
                    duk_pop_2 (ctx);
                    anyRemaining = duk_next (ctx, -1, 1);
                }

                duk_pop (ctx);
                return object;
            }

            case DUK_TYPE_NONE:
            default:
                CHOC_ASSERT (false);
                return {};
        }
    }

    struct RegisteredFunction
    {
        RegisteredFunction (NativeFunction&& f, Pimpl& p) : function (std::move (f)), owner (p) {}

        NativeFunction function;
        Pimpl& owner;

        static duktape::duk_ret_t invoke (duktape::duk_context* ctx)
        {
            duk_push_current_function (ctx);
            auto& fn = RegisteredFunction::get (ctx, -1);
            duk_pop (ctx);

            auto numArgs = duk_get_top (ctx);

            if (numArgs <= 4)
            {
                choc::value::Value args[4];

                for (decltype (numArgs) i = 0; i < numArgs; ++i)
                    args[i] = readValue (ctx, i);

                return fn.invokeWithArgs (ctx, args, (size_t) numArgs);
            }

            std::vector<choc::value::Value> args;
            args.reserve ((size_t) numArgs);

            for (decltype (numArgs) i = 0; i < numArgs; ++i)
                args.push_back (readValue (ctx, i));

            return fn.invokeWithArgs (ctx, args.data(), (size_t) numArgs);
        }

        duktape::duk_ret_t invokeWithArgs (duktape::duk_context* ctx, const choc::value::Value* args, size_t numArgs)
        {
            auto result = std::invoke (function, ArgumentList { args, numArgs });

            if (result.isVoid())
                return 0;

            pushValue (ctx, result);
            return 1;
        }

        static duktape::duk_ret_t destroy (duktape::duk_context* ctx)
        {
            duk_require_function (ctx, 0);
            RegisteredFunction::get (ctx, 0).deregister();
            duk_pop (ctx);
            return 0;
        }

        void attachToObject (duktape::duk_context* ctx)
        {
            duk_push_pointer (ctx, (void*) this);
            duk_put_prop_string (ctx, -2, DUK_HIDDEN_SYMBOL("registeredFn"));
        }

        void pushToStack (duktape::duk_context* ctx)
        {
            duk_push_c_function (ctx, RegisteredFunction::invoke, (duktape::duk_int_t) (-1));
            attachToObject (ctx);
            duk_push_c_function (ctx, RegisteredFunction::destroy, 1);
            attachToObject (ctx);
            duk_set_finalizer (ctx, -2);
        }

        void deregister()
        {
            auto oldEnd = owner.registeredFunctions.end();
            auto newEnd = std::remove_if (owner.registeredFunctions.begin(), oldEnd,
                                          [this] (auto& f) { return f.get() == this; });

            if (newEnd != oldEnd)
                owner.registeredFunctions.erase (newEnd, oldEnd);
        }

        static RegisteredFunction& get (duktape::duk_context* ctx, duktape::duk_idx_t index)
        {
            duk_get_prop_string (ctx, index, DUK_HIDDEN_SYMBOL("registeredFn"));
            auto r = static_cast<RegisteredFunction*> (duk_get_pointer (ctx, -1));
            CHOC_ASSERT (r != nullptr);
            duk_pop (ctx);
            return *r;
        }
    };

    duktape::duk_context* context = nullptr;
    std::vector<std::unique_ptr<RegisteredFunction>> registeredFunctions;
};

Context::Context()  : pimpl (std::make_unique<Pimpl>()) {}
Context::~Context() = default;

choc::value::Value Context::evaluate (std::string_view javascriptCode)
{
    return pimpl->evaluate (std::move (javascriptCode));
}

void Context::registerFunction (std::string_view name, NativeFunction fn)
{
    pimpl->registerFunction (std::move (name), std::move (fn));
}

const choc::value::Value* ArgumentList::operator[] (size_t index) const
{
    return index < numArgs ? args + index : nullptr;
}

void Context::prepareForCall (std::string_view name, size_t numArgs) { pimpl->prepareForCall (name, numArgs); }
choc::value::Value Context::performCall (size_t numArgs)             { return pimpl->performCall (numArgs); }
void Context::addValueArgument (const choc::value::ValueView& v)     { pimpl->pushValue (v); }
void Context::addDoubleArgument (double v)                           { pimpl->pushValue (v); }
void Context::addBoolArgument (bool v)                               { pimpl->pushValue (v); }
void Context::addStringArgument (std::string_view v)                 { pimpl->pushValue (v); }

} // namespace choc::javascript

#endif // CHOC_JAVASCRIPT_IMPLEMENTATION
