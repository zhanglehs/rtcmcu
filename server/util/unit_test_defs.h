/**
* @file unit_test_defs.h
* @brief define some basic macros for unit tests
* @author songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/06/23
*/

#ifndef __UNIT_TEST_DEFS_H__
#define __UNIT_TEST_DEFS_H__

#include <string>

std::string methodName(const std::string pretty_function)
{
	size_t begin = pretty_function.rfind("::") + 2;
	size_t end = pretty_function.rfind("(") - begin;
	return pretty_function.substr(begin, end) + "()";
}

#define __METHOD_NAME__ methodName(__PRETTY_FUNCTION__)

std::string className(const std::string pretty_function)
{
	size_t end = pretty_function.rfind("::") - 4;
	size_t begin = pretty_function.substr(0, end).rfind(" ") + 1;
	return pretty_function.substr(begin, end);
}

#define __CLASS_NAME__ className(__PRETTY_FUNCTION__)



#endif  /* __UNIT_TEST_DEFS_H__ */
