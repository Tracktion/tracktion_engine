#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <iomanip>
#include "../choc/text/choc_HTML.h"
#include "../choc/text/choc_TextTable.h"
#include "../choc/text/choc_StringUtilities.h"
#include "../choc/text/choc_UTF8.h"
#include "../choc/text/choc_Wildcard.h"
#include "../choc/text/choc_CodePrinter.h"
#include "../choc/text/choc_FloatToString.h"

struct Employee
{
    std::string name;
    std::string department;
    int age;
    double salary;
    std::string email;
};

void demonstrateStringUtilities()
{
    std::cout << "\n=== String Utilities Demo ===\n";

    std::string text = "  Hello, World! This is a test string.  ";
    std::cout << "Original: '" << text << "'\n";

    // Trimming operations
    std::cout << "Trimmed: '" << choc::text::trim (text) << "'\n";
    std::cout << "Left trimmed: '" << choc::text::trimStart (text) << "'\n";
    std::cout << "Right trimmed: '" << choc::text::trimEnd (text) << "'\n";

    // Case operations
    std::string testCase = "Hello World";
    std::cout << "Original: " << testCase << "\n";
    std::cout << "Uppercase: " << choc::text::toUpperCase (testCase) << "\n";
    std::cout << "Lowercase: " << choc::text::toLowerCase (testCase) << "\n";

    // String manipulation
    std::string target = "The quick brown fox jumps over the lazy dog";
    std::cout << "\nString replacement demo:\n";
    std::cout << "Original: " << target << "\n";
    std::cout << "Replace 'fox' with 'cat': "
              << choc::text::replace (target, "fox", "cat") << "\n";
    std::cout << "Replace all 'the' with 'a': "
              << choc::text::replace (target, "the", "a") << "\n";

    // String testing
    std::cout << "\nString testing:\n";
    std::cout << "Contains 'quick': " << choc::text::contains (target, "quick") << "\n";
    std::cout << "Starts with 'The': " << choc::text::startsWith (target, "The") << "\n";
    std::cout << "Ends with 'dog': " << choc::text::endsWith (target, "dog") << "\n";

    // Splitting
    std::string csv = "apple,banana,cherry,date,elderberry";
    auto fruits = choc::text::splitString (csv, ',', false);
    std::cout << "\nSplit '" << csv << "' by comma:\n";
    for (size_t i = 0; i < fruits.size(); ++i)
    {
        std::cout << "  " << i << ": '" << fruits[i] << "'\n";
    }

    // Joining
    std::vector<std::string> words = {"Hello", "beautiful", "world"};
    std::cout << "Join words with ' ': " << choc::text::joinStrings (words, " ") << "\n";
    std::cout << "Join words with ' - ': " << choc::text::joinStrings (words, " - ") << "\n";
}

void demonstrateUTF8()
{
    std::cout << "\n=== UTF-8 Demo ===\n";

    std::string utf8Text = "Hello 世界! 🌍 Café naïve résumé";
    std::cout << "UTF-8 text: " << utf8Text << "\n";
    std::cout << "Byte length: " << utf8Text.length() << "\n";
    std::cout << "Character count: " << choc::text::UTF8Pointer (utf8Text.c_str()).length() << "\n";

    // Iterate through UTF-8 characters
    std::cout << "Characters:\n";
    auto utf8Ptr = choc::text::UTF8Pointer (utf8Text.c_str());
    int index = 0;
    while (! utf8Ptr.empty())
    {
        auto codepoint = *utf8Ptr;
        std::cout << "  " << index++ << ": U+" << std::hex << std::uppercase
                  << codepoint << std::dec << "\n";
        ++utf8Ptr;
    }

    // UTF-8 validation
    std::string validUTF8 = "Valid UTF-8: Hello";
    std::string invalidUTF8 = "Invalid UTF-8: \xFF\xFE";
    std::cout << "\nValidation:\n";
    std::cout << "'" << validUTF8 << "' is valid: "
              << (choc::text::findInvalidUTF8Data (validUTF8.data(), validUTF8.size()) == nullptr) << "\n";
    std::cout << "Invalid sequence is valid: "
              << (choc::text::findInvalidUTF8Data (invalidUTF8.data(), invalidUTF8.size()) == nullptr) << "\n";
}

