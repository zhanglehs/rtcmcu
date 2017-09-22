/**
 *
 * mms2flv_ant class.
 * @author songshenyi <songshenyi@youku.com>
 * @since  2014/04/10
 *
 */ 

#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <net/uri.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "mms2flv_ant.h"
#include "streamid.h"

using namespace std;

MMS2FLVAntConfig::MMS2FLVAntConfig()
    :AntConfig()
{
    this ->_fifo_file_prefix = "/tmp/fifo_";
}    

int MMS2FLVAntConfig::set_config(const string& req_path, int stream_n_number)
{
    URI uri;
    char* token = NULL;
    uri.parse_path(req_path);

    string stream_id = uri.get_query("dst_sid");
    if (stream_id == "")
    {
        LOG_INFO(g_logger, "MMS2FLVAntConfig::set_config parameter wrong!!! ");
        return -1;
    }
    assert(stream_id != "");

    char* stream_id_str_copy = new char[164];
    vector<string> stream_id_string_array;

    strcpy(stream_id_str_copy, stream_id.c_str());
    token = strtok(stream_id_str_copy, ",");

    int temp = 0;
    while(token != NULL)
    {
        temp++;
        stream_id_string_array.push_back(token);
        if(temp == stream_n_number)
        {
            break;
        }
        token = strtok(NULL, ",");
    }

    delete[] stream_id_str_copy;
    stream_id_str_copy = NULL;

    for(int i = 0; i < stream_n_number; i++)
    {
        this->fifo_file_name.push_back(this->_fifo_file_prefix + stream_id_string_array[i]);
        LOG_DEBUG(g_logger, "MMS2FLVAnt: set_config(), this->fifo_file_name[i] = " << this->fifo_file_name[i]);     
    }
    return 0;
}

MMS2FLVAnt::MMS2FLVAnt(std::vector<StreamId_Ext> stream_id, int stream_n_number, AdminServer *admin)
:Ant(stream_id, admin), _stream_n_number_MMS2FLVAnt(stream_n_number), _transcoder_pid(0), _input_file_fd_array(NULL)
{
    this->_config = new MMS2FLVAntConfig(); //"/tmp/fifo_";
    this->_keep_running = true;
    this->_ant_transcoder = new AntTranscoder(stream_n_number); // load()
}    

MMS2FLVAnt::~MMS2FLVAnt()
{
    delete[] this->_input_file_fd_array;
    this->_input_file_fd_array = NULL;

    delete this->_ant_transcoder;
    this->_ant_transcoder = NULL;

    delete this->_config;
    this->_config = NULL;
}

int MMS2FLVAnt::set_config(const string& req_path, int stream_n_number)
{
    this->_ant_transcoder->set_config(req_path, stream_n_number);
    this->_config->set_config(req_path, stream_n_number);
    return 0;
}

int MMS2FLVAnt::_create_stream_fifo(string fifo_file_name)
{
    string msg;
    //const char* file_name = this ->_config->fifo_file_name.c_str();
    const char* file_name = fifo_file_name.c_str();
  
    int status = access(file_name, 0);
    if(status == 0)
    {
        // fifo exist, delete it.
        msg = "";
        msg.append("MMS2FLVAnt: stream fifo ");
        msg.append(file_name);
        msg.append("exists, unlink this fifo.");
        LOG_INFO(g_logger, msg);
        int unlink_status = unlink(file_name);
        if(unlink_status < 0)
        {
            msg = "";
            msg.append("MMS2FLVAnt: stream fifo ");
            msg.append(file_name);
            msg.append("unlink failed. error: ");
            msg.append(strerror(errno));
            LOG_ERROR(g_logger, msg);
            return -1;
        }
    }

    status = mkfifo(file_name , 0666);
    if( status < 0)
    {
        msg = "";
        msg.append("MMS2FLVAnt: stream fifo ");
        msg.append(file_name);
        msg.append("create failed. error: ");
        msg.append(strerror(errno));
        LOG_ERROR(g_logger, msg);

        return -1;
    }
    return 0;
}

