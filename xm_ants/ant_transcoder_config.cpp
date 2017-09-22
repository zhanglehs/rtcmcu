/**
 * AntTranscoder class
 * @author songshenyi <songshenyi@youku.com>
 * @since  2014/04/15
 */

#include <net/uri.h>
#include "global_var.h"
#include "ant_transcoder.h"

#include <cstring>
#include <iostream>
#include <fstream>
#include <json-c++/json.h>

using namespace std;

string ant_transcoder_config_file_list[] =
{
    "ant_transcoder_config_1.json",
    "ant_transcoder_config_2.json",
    "ant_transcoder_config_3.json",
    "ant_transcoder_config_4.json",
    "ant_transcoder_config_5.json",
};

AntTranscoderConfig::AntTranscoderConfig(int stream_n_number)
{
    this->_argv_char = NULL;
    this->load(stream_n_number);
}

AntTranscoderConfig::AntTranscoderConfig(AntTranscoderConfig &source_config)
{
    this->_argv_char = NULL;
    int config_vector_size = source_config._config_vector[0].size();
    
    for( int i = 0; i < config_vector_size; i++)
    {
        AntTranscoderConfigItem *item = new AntTranscoderConfigItem(*(source_config._config_vector[0][i]));
        this->_config_vector[0].push_back(item);
    }
    this->_transcoder_exec = source_config._transcoder_exec;
}

int AntTranscoderConfig::load(int stream_n_number)
{
    // TODO: load config from config file.

    this->_config_vector.resize(stream_n_number);

    for(int k = 0; k < stream_n_number; k++)
    {
        LOG_ERROR(g_logger, "AntTranscoderConfig: load(). file name: " << ant_transcoder_config_file_list[k]);
        // config file stream.
        ifstream    ifs;
        string      config_file_name = ant_transcoder_config_file_list[k];

        ifs.open(("config/" + config_file_name).c_str());

        if( ! ifs.is_open())
        {
            LOG_ERROR(g_logger, "AntTranscoderConfig: open config file error. file name: "<<config_file_name);
            return -1;
        }
        
        Json::Reader    reader;
        Json::Value     root;
        if( ! reader.parse(ifs, root, false))
        {
            LOG_ERROR(g_logger, "AntTranscoderConfig: load config file error. file name: "<<config_file_name);
            ifs.close();
            return -1;
        }

        string config_type = root["config_type"].asString();
        if ( config_type != "ant_transcoder")
        {
            LOG_ERROR(g_logger, "AntTranscoderConfig: config type error. file name: "<<config_file_name);
            ifs.close();
            return -1;
        }

        this->_transcoder_exec = root["config_value"]["transcoder_exec"].asString(); //"bin/ffmpeg"
        
        Json::Value arguments = root["config_value"]["arguments"];
        int size = arguments.size(); // size == 13

        //this->_config_vector[k].resize(size);
        this->_config_vector[k].clear();
        
        AntTranscoderConfigItem *item;
        
        for(int i = 0; i < size; i++)
        {
            item = new AntTranscoderConfigItem();
            item->config_name = arguments[i]["config_name"].asString();
            item->default_specified = arguments[i]["default_specified"].asBool();
            string test = arguments[i]["must_specified"].asString();
            item->must_specified = arguments[i]["must_specified"].asBool();
            item->argument_name = arguments[i]["argument_name"].asString();
            item->need_value = arguments[i]["need_value"].asBool();
            int value_count = arguments[i]["default_value"].size();

            for( int j = 0; j < value_count; j++)
            {
                item->values.push_back(arguments[i]["default_value"][j].asString() );
            }

            this->_config_vector[k].push_back(item);
        }

        if (0 != k) // if not the first config file, then erase the first parameter in the vector.(-i src url)
        {
            this->_config_vector[k].erase(this->_config_vector[k].begin());
        }

        ifs.close();
    }
    return 0;
}