void demonstrateWildcardMatching()
{
    std::cout << "\n=== Wildcard Matching Demo ===\n";

    std::vector<std::string> filenames = {
        "document.txt", "image.png", "video.mp4", "archive.zip",
        "source.cpp", "header.h", "readme.md", "test_file.txt"
    };

    std::vector<std::string> patterns = {
        "*.txt", "*.png", "test_*", "*.*", "*.{cpp,h}", "doc*"
    };

    for (const auto& pattern : patterns)
    {
        std::cout << "\nPattern '" << pattern << "' matches:\n";
        choc::text::WildcardPattern matcher (pattern, true);

        for (const auto& filename : filenames)
            if (matcher.matches (filename))
                std::cout << "  " << filename << "\n";
    }
}

void demonstrateTextTable()
{
    std::cout << "\n=== Text Table Demo ===\n";

    // Create sample employee data
    std::vector<Employee> employees = {
        {"Alice Johnson", "Engineering", 28, 75000.0, "alice@company.com"},
        {"Bob Smith", "Marketing", 35, 65000.0, "bob@company.com"},
        {"Carol Williams", "Engineering", 31, 82000.0, "carol@company.com"},
        {"David Brown", "Sales", 29, 58000.0, "david@company.com"},
        {"Eve Davis", "HR", 42, 70000.0, "eve@company.com"}
    };

    choc::text::TextTable table;

    // Add header row
    table << "Name" << "Department" << "Age" << "Salary" << "Email";
    table.newRow();

    // Add data rows
    for (const auto& emp : employees)
    {
        table << emp.name
              << emp.department
              << std::to_string (emp.age)
              << ("$" + choc::text::floatToString (emp.salary, 0))
              << emp.email;

        table.newRow();
    }

    std::cout << "Employee Table:\n";
    std::cout << table.toString ("| ", " | ", " |\n");

    // Create a summary table
    choc::text::TextTable summary;
    std::map<std::string, std::vector<double>> deptSalaries;

    // Group salaries by department
    for (const auto& emp : employees)
        deptSalaries[emp.department].push_back (emp.salary);

    summary << "Department" << "Employees" << "Avg Salary" << "Total Salary";
    summary.newRow();

    for (const auto& [dept, salaries] : deptSalaries)
    {
        double total = 0.0;

        for (double salary : salaries)
            total += salary;

        double average = total / salaries.size();

        summary << dept
                << std::to_string (salaries.size())
                << ("$" + choc::text::floatToString (average, 0))
                << ("$" + choc::text::floatToString (total, 0));

        summary.newRow();
    }

    std::cout << "\nDepartment Summary:\n";
    std::cout << summary.toString ("| ", " | ", " |\n");
}

