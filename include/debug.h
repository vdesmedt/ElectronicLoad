#include <Arduino.h>
#define EDCL_DEBUG true

#if EDCL_DEBUG
#define DEB_BUFFER_LEN 100
#define debug_printa(msg, ...)                                            \
    do                                                                    \
    {                                                                     \
        char buffer[DEB_BUFFER_LEN];                                      \
        snprintf(buffer, DEB_BUFFER_LEN, (const char *)msg, __VA_ARGS__); \
        Serial.print(buffer);                                             \
    } while (0)
#define debug_printb(pre, msg, ...)                                       \
    do                                                                    \
    {                                                                     \
        Serial.print(micros());                                           \
        Serial.print(":");                                                \
        Serial.print(pre);                                                \
        char buffer[DEB_BUFFER_LEN];                                      \
        snprintf(buffer, DEB_BUFFER_LEN, (const char *)msg, __VA_ARGS__); \
        Serial.print(buffer);                                             \
    } while (0)
#define debug_print(msg)        \
    do                          \
    {                           \
        Serial.print(micros()); \
        Serial.print(":");      \
        Serial.print(msg);      \
    } while (0)
#else
#define debug_printa(msg, ...)
#define debug_print(msg)
#endif
