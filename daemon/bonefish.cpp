/**
 *  Copyright (C) 2015 Topology LP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include "daemon.hpp"
#include "daemon_options.hpp"

#include <cstdint>
#include <iostream>
#include <algorithm>

namespace {

char* get_option(char **begin, char **end, const std::string &opt)
{
    char **it = std::find(begin, end, opt);
    if (it != end && ++it != end)
    {
        return *it;
    }
    return nullptr;
}

bool check_option_present(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

const std::string help_param_name{"--help"};
const std::string websocket_port_name1{"-w"};
const std::string websocket_port_name2{"--websocket-port"};
const std::string rawsocket_port_name1{"-t"};
const std::string rawsocket_port_name2{"--rawsocket-port"};
const std::string rawsocket_path_name1{"-u"};
const std::string rawsocket_path_name2{"--rawsocket-path"};
const std::string realm_name1{"-r"};
const std::string realm_name2{"--realm"};
const std::string dis_json_ser_name{"--no-json"};
const std::string dis_msgpack_ser_name{"--no-msgpack"};
const std::string debug_name1{"-d"};
const std::string debug_name2{"--debug"};

}


int main(int argc, char** argv)
{
    bonefish::daemon_options options;

    try{
        if(check_option_present(argv, argv+argc, help_param_name)){
            std::cout
            <<"Allowed options:"<<std::endl
            <<(help_param_name+ "    produce help message")<<std::endl
            <<(websocket_port_name1 + ", " + websocket_port_name2 + "    enable websocket transport on the given port")<<std::endl
            <<(rawsocket_port_name1 + ", " + rawsocket_port_name2 + "    enable rawsocket transport on the given port")<<std::endl
            <<(rawsocket_path_name1 + ", " + rawsocket_path_name2 + "    enable rawsocket transport for the given path")<<std::endl
            <<(realm_name1 + ", " + realm_name2 + "    set the WAMP realm for this router")<<std::endl
            <<(dis_json_ser_name + "    disable JSON serialization")<<std::endl
            <<(dis_msgpack_ser_name + "    disable msgpack serialization")<<std::endl
            <<(debug_name1 + ", " + debug_name2 + "    enable debug output")<<std::endl
            <<std::endl;
            return 1;
        }else{
            if(check_option_present(argv, argv+argc, debug_name1) ||
               check_option_present(argv, argv+argc, debug_name2) ){
                options.set_debug_enabled(true);
            }

            char *param_str(nullptr);

            if( (nullptr != (param_str = get_option(argv, argv+argc, websocket_port_name1))) ||
                (nullptr != (param_str = get_option(argv, argv+argc, websocket_port_name2))) ){
                std::uint16_t socket_port(stoul(std::string(param_str)));

                if(std::numeric_limits<decltype(socket_port)>::max() < socket_port ){
                    throw std::out_of_range("Websocket Port value out of range");
                }
                options.set_websocket_port(static_cast<std::uint16_t>(socket_port));
                options.set_websocket_enabled(true);
            }

            if( (nullptr != (param_str = get_option(argv, argv+argc, rawsocket_port_name1))) ||
                (nullptr != (param_str = get_option(argv, argv+argc, rawsocket_port_name2))) ){
                std::uint16_t socket_port(stoul(std::string(param_str)));

                if(std::numeric_limits<decltype(socket_port)>::max() < socket_port ){
                    throw std::out_of_range("Rawsocket Port value out of range");
                }
                options.set_rawsocket_port(static_cast<std::uint16_t>(socket_port));
                options.set_rawsocket_enabled(true);
            }

            if( (nullptr != (param_str = get_option(argv, argv+argc, rawsocket_path_name1))) ||
                (nullptr != (param_str = get_option(argv, argv+argc, rawsocket_path_name2))) ){
                options.set_rawsocket_path(std::string(param_str));
                options.set_rawsocket_enabled(true);
            }

            if( (nullptr != (param_str = get_option(argv, argv+argc, realm_name1))) ||
                (nullptr != (param_str = get_option(argv, argv+argc, realm_name2))) ){
                options.set_realm(std::string(param_str));
            }

            if(check_option_present(argv, argv+argc, dis_json_ser_name) ){
                options.set_json_serialization_enabled(false);;
            }

            if(check_option_present(argv, argv+argc, dis_msgpack_ser_name) ){
                options.set_msgpack_serialization_enabled(false);;
            }
        }
    } catch (...) {
        std::cout << "Unrecognized options. Run with "<<help_param_name<<" for parameters description.\n\n";
        return 1;
    }

    bonefish::daemon daemon(options);
    daemon.run();
    return 0;
}
