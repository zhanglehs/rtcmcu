/**
 *
 * mms2flv_ant class.
 * @author songshenyi <songshenyi@youku.com>
 * @since  2014/04/10
 *
 */ 

#ifndef __MMS2FLV_ANT_H_
#define __MMS2FLV_ANT_H_

#include <string>
#include <sys/wait.h>

#include <utils/buffer_queue.h>
#include "global_var.h"
#include "ant.h"
#include "ant_transcoder.h"
#include "streamid.h"

using namespace std;

class MMS2FLVAntConfig: public AntConfig
{
    public:
        MMS2FLVAntConfig();

        int set_config(const string& req_path, int stream_n_number);

    public:
        vector<string> fifo_file_name;

    protected:
        string  _fifo_file_prefix;        
};

/**
 * FLV Ant.
 */ 
class MMS2FLVAnt: public Ant
{
    public:
        /**
         * constructor
         * @param stream_id stream id.
         * @param admin     AdminServer.
         */ 
        MMS2FLVAnt(std::vector<StreamId_Ext>  stream_id, int stream_n_number, AdminServer *admin);

        virtual ~MMS2FLVAnt();
        
        virtual int set_config(const string& req_path, int stream_n_number);

        virtual void run();

        virtual void stop();

        virtual int force_exit();

    protected:
        int _ant_loop();    

        int _create_stream_fifo(string fifo_file_name);

        int _kill_transcoder();

    protected:
        AntTranscoder   *_ant_transcoder;

    private:
        
        int32_t*        _input_file_fd_array;
        
        bool            _keep_running;

        MMS2FLVAntConfig*   _config;

        int             _transcoder_pid;
        int             _stream_n_number_MMS2FLVAnt;
};


#endif /* __MMS2FLV_ANT_H_ */
