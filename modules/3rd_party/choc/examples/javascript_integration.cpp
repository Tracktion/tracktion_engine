#include <iostream>
#include <string>
#include <cmath>
#include "../choc/javascript/choc_javascript.h"

// Include one of the JavaScript engine implementations
// Note: This is a large file that contains the entire QuickJS engine
#include "../choc/javascript/choc_javascript_QuickJS.h"

void demonstrateBasicJavaScript()
{
    std::cout << "\n=== Basic JavaScript Execution Demo ===\n";

    try
    {
        // Create a QuickJS context
        auto context = choc::javascript::createQuickJSContext();

        // Execute simple JavaScript expressions
        auto result1 = context.evaluateExpression ("2 + 3");
        std::cout << "2 + 3 = " << result1.getWithDefault<int>(0) << "\n";

        auto result2 = context.evaluateExpression ("Math.sqrt (16)");
        std::cout << "Math.sqrt (16) = " << result2.getWithDefault<double>(0.0) << "\n";

        auto result3 = context.evaluateExpression ("'Hello, ' + 'World!'");
        std::cout << "String concatenation: " << result3.toString() << "\n";

        // Execute more complex JavaScript
        auto result4 = context.evaluateExpression (R"(
            (function() {
                function factorial (n) {
                    if (n <= 1) return 1;
                    return n * factorial (n - 1);
                }
                return factorial (5);
            })()
        )");
        std::cout << "factorial (5) = " << result4.getWithDefault<int>(0) << "\n";

        // Work with JavaScript objects
        context.evaluateExpression ("var person = { name: 'John', age: 30, city: 'New York' };");
        auto name = context.evaluateExpression ("person.name");
        auto age = context.evaluateExpression ("person.age");
        std::cout << "Person: " << name.toString()
                  << ", age " << age.getWithDefault<int>(0) << "\n";
    }
    catch (const choc::javascript::Error& e)
    {
        std::cout << "JavaScript Error: " << e.what() << "\n";
    }
}

void demonstrateNativeFunctionBinding()
{
    std::cout << "\n=== Native Function Binding Demo ===\n";

    try
    {
        auto context = choc::javascript::createQuickJSContext();

        // Register a simple C++ function
        context.registerFunction ("add", [](choc::javascript::ArgumentList args) -> choc::value::Value
        {
            double a = args.get<double>(0, 0.0);
            double b = args.get<double>(1, 0.0);
            return choc::value::createFloat64 (a + b);
        });

        // Register a more complex function
        context.registerFunction ("greet", [](choc::javascript::ArgumentList args) -> choc::value::Value
        {
            std::string name = args.get<std::string>(0, "World");
            return choc::value::createString ("Hello, " + name + "!");
        });

        // Register a function that works with arrays
        context.registerFunction ("sum", [](choc::javascript::ArgumentList args) -> choc::value::Value
        {
            if (args.size() == 0)
                return choc::value::createFloat64 (0.0);

            auto array = args[0];
            if (! array || ! array->isArray())
                return choc::value::createFloat64 (0.0);

            double sum = 0.0;
            for (uint32_t i = 0; i < array->size(); ++i)
            {
                auto element = (*array)[i];
                if (element.isFloat() || element.isInt())
                    sum += element.getWithDefault<double>(0.0);
            }
            return choc::value::createFloat64 (sum);
        });

        // Register a function that demonstrates error handling
        context.registerFunction ("divide", [](choc::javascript::ArgumentList args) -> choc::value::Value
        {
            double a = args.get<double>(0, 0.0);
            double b = args.get<double>(1, 0.0);
            if (b == 0.0)
                throw choc::javascript::Error ("Division by zero!");
            return choc::value::createFloat64 (a / b);
        });

        // Test the registered functions
        std::cout << "Testing native functions from JavaScript:\n";

        auto result1 = context.evaluateExpression ("add (5, 3)");
        std::cout << "add (5, 3) = " << result1.getWithDefault<double>(0.0) << "\n";

        auto result2 = context.evaluateExpression ("greet ('JavaScript')");
        std::cout << "greet ('JavaScript') = " << result2.toString() << "\n";

        auto result3 = context.evaluateExpression ("sum ([1, 2, 3, 4, 5])");
        std::cout << "sum ([1, 2, 3, 4, 5]) = " << result3.getWithDefault<double>(0.0) << "\n";

        auto result4 = context.evaluateExpression ("divide (10, 2)");
        std::cout << "divide (10, 2) = " << result4.getWithDefault<double>(0.0) << "\n";

        // Test error handling
        try
        {
            context.evaluateExpression ("divide (10, 0)");
        }
        catch (const choc::javascript::Error& e)
        {
            std::cout << "Expected error: " << e.what() << "\n";
        }
    }
    catch (const choc::javascript::Error& e)
    {
        std::cout << "JavaScript Error: " << e.what() << "\n";
    }
}

