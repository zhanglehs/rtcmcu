#ifndef __REPORT_STATS_H_
#define __REPORT_STATS_H_

#include <stdint.h>
#include <string>
#include "common.h"

using namespace std;

#include "report_type_defs.h"

class ReportStats
{

public:
	ReportStats();
	~ReportStats();

	static ReportStats* get_instance();

	int init(const char *path, const char * process_name,
		const char *version, const char *ip, uint16_t port);

	void fini();

	void write(const char* module_name_str, int32_t report_type_id, \
		const char* report_type_name, \
		/*int32_t range_begin, int32_t range_end, */\
		const char *format, ...);
	void check_reopen();


private:
	static ReportStats* _instance;
	int _fd;
	string _prefix_tags;
	string _version;
	string _process_name;
	string _path;

};

#define TO_VALUE_NAME(value) #value
#define TO_MODULE_NAME(module) \
	TO_VALUE_NAME(RPN_##module)

#define TO_BEGIN(module_name) RMR_##module_name##_BEGIN
#define TO_END(module_name) RMR_##module_name##_END

template <bool x> struct REPORT_TYPE_ID_ASSERTION_FAILURE;
template <> struct REPORT_TYPE_ID_ASSERTION_FAILURE<true> { enum { value = 1 }; };
template<int x> struct REPORT_TYPE_ID_ASSERT_TEST{};
#define REPORT_TYPE_ID_ASSERT(B) typedef REPORT_TYPE_ID_ASSERT_TEST<sizeof(REPORT_TYPE_ID_ASSERTION_FAILURE< (bool)(B)>)> REPORT_TYPE_ID_ASSERT_TEST_T;

#define REPORT_V2(module, report_type_id, ...) do {\
	REPORT_TYPE_ID_ASSERT(report_type_id >= (int32_t)TO_BEGIN(module) && report_type_id <= (int32_t)TO_END(module)); \
	ReportStats::get_instance()->write(TO_MODULE_NAME(module), report_type_id, TO_VALUE_NAME(report_type_id), __VA_ARGS__); \
}while (0)

#define REPORT_FORWARD_CLIENT(report_type_id, ...) \
	REPORT_V2(FORWARD_CLIENT, report_type_id, __VA_ARGS__)

#define REPORT_FORWARD_SERVER(report_type_id, ...) \
	REPORT_V2(FORWARD_SERVER, report_type_id, __VA_ARGS__)

#define REPORT_UPLOADER(report_type_id, ...) \
	REPORT_V2(UPLOADER, report_type_id, __VA_ARGS__)

#define REPORT_PLAYER(report_type_id, ...) \
	REPORT_V2(PLAYER, report_type_id, __VA_ARGS__)

#define REPORT_AVDEMUXER(report_type_id, ...) \
	REPORT_V2(AVDEMUXER, report_type_id, __VA_ARGS__)


#endif//__REPORT_STATS_H_