int AntTranscoderConfig::set_config(const string& req_path, int stream_n_number)
{
    URI uri;
    uri.parse_path(req_path);
    char* token = NULL;

    string stream_id = uri.get_query("dst_sid");
    if (stream_id == "")
    {
        LOG_INFO(g_logger, "AntTranscoderConfig::set_config parameter wrong!!! ");
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

    //the first stream sid will be used in transcoder log file name.
    if (1 == stream_n_number)
    {
        this->_transcoder_log_id = stream_id_string_array[0];
    }
    else
    {
        stringstream ss; 
        ss << stream_n_number;
        this->_transcoder_log_id = stream_id_string_array[0] + "_" + ss.str() + "streams";
    }

    int return_num = 0; 
    vector<string> argv_vec;
    vector<AntTranscoderConfigItem*> config_vector;
    
    for(int i = 0; i < stream_n_number; i++)
    {
        if (0 == i)
        {
            argv_vec.push_back(this->get_transcoder_exec());
        }
        config_vector = this->_config_vector[i];

        vector<AntTranscoderConfigItem*>::iterator it;
        for(it = config_vector.begin();
            it != config_vector.end();
            it++)
        {
            // need to build fifo name for output
            if((*it)->config_name == "output")
            {
                string stream_id_sub = stream_id_string_array[i];
                string fifo_name = "/tmp/fifo_" + stream_id_sub;
                argv_vec.push_back(fifo_name);
                continue;
            }

            string query = uri.get_query((*it)->config_name);
            
            if( query == "" )
            {
                // if this field must specify a value, this is an error.
                if((*it)->must_specified == true)
                {
                    LOG_ERROR(g_logger, "AntTranscoderConfig: you must specify " + (*it)->config_name);
                    return_num = -1;
                    continue;
                }
                // else use default values
                else
                {
                    (*it)->specified = false;
                    if((*it)->default_specified == true)
                    {
                        argv_vec.push_back((*it)->argument_name);
                        if((*it)->need_value)
                        {
                            argv_vec.insert(argv_vec.end(), (*it)->values.begin(), (*it)->values.end());
                        }
                    }
                }
            }
            else
            {
                if( (*it)->config_name == "src" || (*it)->config_name == "dst_format" )
                {
                    (*it)->specified = true;
                    argv_vec.push_back((*it)->argument_name);
                    if((*it)->need_value)
                    {
                        argv_vec.push_back(query);
                    }
                }
                else
                {
                    char* token = NULL;
                    char* query_str_copy = new char[164];
                    vector<string> query_string_array;

                    strcpy(query_str_copy, query.c_str());
                    token = strtok(query_str_copy, ",");

                    int temp = 0;
                    while(token != NULL)
                    {
                        temp++;
                        query_string_array.push_back(token);
                        if(temp == stream_n_number)
                        {
                            break;
                        }
                        token = strtok(NULL, ",");
                    }

                    argv_vec.push_back((*it)->argument_name);
                    if((*it)->need_value)
                    {
                        if (query_string_array[i] == "default")
                        {
                            (*it)->specified = false;
                            argv_vec.insert(argv_vec.end(), (*it)->values.begin(), (*it)->values.end());
                        }
                        else
                        {
                            (*it)->specified = true;
                            argv_vec.push_back(query_string_array[i]);
                        }
                    }

                    delete[] query_str_copy;
                    query_str_copy = NULL;
                }
            }
        }
    
    }


    if(return_num != -1)
    {
        //this -> _argv_vector = argv_vec;
        this->_argv_vector.insert(this->_argv_vector.end(), argv_vec.begin(), argv_vec.end());
    }

    return return_num;
}

char** AntTranscoderConfig::build_argv()
{
    int n = this->_argv_vector.size();
    this->_argv_char = new char*[n+1];
    for(int i = 0; i < n; i++)
    {
        int m = this->_argv_vector[i].length() + 1;
        this->_argv_char[i] = new char[m];
        strcpy(this->_argv_char[i], this->_argv_vector[i].c_str());
    }
    this->_argv_char[n] = NULL;
    return this->_argv_char;
}

string AntTranscoderConfig::get_transcoder_arguments()
{
    vector<string>::iterator it;
    string arguments;
    for(it = this->_argv_vector.begin();
        it != this->_argv_vector.end();
        it++)
    {
        arguments.append(" ");
        arguments.append((*it));
    }    
    return arguments;
}

string AntTranscoderConfig::get_transcoder_exec()
{
    return this->_transcoder_exec;
}

string AntTranscoderConfig::get_transcoder_log_id()
{
    return this->_transcoder_log_id;
}

AntTranscoderConfig::~AntTranscoderConfig()
{
    int n = this -> _argv_vector.size();
    
    // TODO: destructor for _argv_vector.
    
    if(this->_argv_char != NULL)
    {
        for(int i = 0; i < n+1; i++)
        {
            if(this->_argv_char[i] != NULL)
            {
                delete[] this->_argv_char[i];
                this->_argv_char[i] = NULL;
            }
        }
        delete[] this->_argv_char;
        this->_argv_char = NULL;
    }
}

