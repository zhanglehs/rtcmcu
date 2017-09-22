/**
 * AntTranscoder class
 * @author songshenyi <songshenyi@youku.com>
 * @since  2014/04/15
 */

#ifndef __ANT_TRANSCODER_H_
#define __ANT_TRANSCODER_H_

enum AntTranscoderConfigCategory
{
    audio,
    video,
    others
};

class AntTranscoderConfigItem
{
    public:
        AntTranscoderConfigItem()
        {
        }

        AntTranscoderConfigItem(AntTranscoderConfigItem& item);

        ~AntTranscoderConfigItem()
        {
        }

    public:
        /**
         * the name of config item
         */ 
        string  config_name;

        /** 
         * whether this item default specified as an arugment
         */ 
        bool    default_specified;

        /**
         * whether this item must be specified in start/stop url.
         */ 
        bool    must_specified;

        /**
         * whether this item was specified in start/stop url.
         */ 
        bool    specified;

        /**
         * argument name. e.g.: -acodec
         */ 
        string  argument_name;

        /**
         * whether this config need values.
         */ 
        bool    need_value;

        /**
         * values.
         */ 
        vector<string>    values;

        /**
         * define a category for every config item.
         */ 
        AntTranscoderConfigCategory category;   
};

class AntTranscoderConfig
{
    public:
        AntTranscoderConfig(int stream_n_number);
        AntTranscoderConfig(AntTranscoderConfig &source_config);
        ~AntTranscoderConfig();
        char**  build_argv();
        int     load(int stream_n_number);
        
        /**
         * parse req_path, and set the values into _argv_vector.
         */
        int     set_config(const string& req_path, int stream_n_number);
        string  get_transcoder_arguments();
        string  get_transcoder_exec();
        string  get_transcoder_log_id();
        string  set_transcoder_exec(string path);

    protected:
        vector<vector<AntTranscoderConfigItem*> > _config_vector; 
        vector<string>  _argv_vector;
        char**          _argv_char;
        string          _transcoder_exec;
        string          _transcoder_log_id;
};

class AntTranscoder
{
    public:
        AntTranscoder(int stream_n_number);
        
        ~AntTranscoder(){}

        int set_config( const string& req_path, int stream_n_number);

        int launch();

    private:
        
        char** _build_argv();

    private:
        char** _argv;

        AntTranscoderConfig* _config; 
};

#endif /* __ANT_TRANSCODER_H_ */
