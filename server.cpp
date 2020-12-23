//вариант 27 топология - 3, тип команд - 1, тип проверки доступности узлов - 3

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "zmq.h"
#include <assert.h>

#include <zmq.hpp>
#include <string>
#include <iostream>
#include <string>
#include <vector>
#include <pthread.h>
#include <unistd.h>
#include "ZMQ.h"

void* to_rec;
void* from_result;

struct Message {
    std::string type;
    unsigned int id;
    std::vector<int> task;
};

[[noreturn]] void* thread_func_wait_result(void*) {
    while (true) {
        Message msg;
        zmq_std::recieve_msg_wait(msg, from_rec);
        std::cout << msg.type << " " << msg.id << "\n";
    }
    return NULL;
}

[[noreturn]] void* thread_func_send_rec(void*) {
    while (true) {
        Message msg;
        std::cin >> msg.type >> msg.id;
//        if ( msg.type == "calculate"){
//
//        } else {
//
//        }
        zmq_std::send_msg_dontwait(msg, to_rec);
    }
    return NULL;
}

int main (int argc, char const *argv[])
{
    assert(argc == 2);
    node_id = std::stoll(std::string(argv[1]));

    void* context = zmq_ctx_new();
    from_result = zmq_socket(context, ZMQ_PULL);
    to_rec = zmq_socket(context, ZMQ_PUSH);

    unsigned int root_id;
    std::cin >> root_id;

    int rc = zmq_bind(from_rec, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + root_id)).c_str());
    assert(rc == 0);
    rc = zmq_connect(to_result, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + root_id)).c_str());

    int id = fork();
    if (id == 0){

        //assert(rc == 0);
    } else if (id == 1){
        char* argv[3] = {"child", root_id, -1, (char *)NULL};
        if (execv("child", argv) == -1){
            printf("execl error\n");
        }
    }


    return 0;
}