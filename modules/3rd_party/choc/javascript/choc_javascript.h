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

#ifndef CHOC_JAVASCRIPT_HEADER_INCLUDED
#define CHOC_JAVASCRIPT_HEADER_INCLUDED

#include <stdexcept>
#include <functional>
#include "../containers/choc_Value.h"
#include "../text/choc_JSON.h"

/**
    Wrapper classes for encapsulating different javascript engines such as
    Duktape and QuickJS.

    Just use one of the context-creation functions such as
    choc::javascript::createQuickJSContext() to create a context for running
    javascript code.
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
        const choc::value::Value* args = nullptr;
        size_t numArgs = 0;

        /// Returns the number of arguments
        size_t size() const noexcept                                    { return numArgs; }
        /// Returns true if there are no arguments
        bool empty() const noexcept                                     { return numArgs == 0; }

        /// Returns an argument, or a nullptr if the index is out of range.
        const choc::value::Value* operator[] (size_t index) const       { return index < numArgs ? args + index : nullptr; }

        /// Gets an argument as a primitive type (or a string).
        /// If the index is out of range or the object isn't a suitable type,
        /// then the default value provided will be returned instead.
        template <typename PrimitiveType>
        PrimitiveType get (size_t argIndex, PrimitiveType defaultValue = {}) const;

        /// Standard iterator
        const choc::value::Value* begin() const noexcept                { return args; }
        const choc::value::Value* end() const noexcept                  { return args + numArgs; }
    };

    //==============================================================================
    /**
        An execution context which you use for running javascript code.

        These are really simple to use: call one of the creation functions such
        as choc::javascript::createQuickJSContext() which will give you a shared_ptr to
        a context. Then you can add any native bindings that you need with
        registerFunction(), and call evaluate() or invoke() to execute code or call
        functions directly.

        These contexts are not thread safe, so it's up to the caller to handle thread
        synchronisation issues.
    */
    class Context
    {
    public:
        /// To create a Context, use a function such as choc::javascript::createQuickJSContext();
        Context() = delete;
        Context (Context&&);
        Context& operator= (Context&&);
        ~Context();

        //==============================================================================
        /// Evaluates the given chunk of javascript.
        /// If there are any parse errors, this will throw a choc::javascript::Error exception.
        choc::value::Value evaluate (const std::string& javascriptCode);

        /// Attempts to invoke a global function with no arguments.
        /// Any errors will throw a choc::javascript::Error exception.
        choc::value::Value invoke (std::string_view functionName);

        /// Attempts to invoke a global function with the arguments provided.
        /// The arguments can be primitives, strings, choc::value::ValueView or
        /// choc::value::Value types.
        /// Any errors will throw a choc::javascript::Error exception.
        template <typename... Args>
        choc::value::Value invoke (std::string_view functionName, Args&&... args);

        /// Attempts to invoke a global function with an array of arguments.
        /// The objects in the argument list can be primitives, strings, choc::value::ValueView
        /// or choc::value::Value types.
        /// Any errors will throw a choc::javascript::Error exception.
        template <typename ArgList>
        choc::value::Value invokeWithArgList (std::string_view functionName, const ArgList& args);

        /// This is the prototype for a lambda which can be bound as a javascript function.
        using NativeFunction = std::function<choc::value::Value(ArgumentList)>;

        /// Binds a lambda function to a global name so that javascript code can invoke it.
        void registerFunction (const std::string& name, NativeFunction fn);

        //==============================================================================
        /// @internal
        struct Pimpl;
        /// @internal
        Context (std::unique_ptr<Pimpl>);

    private:
        std::unique_ptr<Pimpl> pimpl;
    };

    //==============================================================================
    /// Creates a QuickJS-based context. If you call this, then you'll need to
    /// include choc_javascript_QuickJS.h in one (and only one!) of your source files.
    Context createQuickJSContext();

    /// Creates a Duktape-based context. If you call this, then you'll need to
    /// include choc_javascript_Duktape.h in one (and only one!) of your source files.
    Context createDuktapeContext();
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

