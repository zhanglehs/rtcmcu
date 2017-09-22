/**
 * AntTranscoder class
 * @author songshenyi <songshenyi@youku.com>
 * @since  2014/04/15
 */

#include <net/uri.h>
#include "global_var.h"
#include "ant_transcoder.h"
#include <unistd.h>


AntTranscoder::AntTranscoder(int stream_n_number)
{
    this->_config = new AntTranscoderConfig(stream_n_number); //load()
}

char** AntTranscoder::_build_argv()
{
    return this->_config->build_argv();
}

int AntTranscoder::set_config(const string& req_path, int stream_n_number)
{
    return this->_config->set_config(req_path,stream_n_number);
}

int AntTranscoder::launch()
{
    char** argv_char = NULL;
    argv_char = this->_build_argv();
    if(argv_char == NULL)
    {
        LOG_ERROR(g_logger, "AntTranscoder: argv_char is NULL, exit.");
        return -1;
    }

    time_t rawtime;
    struct tm * timeinfo;
    char buffer[25];

    time(&rawtime);
    timeinfo = localtime (&rawtime);
    strftime(buffer,25,"%F_%H-%M-%S", timeinfo);

    string transcoder_exec = this->_config->get_transcoder_exec();
    string log_ffmpeg_path = g_config_info.log_ffmpeg_path;
    string transcoder_log_id = this->_config->get_transcoder_log_id();
    string transcoder_log = "file=" + log_ffmpeg_path + "/ffmpeg_sid_" + transcoder_log_id + "_" + string(buffer) + ".log";

    LOG_INFO(g_logger, "AntTranscoder: executable file " + transcoder_exec);
    LOG_INFO(g_logger, "AntTranscoder: transcoder_log " + transcoder_log);
    LOG_INFO(g_logger, "AntTranscoder: execution arguments " + this->_config->get_transcoder_arguments());

    int pid = fork();
    setenv("FFREPORT", transcoder_log.c_str(), 1);

    if(pid == 0)
    {
        int exec_num = execv(transcoder_exec.c_str(), argv_char);
        if(exec_num < 0)
        {
            LOG_ERROR(g_logger, "AntTranscoder: transcoder exec failed.");
            exit(1);
        }
    }

    return pid;
}
