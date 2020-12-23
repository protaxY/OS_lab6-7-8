#include <zmq.hpp>
#include <string>
#include <iostream>
#include <string>
#include <vector>
#include <pthread.h>
#include <unistd.h>
#include "ZMQ.h"

unsigned int node_id;

void* from_rec = nullptr;
void* to_rec_left = nullptr;
void* to_rec_right = nullptr;

void* form_result_left = nullptr;
void* form_result_right = nullptr;
void* to_result = nullptr;

pthread_mutex_t* mutex;

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

        } else if (msg.id < node_id){
            zmq_std::send_msg_dontwait(&msg, to_rec_left);
        } else if (msg.id > node_id){
            zmq_std::send_msg_dontwait(&msg, to_rec_right);
        }
    }
    return NULL;
}

[[noreturn]] void* thread_func_wait_result_left(void*) {
    while (true) {
        Message msg;
        zmq_std::recieve_msg_wait(msg, form_result_left);
        pthread_mutex_lock(mutex);
        if (msg.type == "result"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.type == "already created"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.type == "created"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.type == "dead"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.type == "alive"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        }
        pthread_mutex_unlock(mutex);
    }
    return NULL;
}

[[noreturn]] void* thread_func_wait_result_right(void*) {
    while (true) {
        Message msg;
        zmq_std::recieve_msg_wait(msg, form_result_right);
        pthread_mutex_unlock(mutex);
        if (msg.type == "result"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.type == "already created"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.type == "created"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.type == "dead"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        } else if (msg.type == "alive"){
            zmq_std::send_msg_dontwait(&msg, to_result);
        }
        pthread_mutex_unlock(mutex);
    }
    return NULL;
}

[[noreturn]] void* heartbeat_func(void*) {
    while (true) {
        sleep(4);
        Message msg;
        msg.id = node_id;
        msg.type = "alive";
        pthread_mutex_lock(mutex);
        zmq_std::send_msg_dontwait(&msg, to_result);
        pthread_mutex_unlock(mutex);
    }
    return NULL;
}

int main (int argc, char** argv) {
    bool left_child = false;
    bool right_child = false;

    assert(argc == 3);
    node_id = std::stoll(std::string(argv[1]));
    unsigned int parent_id = std::stoll(std::string(argv[2]));

    void* context = zmq_ctx_new();
    from_rec = zmq_socket(context, ZMQ_PULL);
    to_result = zmq_socket(context, ZMQ_PUSH);
    to_rec_left = zmq_socket(context, ZMQ_PUSH);
    to_rec_right = zmq_socket(context, ZMQ_PUSH);
    form_result_left = zmq_socket(context, ZMQ_PULL);
    form_result_right = zmq_socket(context, ZMQ_PULL);

    int rc = zmq_bind(from_rec, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + node_id)).c_str());
	assert(rc == 0);
    rc = zmq_connect(to_result, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + parent_id)).c_str());
    assert(rc == 0);

    pthread_mutexattr_t mutexAttribute;
    if (pthread_mutexattr_init(&mutexAttribute) != 0){
        perror("pthread_mutexattr_init error\n");
        return -1;
    }
    if (pthread_mutex_init(mutex, &mutexAttribute) != 0){
        perror("pthread_mutex_init error\n");
        return -1;
    }

    pthread_t rec;
    pthread_create(&rec, nullptr, thread_func_wait_rec, nullptr);
    pthread_t res_left;
    pthread_create(&res_left, nullptr, thread_func_wait_result_left, nullptr);
    pthread_t res_right;
    pthread_create(&res_right, nullptr, thread_func_wait_result_right, nullptr);

    rc = pthread_join(rec, NULL);
    assert(rc == 0);
    rc = pthread_join(res_left, NULL);
    assert(rc == 0);
    rc = pthread_join(res_right, NULL);
    assert(rc == 0);

    return 0;
}