#include <iostream>
#include <limits>
#include <string>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include "json_canonicalizer.hpp"

namespace fs = std::filesystem;

std::string read_file(const fs::path &path)
{
    std::ifstream in(path);
    if (!in)
        throw std::runtime_error("Failed to open file: " + path.string());
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

int main()
{
    fs::path input_dir = "./fixtures/input";
    fs::path output_dir = "./fixtures/output";
    int failed = 0, total = 0;

    for (const auto &entry : fs::directory_iterator(input_dir))
    {
        if (!entry.is_regular_file())
            continue;
        fs::path input_path = entry.path();
        fs::path output_path = output_dir / input_path.filename();

        try
        {
            std::string input_json = read_file(input_path);
            std::string expected_output = read_file(output_path);

            std::string canonical = jsoncanonicalizer::canonicalize(input_json);

            if (canonical != expected_output)
            {
                std::cerr << "Test failed for " << input_path.filename() << ":\n"
                          << "Expected: " << expected_output << "\n"
                          << "Got:      " << canonical << "\n";
                ++failed;
            }
            else
            {
                std::cout << "Test passed for " << input_path.filename() << std::endl;

                std::cout << "Input JSON: " << input_json << std::endl;
                std::cout << "Canonical JSON: " << canonical << std::endl;
                std::cout << "Expected Output: " << expected_output << std::endl;
            }
            ++total;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error with " << input_path.filename() << ": " << e.what() << std::endl;
            ++failed;
            ++total;
        }
    }

    std::cout << (total - failed) << "/" << total << " tests passed." << std::endl;
    return failed ? 1 : 0;
}
