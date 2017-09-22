#ifndef CRC32_H_
#define CRC32_H_

#if (defined __cplusplus && !defined _WIN32)
extern "C"
{
#endif
unsigned int generate_crc32c(const char *buffer, int len);

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif
