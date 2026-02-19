#include <iostream>

#include "../choc/text/choc_JSON.h"
#include "../choc/containers/choc_Value.h"

int main()
{
    // Create a choc::value::Value object
    auto address = choc::json::create ("street", "123 Main St",
                                       "city", "Anytown");

    auto person = choc::json::create ("name", "John Doe",
                                      "age", 30,
                                      "isStudent", false,
                                      "address", address);

    // Convert the choc::value::Value object to a JSON string
    auto jsonString = choc::json::toString (person);
    std::cout << "Generated JSON:\n" << jsonString << std::endl;

    // Parse the JSON string back into a choc::value::Value object
    auto parsedValueHolder = choc::json::parse (jsonString);
    auto parsedValue = parsedValueHolder.getView();

    // Access the data from the parsed object
    std::cout << "\nParsed JSON data:\n";
    std::cout << "Name: " << parsedValue["name"].get<std::string>() << std::endl;
    std::cout << "Age: " << parsedValue["age"].get<int>() << std::endl;
    std::cout << "Is Student: " << (parsedValue["isStudent"].get<bool>() ? "true" : "false") << std::endl;
    std::cout << "Address: " << parsedValue["address"]["street"].get<std::string>() << ", " << parsedValue["address"]["city"].get<std::string>() << std::endl;

    return 0;
}
