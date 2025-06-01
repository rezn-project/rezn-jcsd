#include "json_canonicalizer.hpp"
#include <string>
#include <cstring>
#include <cstdlib>

/**
 * Canonicalizes UTF-8 encoded JSON string according to RFC 8785.
 *
 * @param json_utf8 Input JSON string (must not be null)
 * @return Canonicalized JSON string allocated with malloc(),
 *         or nullptr on error. Caller must free() the returned pointer.
 */
extern "C" const char *rezn_canonicalize(const char *json_utf8)
{
    try
    {
        if (json_utf8 == nullptr)
        {
            return nullptr;
        }
        std::string canon = jsoncanonicalizer::canonicalize(json_utf8);
        char *result = (char *)malloc(canon.size() + 1);
        if (result == nullptr)
        {
            return nullptr;
        }
        std::memcpy(result, canon.c_str(), canon.size() + 1);
        return result;
    }
    catch (...)
    {
        return nullptr;
    }
}

extern "C" void rezn_free(char *ptr)
{
    free(ptr);
}