void demonstrateValueConversion()
{
    std::cout << "\n=== Value Conversion Demo ===\n";

    try
    {
        auto context = choc::javascript::createQuickJSContext();

        // Test different value types
        context.evaluateExpression ("var testNumber = 42.5;");
        context.evaluateExpression ("var testString = 'Hello World';");
        context.evaluateExpression ("var testBoolean = true;");
        context.evaluateExpression ("var testArray = [1, 2, 3, 'four', 5.5];");
        context.evaluateExpression ("var testObject = { name: 'test', value: 123, nested: { a: 1, b: 2 } };");

        // Get and display values
        auto number = context.evaluateExpression ("testNumber");
        std::cout << "Number: " << number.getWithDefault<double>(0.0) << " (type: " << number.getType().getDescription() << ")\n";

        auto string = context.evaluateExpression ("testString");
        std::cout << "String: '" << string.toString() << "' (type: " << string.getType().getDescription() << ")\n";

        auto boolean = context.evaluateExpression ("testBoolean");
        std::cout << "Boolean: " << (boolean.getWithDefault<bool>(false) ? "true" : "false") << " (type: " << boolean.getType().getDescription() << ")\n";

        auto array = context.evaluateExpression ("testArray");
        std::cout << "Array: [";

        for (uint32_t i = 0; i < array.size(); ++i)
        {
            if (i > 0) std::cout << ", ";
            auto element = array[i];

            if (element.isString())
                std::cout << "'" << element.toString() << "'";
            else
                std::cout << element.getWithDefault<double>(0.0);
        }

        std::cout << "] (size: " << array.size() << ")\n";

        auto object = context.evaluateExpression ("testObject");
        std::cout << "Object properties:\n";

        if (object.isObject())
        {
            std::cout << "  name: " << object["name"].toString() << "\n";
            std::cout << "  value: " << object["value"].getWithDefault<double>(0.0) << "\n";
            std::cout << "  nested: [object]\n";
        }
    }
    catch (const choc::javascript::Error& e)
    {
        std::cout << "JavaScript Error: " << e.what() << "\n";
    }
}

void demonstrateAdvancedFeatures()
{
    std::cout << "\n=== Advanced Features Demo ===\n";

    try
    {
        auto context = choc::javascript::createQuickJSContext();

        // Register a function that creates and returns complex objects
        context.registerFunction ("createPoint", [](choc::javascript::ArgumentList args) -> choc::value::Value
        {
            double x = args.get<double>(0, 0.0);
            double y = args.get<double>(1, 0.0);

            auto point = choc::value::createObject ("Point");
            point.setMember ("x", choc::value::createFloat64 (x));
            point.setMember ("y", choc::value::createFloat64 (y));
            point.setMember ("distance", choc::value::createFloat64 (std::sqrt (x * x + y * y)));
            return point;
        });

        // Register a function that processes callbacks via invoke
        context.registerFunction ("processCallback", [&](choc::javascript::ArgumentList args) -> choc::value::Value
        {
            if (args.size() == 0)
                return choc::value::createString ("No callback provided");

            // For this demo, we'll just return a fixed response since callback invocation
            // would require more complex setup
            return choc::value::createString ("Callback processing completed");
        });

        // Test advanced features
        auto point = context.evaluateExpression ("createPoint (3, 4)");
        std::cout << "Created point: (" << point["x"].getWithDefault<double>(0.0)
                  << ", " << point["y"].getWithDefault<double>(0.0)
                  << ") distance = " << point["distance"].getWithDefault<double>(0.0) << "\n";

        // Test module-like functionality
        context.evaluateExpression (R"(
            var MathUtils = {
                PI: 3.14159,
                square: function (x) { return x * x; },
                cube: function (x) { return x * x * x; },
                factorial: function (n) {
                    if (n <= 1) return 1;
                    return n * this.factorial (n - 1);
                }
            };
        )");

        auto pi = context.evaluateExpression ("MathUtils.PI");
        auto square = context.evaluateExpression ("MathUtils.square (5)");
        auto cube = context.evaluateExpression ("MathUtils.cube (3)");
        auto factorial = context.evaluateExpression ("MathUtils.factorial (6)");

        std::cout << "MathUtils.PI = " << pi.getWithDefault<double>(0.0) << "\n";
        std::cout << "MathUtils.square (5) = " << square.getWithDefault<double>(0.0) << "\n";
        std::cout << "MathUtils.cube (3) = " << cube.getWithDefault<double>(0.0) << "\n";
        std::cout << "MathUtils.factorial (6) = " << factorial.getWithDefault<double>(0.0) << "\n";
    }
    catch (const choc::javascript::Error& e)
    {
        std::cout << "JavaScript Error: " << e.what() << "\n";
    }
}

int main()
{
    std::cout << "CHOC JavaScript Engine Integration Example\n";
    std::cout << "==========================================\n";

    try
    {
        demonstrateBasicJavaScript();
        demonstrateNativeFunctionBinding();
        demonstrateValueConversion();
        demonstrateAdvancedFeatures();

        std::cout << "\n=== All demonstrations completed successfully! ===\n";
    }
    catch (const std::exception& e)
    {
        std::cout << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}