template <typename PrimitiveType>
PrimitiveType ArgumentList::get (size_t index, PrimitiveType defaultValue) const
{
    if (auto a = (*this)[index])
        return a->getWithDefault<PrimitiveType> (defaultValue);

    return defaultValue;
}

struct Context::Pimpl
{
    Pimpl() = default;
    virtual ~Pimpl() = default;

    virtual void registerFunction (const std::string&, NativeFunction) = 0;
    virtual choc::value::Value evaluate (const std::string&) = 0;
    virtual void prepareForCall (std::string_view, uint32_t numArgs) = 0;
    virtual choc::value::Value performCall() = 0;
    virtual void pushObjectOrArray (const choc::value::ValueView&) = 0;
    virtual void pushArg (std::string_view) = 0;
    virtual void pushArg (int32_t) = 0;
    virtual void pushArg (int64_t) = 0;
    virtual void pushArg (uint32_t) = 0;
    virtual void pushArg (double) = 0;
    virtual void pushArg (bool) = 0;

    void pushArg (const std::string& v)   { pushArg (std::string_view (v)); }
    void pushArg (const char* v)          { pushArg (std::string_view (v)); }
    void pushArg (uint64_t v)             { pushArg (static_cast<int64_t> (v)); }
    void pushArg (float v)                { pushArg (static_cast<double> (v)); }

    void pushArg (const choc::value::ValueView& v)
    {
        if (v.isInt32())    return pushArg (v.getInt32());
        if (v.isInt64())    return pushArg (v.getInt64());
        if (v.isFloat32())  return pushArg (v.getFloat32());
        if (v.isFloat64())  return pushArg (v.getFloat64());
        if (v.isString())   return pushArg (v.getString());
        if (v.isBool())     return pushArg (v.getBool());
        if (v.isVoid())     throw Error ("Function arguments cannot be void!");

        pushObjectOrArray (v);
    }
};

inline Context::Context (std::unique_ptr<Pimpl> p) : pimpl (std::move (p)) {}
inline Context::~Context() = default;
inline Context::Context (Context&&) = default;
inline Context& Context::operator= (Context&&) = default;

inline choc::value::Value Context::invoke (std::string_view functionName)
{
    CHOC_ASSERT (pimpl != nullptr); // cannot call this on a moved-from context!
    pimpl->prepareForCall (functionName, 0);
    return pimpl->performCall();
}

template <typename... Args>
choc::value::Value Context::invoke (std::string_view functionName, Args&&... args)
{
    CHOC_ASSERT (pimpl != nullptr); // cannot call this on a moved-from context!
    pimpl->prepareForCall (functionName, sizeof...(args));
    (pimpl->pushArg (std::forward<Args> (args)), ...);
    return pimpl->performCall();
}

template <typename ArgList>
choc::value::Value Context::invokeWithArgList (std::string_view functionName, const ArgList& args)
{
    CHOC_ASSERT (pimpl != nullptr); // cannot call this on a moved-from context!
    pimpl->prepareForCall (functionName, static_cast<uint32_t> (args.size()));

    for (auto& arg : args)
        pimpl->pushArg (arg);

    return pimpl->performCall();
}

inline void Context::registerFunction (const std::string& name, NativeFunction fn)
{
    CHOC_ASSERT (pimpl != nullptr); // cannot call this on a moved-from context!
    pimpl->registerFunction (name, std::move (fn));
}

inline choc::value::Value Context::evaluate (const std::string& javascriptCode)
{
    CHOC_ASSERT (pimpl != nullptr); // cannot call this on a moved-from context!
    return pimpl->evaluate (javascriptCode);
}

} // namespace choc::javascript

#endif // CHOC_JAVASCRIPT_HEADER_INCLUDED

#ifdef CHOC_JAVASCRIPT_IMPLEMENTATION
 // The way the javascript classes work has changed: instead of
 // setting CHOC_JAVASCRIPT_IMPLEMENTATION in one of your source files, just
 // include the actual engine that you want to use, e.g. choc_javasscript_QuickJS.h
 // in (only!) one of your source files, and use that to create instances of that engine.
 #error "CHOC_JAVASCRIPT_IMPLEMENTATION is deprecated"
#endif