int MMS2FLVAnt::_ant_loop()
{
    this->_input_file_fd_array = new int32_t[_stream_n_number_MMS2FLVAnt];

    for(int i = 0; i < _stream_n_number_MMS2FLVAnt; i++)
    {
        this->_input_file_fd_array[i] = open(this->_config->fifo_file_name[i].c_str(), O_RDONLY|O_NONBLOCK);

        if(this->_input_file_fd_array[i] < 0)
        {
            LOG_ERROR(g_logger, "MMS2FLVAnt: failed open flv input file. error: " << strerror(errno));
        }
        else
        {
            LOG_INFO(g_logger, "MMS2FLVAnt: open flv input file. stream_id=" << _stream_id[i].unparse() );
        }       
    }

#ifdef DUMP_FLV
    string dump_file_name = "flv_dump.flv";

    int dump_fd = open(dump_file_name.c_str() , O_WRONLY|O_CREAT);
#endif

    const uint32_t buffer_size = 16 * 1024; 
    char** buf = new char*[_stream_n_number_MMS2FLVAnt];
    for(int i = 0; i < _stream_n_number_MMS2FLVAnt; i++)
    {
        buf[i] = new char[buffer_size];
    }
    
    uint64_t* total_size = new uint64_t[_stream_n_number_MMS2FLVAnt];
    int*  buf_cursor = new int[_stream_n_number_MMS2FLVAnt];
    int*  buf_remain = new int[_stream_n_number_MMS2FLVAnt];
    int*  total_push = new int[_stream_n_number_MMS2FLVAnt];

    int*  read_num = new int[_stream_n_number_MMS2FLVAnt];
    int*  push_num = new int[_stream_n_number_MMS2FLVAnt];

    for(int i = 0; i< _stream_n_number_MMS2FLVAnt; i++)
    {
        total_size[i] = 0;
        buf_cursor[i] = 0;
        buf_remain[i] = 0;
        total_push[i] = 0;
        read_num[i] = 0;
        push_num[i] = 0;
    }

    do
    {
        // there is still data waiting for push, push it into buffer.
        for(int i = 0; i < _stream_n_number_MMS2FLVAnt; i++)
        {
            if (buf_remain[i] > 0)
            {
                push_num[i] = this->_push_data(buf[i] + buf_cursor[i], (size_t)buf_remain[i], i);
                if (push_num[i] <= 0)
                {
                    // no data pushed, push again later;
                    LOG_WARN(g_logger, "MMS2FLVAnt: FLVAnt push data failed. stream_id=" << _stream_id[i].unparse());
                    usleep(10000);
                    goto loop_again;
                }
                else
                {
                    buf_cursor[i] += push_num[i];
                    buf_remain[i] -= push_num[i];
                    total_push[i] += push_num[i];
                }
                //continue;
            }
        }

        for(int j = 0; j < _stream_n_number_MMS2FLVAnt; j++)
        { 
            buf_cursor[j] = 0;
            buf_remain[j] = 0;

            // read from fifo, transcoder will write fifo.
            read_num[j] = read(this->_input_file_fd_array[j], buf[j], buffer_size);

            if(read_num[j] < 0)
            {
                if((errno == EAGAIN)||(errno == EWOULDBLOCK))  // read 0 bytes.
                {
                    usleep(10000);
                }
                else
                {
                    LOG_WARN(g_logger, "MMS2FLVAnt: FLV read failed. stream_id=" << _stream_id[j].unparse());
                    this->stop();
                }
                continue;
            }
            else if(read_num[j] == 0)
            {
                // check whether trancoder running.
                if (waitpid(this->_transcoder_pid, NULL, WNOHANG) == 0 )
                {
                    usleep(10000);
                }
                else
                {
                    // transcoder process exit, read_num == 0, means transcoder stopped by accident.
                    // will reboot transcoder.
                    LOG_WARN(g_logger, "MMS2FLVAnt: transcoder exit unexpectly, stop ant. stream_id: "<< _stream_id[j].unparse());
                    this->stop();
                }
                continue;
            }

            buf_remain[j] = read_num[j];
            total_size[j] += read_num[j];
            LOG_DEBUG(g_logger, "MMS2FLVAnt: total bytes read=" << total_size[j]);

#ifdef DUMP_FLV
            int dump_num = write(dump_fd, buf[j], (size_t)read_num[j]);
            if(dump_num < 0)
            {
                LOG_WARN(g_logger, "MMS2FLVAnt: FLVAnt dump data failed.");
            }
#endif
        }

        loop_again:
            ;
    }while(this->_keep_running);

    for(int i = 0; i < _stream_n_number_MMS2FLVAnt; i++)
    {
        close(this->_input_file_fd_array[i]);
    }

    delete[] total_size;
    total_size = NULL;
    
    delete[] buf_cursor;
    buf_cursor = NULL;
    
    delete[] buf_remain;
    buf_remain = NULL;
    
    delete[] total_push;
    total_push = NULL;
    
    delete[] read_num; 
    read_num = NULL;
    
    delete[] push_num;
    push_num = NULL;    
    
#ifdef DUMP_FLV
    close(dump_fd);
#endif

    // if trancoder process still running, kill it.
    this->_kill_transcoder();

    for(int i = 0; i < _stream_n_number_MMS2FLVAnt; i++)
    {
        unlink(this->_config->fifo_file_name[i].c_str());
    }

    for(int i = 0; i < _stream_n_number_MMS2FLVAnt; i++)
    {
        delete[] buf[i];
        buf[i] = NULL;
    }

    delete[] buf;
    buf = NULL;
    
    return 0;
}

void MMS2FLVAnt::run()
{
    // 1. make fifo    
    int status = 0;
    for(int i = 0; i < _stream_n_number_MMS2FLVAnt; i++)
    {
        status = this->_create_stream_fifo(this->_config->fifo_file_name[i]);
    }
    if(status < 0)
    {
        string msg = "MMS2FLVAnt: run failed";
        LOG_ERROR(g_logger, msg);
        this->_stop(true);
        return;
    }

    // 2. launch transcoder
    this->_transcoder_pid = this->_ant_transcoder->launch();
    if (this->_transcoder_pid <= 0)
    {
        //string msg = "MMS2FLVAnt: launch transcoder failed";
        //LOG_ERROR(g_logger, msg);
        LOG_ERROR(g_logger, "MMS2FLVAnt: launch transcoder failed. error: " << strerror(errno));        
        this->_stop(true);
        return;
    }

    // 3. ant loop
    this->_ant_loop();

    // 4. stop
    this->_stop(true);
}

void MMS2FLVAnt::stop()
{
    this->_keep_running = false;
}

int MMS2FLVAnt::force_exit()
{
    Ant::force_exit();
    this->_kill_transcoder();

    return 0;
}

int MMS2FLVAnt::_kill_transcoder()
{
    if (this->_transcoder_pid > 0)
    {
        if (waitpid(this->_transcoder_pid, NULL, WNOHANG) == 0)
        {
            kill(this->_transcoder_pid, SIGKILL);
            waitpid(this->_transcoder_pid, NULL, 0);
        }
    }

    return this->_transcoder_pid;
}
