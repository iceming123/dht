#pragma once

void reg_algs(void);
bool sign(const unsigned char* ip, int ilen, unsigned char* out, unsigned long clen, const char* fprvkey);
bool verify(const unsigned char* cip, int clen, const unsigned char* ip, int ilen, const char* fpubkey);
bool make_key(const char* fpubkey, const char* fprvkey);