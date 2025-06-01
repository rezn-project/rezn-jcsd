#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <stdexcept>

#include "json_canonicalizer.hpp"

std::string read_input(std::istream &in)
{
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

int main(int argc, char *argv[])
{
    try
    {
        std::string input;

        if (argc == 1)
        {
            input = read_input(std::cin);
        }
        else if (argc == 2)
        {
            std::ifstream file(argv[1]);
            if (!file)
            {
                std::cerr << "Failed to open file: " << argv[1] << std::endl;
                return 1;
            }
            input = read_input(file);
        }
        else
        {
            std::cerr << "Usage: " << argv[0] << " [input.json]" << std::endl;
            return 1;
        }

        std::string output = jsoncanonicalizer::canonicalize(input);
        std::cout << output << std::endl;
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << std::endl;
        return 1;
    }
}
