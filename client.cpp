#include <zmq.hpp>
#include <string>
#include <iostream>
#include <string>
#include <vector>
#include "ZMQ.h"

unsigned int node_id;

void* from_rec = nullptr;
void* to_rec = nullptr;

void* form_result = nullptr;
void* to_result = nullptr;

struct Message{
    std::string type;
    unsigned int id;
    std::vector<int> task;
};


[[noreturn]] void* thread_func_wait_rec(void*) {
    while (true) {
        Message msg;
        zmq_std::recieve_msg_wait(msg, from_rec);
        if (msg.id == node_id && msg.type == "calculate"){
            msg.type = "result";
            msg.task.push_back(1);
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.id == node_id && msg.type == "create"){
            msg.type = "already created";
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.id != node_id && msg.type == "create"){

        } else {
            zmq_std::send_msg_dontwait(&msg, to_rec);
        }
    }
    return NULL;
}

[[noreturn]] void* thread_func_wait_result(void*) {
    while (true) {
        Message msg;
        zmq_std::recieve_msg_wait(msg, form_result);
        if (msg.type == "result"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.type == "already created"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.type == "created"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.type == "dead"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        }
    }
    return NULL;
}


int main (int argc, char** argv) {
    bool child = false;

    assert(argc == 2);
    node_id = std::stoll(std::string(argv[1]));

    void* context = zmq_ctx_new();
    from_rec = zmq_socket(context, ZMQ_PAIR);
    to_rec = zmq_socket(context, ZMQ_PAIR);
    form_result = zmq_socket(context, ZMQ_PAIR);
    to_result = zmq_socket(context, ZMQ_PAIR);

    int rc = zmq_connect(node_parent_socket, ("tcp://localhost:" + std::to_string(PORT_BASE + node_id)).c_str());
	assert(rc == 0);

    while (true) {

    }
    return 0;
}