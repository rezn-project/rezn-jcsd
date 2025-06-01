#include <iostream>
#include <string>
#include <stdexcept>
#include "json_canonicalizer.hpp"

int main()
{
    std::string input = R"({ "b": 2, "a": 1 })";
    try
    {
        std::string canonical = jsoncanonicalizer::Transform(input);
        std::cout << canonical << std::endl; // → {"a":1,"b":2}
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed: " << e.what() << std::endl;
    }
}