void demonstrateHTMLGeneration()
{
    std::cout << "\n=== HTML Generation Demo ===\n";

    // Create an HTML document
    choc::html::HTMLElement doc ("html");

    // Add head section
    auto& head = doc.addChild ("head");
    head.addChild ("title").addContent ("Employee Report");
    head.addChild ("meta").setProperty ("charset", "UTF-8");

    // Add some CSS
    auto& style = head.addChild ("style");
    style.addRawContent (R"(
        body { font-family: Arial, sans-serif; margin: 20px; }
        table { border-collapse: collapse; width: 100%; margin: 20px 0; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }
        th { background-color: #f2f2f2; font-weight: bold; }
        .header { color: #333; border-bottom: 2px solid #4CAF50; }
        .summary { background-color: #f9f9f9; padding: 15px; border-radius: 5px; }
        .highlight { background-color: #ffffcc; }
    )");

    // Add body section
    auto& body = doc.addChild ("body");

    // Add header
    body.addChild ("h1").setClass ("header").addContent ("Company Employee Report");
    body.addChild ("p").addContent ("Generated on " + std::string (__DATE__) + " at " + std::string (__TIME__));

    // Add employee table
    body.addChild ("h2").addContent ("Employee Directory");
    auto& table = body.addChild ("table");

    // Table header
    auto& thead = table.addChild ("thead");
    auto& headerRow = thead.addChild ("tr");
    headerRow.addChild ("th").addContent ("Name");
    headerRow.addChild ("th").addContent ("Department");
    headerRow.addChild ("th").addContent ("Age");
    headerRow.addChild ("th").addContent ("Salary");
    headerRow.addChild ("th").addContent ("Email");

    // Table body
    auto& tbody = table.addChild ("tbody");

    std::vector<Employee> employees = {
        {"Alice Johnson", "Engineering", 28, 75000.0, "alice@company.com"},
        {"Bob Smith", "Marketing", 35, 65000.0, "bob@company.com"},
        {"Carol Williams", "Engineering", 31, 82000.0, "carol@company.com"},
        {"David Brown", "Sales", 29, 58000.0, "david@company.com"},
        {"Eve Davis", "HR", 42, 70000.0, "eve@company.com"}
    };

    for (const auto& emp : employees)
    {
        auto& row = tbody.addChild ("tr");
        if (emp.salary > 75000)
            row.setClass ("highlight");

        row.addChild ("td").addContent (emp.name);
        row.addChild ("td").addContent (emp.department);
        row.addChild ("td").addContent (std::to_string (emp.age));
        row.addChild ("td").addContent ("$" + choc::text::floatToString (emp.salary, 0));

        // Make email a clickable link
        auto& emailCell = row.addChild ("td");
        emailCell.addLink ("mailto:" + emp.email).addContent (emp.email);
    }

    // Add summary section
    body.addChild ("h2").addContent ("Summary Statistics");
    auto& summaryDiv = body.addDiv ("summary");

    double totalSalary = 0.0;
    double maxSalary = 0.0;
    double minSalary = std::numeric_limits<double>::max();

    for (const auto& emp : employees)
    {
        totalSalary += emp.salary;
        maxSalary = std::max (maxSalary, emp.salary);
        minSalary = std::min (minSalary, emp.salary);
    }

    double avgSalary = totalSalary / employees.size();

    summaryDiv.addChild ("p").addContent ("Total Employees: " + std::to_string (employees.size()));
    summaryDiv.addChild ("p").addContent ("Average Salary: $" + choc::text::floatToString (avgSalary, 2));
    summaryDiv.addChild ("p").addContent ("Highest Salary: $" + choc::text::floatToString (maxSalary, 0));
    summaryDiv.addChild ("p").addContent ("Lowest Salary: $" + choc::text::floatToString (minSalary, 0));

    // Add footer
    auto& footer = body.addChild ("div");
    footer.addChild ("hr");
    footer.addChild ("p").setInline (true)
          .addContent ("Report generated by ")
          .addSpan ("highlight").addContent ("CHOC Text Processing Example")
          .addContent (" - ")
          .addLink ("https://github.com/Tracktion/choc").addContent ("Learn more about CHOC");

    // Generate the HTML
    std::string htmlContent = doc.toDocument (true);

    // Write to file
    std::ofstream htmlFile ("employee_report.html");

    if (htmlFile.is_open())
    {
        htmlFile << htmlContent;
        htmlFile.close();
        std::cout << "HTML report generated: employee_report.html\n";
    }

    // Show a snippet of the generated HTML
    std::cout << "\nGenerated HTML (first 500 characters):\n";
    std::cout << htmlContent.substr (0, 500) << "...\n";
}

void demonstrateCodePrinter()
{
    std::cout << "\n=== Code Printer Demo ===\n";

    choc::text::CodePrinter printer;

    // Generate some C++ code
    printer << "// Auto-generated employee data structure\n";
    printer << "#include <string>\n";
    printer << "#include <vector>\n\n";

    printer << "namespace Company" << choc::text::CodePrinter::NewLine();

    {
        auto ns_indent = printer.createIndentWithBraces();

        printer << "struct Employee" << choc::text::CodePrinter::NewLine();

        {
            auto struct_indent = printer.createIndentWithBraces();

            printer << "std::string name;" << choc::text::CodePrinter::NewLine();
            printer << "std::string department;" << choc::text::CodePrinter::NewLine();
            printer << "int age;" << choc::text::CodePrinter::NewLine();
            printer << "double salary;" << choc::text::CodePrinter::NewLine();
            printer << "std::string email;" << choc::text::CodePrinter::BlankLine();

            printer << "Employee (const std::string& n, const std::string& d, int a, double s, const std::string& e)" << choc::text::CodePrinter::NewLine();

            {
                auto ctor_indent = printer.createIndent();
                printer << ": name (n), department (d), age (a), salary (s), email (e) {}" << choc::text::CodePrinter::NewLine();
            }
        }
        printer << choc::text::CodePrinter::BlankLine();

        printer << "std::vector<Employee> getEmployees()" << choc::text::CodePrinter::NewLine();

        {
            auto func_indent = printer.createIndentWithBraces();

            printer << "return" << choc::text::CodePrinter::NewLine();

            {
                auto return_indent = printer.createIndentWithBraces();

                std::vector<Employee> employees =
                {
                    {"Alice Johnson", "Engineering", 28, 75000.0, "alice@company.com"},
                    {"Bob Smith", "Marketing", 35, 65000.0, "bob@company.com"},
                    {"Carol Williams", "Engineering", 31, 82000.0, "carol@company.com"}
                };

                for (size_t i = 0; i < employees.size(); ++i)
                {
                    const auto& emp = employees[i];

                    printer << "Employee (\"" << emp.name << "\", \"" << emp.department
                            << "\", " << emp.age << ", " << emp.salary << ", \"" << emp.email << "\")";

                    if (i < employees.size() - 1)
                        printer << ",";

                    printer << choc::text::CodePrinter::NewLine();
                }
            }

            printer << ";" << choc::text::CodePrinter::NewLine();
        }
    }

    printer << choc::text::CodePrinter::NewLine() << "} // namespace Company" << choc::text::CodePrinter::NewLine();

    auto generatedCode = printer.toString();

    std::cout << "Generated C++ code:\n";
    std::cout << generatedCode << "\n";

    // Write to file
    std::ofstream cppFile ("generated_employees.cpp");

    if (cppFile.is_open())
    {
        cppFile << generatedCode;
        cppFile.close();
        std::cout << "C++ code written to: generated_employees.cpp\n";
    }
}

void demonstrateFloatToString()
{
    std::cout << "\n=== Float to String Demo ===\n";

    std::vector<double> testValues = {
        3.14159265359, 0.000001, 1000000.0, -42.5, 0.0,
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN()
    };

    std::cout << "Value              | Default    | 2 digits   | 6 digits   | Scientific\n";
    std::cout << "-------------------|------------|------------|------------|------------\n";

    for (double value : testValues)
    {
        std::cout << std::setw (18) << std::left << value << " | "
                  << std::setw (10) << choc::text::floatToString (value) << " | "
                  << std::setw (10) << choc::text::floatToString (value, 2) << " | "
                  << std::setw (10) << choc::text::floatToString (value, 6) << " | "
                  << choc::text::floatToString (value, 3, true) << "\n";
    }
}

int main()
{
    std::cout << "CHOC Text Processing & HTML Generation Example\n";
    std::cout << "==============================================\n";

    try
    {
        demonstrateStringUtilities();
        demonstrateUTF8();
        demonstrateWildcardMatching();
        demonstrateTextTable();
        demonstrateHTMLGeneration();
        demonstrateCodePrinter();
        demonstrateFloatToString();

        std::cout << "\n=== All text processing demonstrations completed successfully! ===\n";
        std::cout << "Generated files:\n";
        std::cout << "  - employee_report.html (HTML report)\n";
        std::cout << "  - generated_employees.cpp (C++ code)\n";
    }
    catch (const std::exception& e)
    {
        std::cout << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}