//
// base64.h
//
// This file came from 3Proxy.
//

#ifndef __BASE64_H__
#define __BASE64_H__

int de64 (const char *in, char *out, int maxlen);
std::string en64 (const char *in, size_t inlen);
void tohex(char *in, char *out, int len);
void fromhex(char *in, char *out, int len);

#endif
