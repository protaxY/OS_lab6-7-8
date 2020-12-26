//вариант 27 топология - 3, тип команд - 1, тип проверки доступности узлов - 3

//10 - пинг
//11 - нода мертва
//20 - посчитай на ноде (в data рамер массива)
//21 - получить следующий элемент массива
//22 - результат подсчета
//30 - создать ноду
//31 - нода создана
//32 - ноду уже существует
//40 - удалить ноду
//41 - нода удалена
//42 - такой ноды не существует

#include <cstring>
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
    int type;
    unsigned int id;
    int data;
    //std::vector<int> task;
};

[[noreturn]] void* thread_func_wait_result(void*) {
    while (true) {
        Message msg;
        //Message* msg_ptr = &msg;
        zmq_std::recieve_msg_wait(msg, from_result);
        //std::cout << "recive\n";
        if (msg.type == 22){
            std::cout << msg.type << " " << msg.id << " " << msg.data << "\n";
        }
    }
    return NULL;
}

[[noreturn]] void* thread_func_send_rec(void*) {
    Message msg;
    while (std::cin >> msg.type >> msg.id) {
        if (msg.type == 20 || msg.type == 21){
            std::cin >> msg.data;
        }
        Message* msg_ptr = &msg;
        zmq_std::send_msg_dontwait(msg_ptr, to_rec);
    }
    return NULL;
}

int main (int argc, char const *argv[])
{
    void* context = zmq_ctx_new();
    from_result = zmq_socket(context, ZMQ_PULL);
    to_rec = zmq_socket(context, ZMQ_PUSH);

    unsigned int root_id;
    std::cin >> root_id;

    int rc = zmq_bind(from_result, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + root_id)).c_str());
    assert(rc == 0);
    rc = zmq_connect(to_rec, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + root_id)).c_str());
    assert(rc == 0);

    pthread_t result;
    pthread_create(&result, nullptr, thread_func_wait_result, nullptr);
    pthread_t rec;
    pthread_create(&rec, nullptr, thread_func_send_rec, nullptr);

    std::cout << root_id << "\n";
    std::cout << std::to_string(root_id).c_str();
    std::cout << "create\n";
    int id = fork();
    if (id == 0){
        std::cout << root_id << "\n";
        std::cout << std::to_string(root_id).c_str();
        if (execl("client", std::to_string(root_id).c_str(), std::to_string(root_id).c_str(), std::to_string(-1).c_str(), NULL) == -1){
            printf("execl error\n");
        }
    }

    rc = pthread_join(rec, NULL);
    assert(rc == 0);
    rc = pthread_join(result, NULL);
    assert(rc == 0);

    return 0;
}