#include "json_canonicalizer.hpp"
#include <string>
#include <cstring>
#include <cstdlib>

extern "C" const char *rezn_canonicalize(const char *json_utf8)
{
    try
    {
        std::string canon = jsoncanonicalizer::canonicalize(json_utf8);
        char *result = (char *)malloc(canon.size() + 1);
        std::memcpy(result, canon.c_str(), canon.size() + 1);
        return result;
    }
    catch (...)
    {
        return nullptr;
    }
}
