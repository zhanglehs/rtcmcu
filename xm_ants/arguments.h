/*
 *
 *
 */

#ifndef __ARGUMENTS_H_
#define __ARGUMENTS_H_

#include <string>
#include <map>

class Arguments 
{
    public:
        Arguments(){}

        std::string get_program_name() const { return _program_name;}
        std::string get_value(std::string param) { return _map[param];}
        bool is_defined(std::string param) { return _map.find(param) != _map.end();}
        void parse(int argc, char *argv[]);
        
    private:

    private:
        std::string _program_name;
        std::map<std::string, std::string> _map;
};


#endif

