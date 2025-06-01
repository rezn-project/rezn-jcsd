#ifdef __cplusplus
extern "C"
{
#endif

    // Returns a freshly allocated canonical JSON string.
    // Caller must free() the result.
    const char *rezn_canonicalize(const char *json_utf8);

#ifdef __cplusplus
}
#endif
