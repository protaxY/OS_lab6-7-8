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
#include <string>
#include <iostream>
#include <queue>
#include <pthread.h>
#include <unistd.h>
#include <string>
#include <time.h>
#include "ZMQ.h"

void* to_rec;
void* from_result;

struct QTimer{
    unsigned int id;
    time_t time;
};

std::vector<QTimer> heartbeat_vector;
pthread_mutex_t* mutex;

struct Message {
    int type;
    unsigned int id;
    int data;
};

[[noreturn]] void* thread_func_wait_result(void*) {
    while (true) {
        Message msg;
        zmq_std::recieve_msg_wait(msg, from_result);
        pthread_mutex_lock(mutex);
        //std::cout << msg.type << " " << msg.id << " " << msg.data << "\n";
        if (msg.type == 22){
            std::cout << "result on node " << msg.id << ":" << msg.data << "\n";
            //std::cout << msg.type << " " << msg.id << " " << msg.data << "\n";
        } else if (msg.type == 32){
            std::cout << "node already exists\n";
        } else if (msg.type == 10){
            for (int i = 0; i < heartbeat_vector.size(); ++i){
                if (heartbeat_vector[i].id == msg.id){
                    //std::cout << "check\n";
                    heartbeat_vector[i].time = time(NULL);
                    break;
                }
            }
        }
        pthread_mutex_unlock(mutex);
    }
    return NULL;
}

[[noreturn]] void* thread_func_send_rec(void*) {
    Message msg;
    Message* msg_ptr = &msg;
    std::string command;
    while (true) {
        std::cin >> command >> msg.id;
        if (command == "create"){
            pthread_mutex_lock(mutex);
            msg.type = 30;
            zmq_std::send_msg_dontwait(msg_ptr, to_rec);
            QTimer tmp;
            tmp.id = msg.id;
            tmp.time = time(NULL);
            heartbeat_vector.push_back(tmp);
            sleep(1);
            pthread_mutex_unlock(mutex);

        } else if (command == "delete"){
            pthread_mutex_lock(mutex);
            msg.type = 40;
            for (int i = 0; i < heartbeat_vector.size(); ++i){
                if (heartbeat_vector[i].id == msg.id){
                    heartbeat_vector.erase(heartbeat_vector.begin() + i);
                    break;
                }
            }
            zmq_std::send_msg_dontwait(msg_ptr, to_rec);
            sleep(1);
            pthread_mutex_unlock(mutex);

        } else if (command == "calculate"){
            pthread_mutex_lock(mutex);
            msg.type = 20;
            std::cin >> msg.data;
            zmq_std::send_msg_dontwait(msg_ptr, to_rec);
            sleep(1);
            pthread_mutex_unlock(mutex);

        } else if (command == "add"){
            pthread_mutex_lock(mutex);
            msg.type = 21;
            std::cin >> msg.data;
            zmq_std::send_msg_dontwait(msg_ptr, to_rec);
            pthread_mutex_unlock(mutex);
        }
    }
    return NULL;
}

[[noreturn]] void* heartbeat_monitor(void*) {
    Message msg;
    Message* msg_ptr = &msg;
    std::string command;
    while (true) {
        sleep(1);
        for (int i = 0; i < heartbeat_vector.size(); ++i){
            if (time(NULL) - heartbeat_vector[i].time > 4){
                std::cout << "node " << heartbeat_vector[i].id << " is temporarily unavailable\n";
                pthread_mutex_lock(mutex);
                heartbeat_vector.erase(heartbeat_vector.begin() + i);
                pthread_mutex_unlock(mutex);
            }
        }
    }
    return NULL;
}


int main (int argc, char const *argv[]){
    mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    int rc = pthread_mutex_init(mutex, nullptr);
    assert(rc == 0);
    void* context = zmq_ctx_new();
    from_result = zmq_socket(context, ZMQ_PULL);
    to_rec = zmq_socket(context, ZMQ_PUSH);

    std::cout << "enter root id:";
    unsigned int root_id;
    std::cin >> root_id;

    rc = zmq_bind(from_result, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + root_id)).c_str());
    assert(rc == 0);
    rc = zmq_connect(to_rec, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + root_id)).c_str());
    assert(rc == 0);

    pthread_t result;
    pthread_create(&result, nullptr, thread_func_wait_result, nullptr);
    pthread_t rec;
    pthread_create(&rec, nullptr, thread_func_send_rec, nullptr);
    pthread_t heartbeat;
    pthread_create(&heartbeat, nullptr, heartbeat_monitor, nullptr);

    pthread_mutex_lock(mutex);
    QTimer tmp;
    tmp.id = root_id;
    tmp.time = time(NULL);
    heartbeat_vector.push_back(tmp);
    pthread_mutex_unlock(mutex);

    int id = fork();
    if (id == 0){
        if (execl("client", std::to_string(root_id).c_str(), std::to_string(root_id).c_str(), std::to_string(-1).c_str(), NULL) == -1){
            printf("execl error\n");
        }
    }

    rc = pthread_join(rec, NULL);
    assert(rc == 0);
    rc = pthread_join(heartbeat, NULL);
    assert(rc == 0);
    rc = pthread_join(result, NULL);
    assert(rc == 0);

    return 0;
}