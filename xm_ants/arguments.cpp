/*
 *
 */

#include <cstring>
#include "arguments.h"

void Arguments::parse(int argc, char *argv[])
{
    char* ptr = strrchr(argv[0], '/');
    if (ptr == NULL) 
    {   
        _program_name = argv[0];
    }   
    else 
    {   
        _program_name = (ptr + 1); 
    }   

    for (int i = 1; i < argc ; i++)
    {
        if (strlen(argv[i]) <= 2)   // strlen("==")
        {
            continue;
        }
        const char * s = argv[i] + 2;
        const char * e = strchr(s,'=');
        if (!e)
        {
            _map[std::string(s)] = "";
        }
        else
        {
            _map[std::string(s, e-s)] = std::string(e+1);
        }

    }
}

