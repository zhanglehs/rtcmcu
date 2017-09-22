#ifndef  UTILITY_INC
#define  UTILITY_INC

#include <string>

namespace Utility
{
    /// \name 时间函数
    //@{

    /**
     * \brief 将当前时间字符串按照 fmt指定的格式打印到buf里
     * \return buf
     * \see man strftime
     */
    char * get_cur_time_formated(char * buf, size_t buf_size, const char * fmt);
    
    /**
     * \brief 返回"0000-00-00 00:00:00"格式的时间字符串
     */
    std::string get_standard_cur_timestr();

    std::string get_cur_4y2m2d();
    std::string get_cur_4y2m2d2h2m();
    std::string get_cur_4y2m2d2h2m2s();

    //@}
};

#endif   /* ----- #ifndef UTILITY_INC  ----- */

