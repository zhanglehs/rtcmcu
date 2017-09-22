/**
 * \file
 * \brief md5 Ëã·¨
 */
#ifndef _MD5_HPP
#define _MD5_HPP

#include <iostream>
#include <string>
#include <cstring>
using namespace std;

class md5
{
public:
    md5();
    md5(const string &);
    md5(const char *, unsigned int);
    void init();
    void update(const string &);
    void update(unsigned char const *, unsigned int);
    void final();
    string hash(const string &);
	string hash_binary(const string &);
    string hash(const char *, unsigned int);
	string hash_binary(const char *, unsigned int);
    string out();
	string out_binary();
    char x_char(unsigned char);
    void echo();

	uint64_t hash64(const char * str,int len);
	uint32_t hash32(const char * str,int len);
	int d_value(char c);

private:
    void reverse(unsigned char *, unsigned int);
    void transform(unsigned int [4], unsigned int const [16]);

private:
    unsigned int  buf[4];
    unsigned int  bits[2];
    unsigned char in[64];
};

#endif
