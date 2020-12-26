#include <zmq.hpp>
#include <string>
#include <iostream>
#include <string>
#include <vector>
#include <pthread.h>
#include <unistd.h>
#include "ZMQ.h"

unsigned int node_id;
bool left_child = false;
bool right_child = false;

void* from_rec = nullptr;
void* to_rec_left = nullptr;
void* to_rec_right = nullptr;

void* form_result_left = nullptr;
void* form_result_right = nullptr;
void* to_result = nullptr;

pthread_mutex_t* mutex;

//10 - пинг
//11 - нода мертва
//20 - посчитай на ноде (в data рамер массива)
//21 - получить следующий элемент массива
//22 - результат подсчета
//30 - создать ноду
//31 - нода создана
//32 - ноду уже существует
//40 - удалить ноду
//41 - поиск левого кандидата
//42 - реконект ноды-предка
//43 - нода удалена


struct Message{
    int type;
    unsigned int id;
    int data;
    //std::vector<int> task;
};

int cnt = 0;
std::vector<int> calculate_data;
bool alive = true;

[[noreturn]] void* thread_func_wait_rec(void*) {
    while (alive) {
        Message msg;
        zmq_std::recieve_msg_wait(msg, from_rec);
        Message* msg_ptr = &msg;
        if (msg.id == node_id && msg.type == 20){
            std::cout << "wait " << msg.data << " elements\n";
            cnt = 0;
            calculate_data.clear();
            calculate_data.resize(msg.data);
        } else if (msg.id == node_id && msg.type == 21){
            std::cout << "got eletmet " << cnt << "\n";
            calculate_data[cnt] = msg.data;
            ++cnt;
            if (cnt == calculate_data.size()){
                std::cout << "got all data\n";
                int result = 0;
                for (int i = 0; i < calculate_data.size(); ++i){
                    result += calculate_data[i];
                }
                msg.type = 22;
                msg.data = result;
                zmq_std::send_msg_dontwait(msg_ptr, to_result);
            }
        } else if (msg.id == node_id && msg.type == 30){
            msg.type = 32;
            std::cout << "node alredy exists\n";
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
        } else if (msg.id != node_id && msg.type == 30){
            std::cout << "ok\n";
            if (msg.id < node_id && left_child == false){
                int rc = zmq_bind(form_result_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + msg.id)).c_str());
                assert(rc == 0);
                rc = zmq_connect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + msg.id)).c_str());
                assert(rc == 0);
                left_child = true;
                int id = fork();
                if (id == 0){
                    if (execl("client", std::to_string(msg.id).c_str(), std::to_string(msg.id).c_str() ,std::to_string(node_id).c_str(), NULL) == -1){
                        printf("execl error\n");
                    }
                }
            } else if (msg.id > node_id && right_child == false){
                std::cout << "ok2\n";
                int rc = zmq_bind(form_result_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + msg.id)).c_str());
                assert(rc == 0);
                rc = zmq_connect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + msg.id)).c_str());
                assert(rc == 0);
                std::cout << "ok2\n";
                right_child = true;
                int id = fork();
                if (id == 0){

                    if (execl("client", std::to_string(msg.id).c_str(), std::to_string(msg.id).c_str() ,std::to_string(node_id).c_str(), NULL) == -1){
                        printf("execl error\n");
                    }
                }
            }
        } else if (msg.id == node_id && msg.type == 40){
            if (left_child == false && right_child == false){
                zmq_close(from_rec);
                zmq_close(to_rec_left);
                zmq_close(to_rec_right);
                zmq_close(form_result_left);
                zmq_close(form_result_right);
                zmq_close(to_result);
                zmq_close(from_rec);
                alive = false;
            } else if (left_child == false){
                msg.type = 43;
                pthread_mutex_lock(mutex);
                zmq_std::send_msg_dontwait(msg_ptr, to_rec_right);
                pthread_mutex_unlock(mutex);
            } else if (right_child == false){
                msg.type = 43;
                pthread_mutex_lock(mutex);
                zmq_std::send_msg_dontwait(msg_ptr, to_rec_left);
                pthread_mutex_unlock(mutex);
            }
        }

        else if (msg.id < node_id){
            pthread_mutex_lock(mutex);
            zmq_std::send_msg_dontwait(msg_ptr, to_rec_left);
            pthread_mutex_unlock(mutex);
        } else if (msg.id > node_id){
            pthread_mutex_lock(mutex);
            zmq_std::send_msg_dontwait(msg_ptr, to_rec_right);
            pthread_mutex_unlock(mutex);
        }
    }
    return NULL;
}

[[noreturn]] void* thread_func_wait_result_left(void*) {
    while (alive) {
        Message msg;
        zmq_std::recieve_msg_wait(msg, form_result_left);
        Message* msg_ptr = &msg;
        pthread_mutex_lock(mutex);
        if (msg.type == 22){
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
        } else if (msg.type == 32){
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
        } else if (msg.type == 31){
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
        } else if (msg.type == 11){
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
        } else if (msg.type == 10){
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
        }
        pthread_mutex_unlock(mutex);
    }
    return NULL;
}

[[noreturn]] void* thread_func_wait_result_right(void*) {
    while (alive) {
        Message msg;
        zmq_std::recieve_msg_wait(msg, form_result_right);
        Message* msg_ptr = &msg;
        pthread_mutex_unlock(mutex);
        if (msg.type == 22){
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
        } else if (msg.type == 32){
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
        } else if (msg.type == 31){
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
        } else if (msg.type == 11){
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
        } else if (msg.type == 10){
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
        }
        pthread_mutex_unlock(mutex);
    }
    return NULL;
}

[[noreturn]] void* heartbeat_func(void*) {
    while (alive) {
        sleep(1);
        //std::cout << "sending\n";
        Message msg;
        msg.id = node_id;
        msg.type = 10;
        Message* msg_ptr = &msg;
        pthread_mutex_lock(mutex);
        zmq_std::send_msg_dontwait(msg_ptr, to_result);
        pthread_mutex_unlock(mutex);
    }
    return NULL;
}

int main (int argc, char** argv) {
    mutex =(pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));

    std::cout << argv[0] << "\n";
    std::cout << argv[1] << "\n";
    //std::cout << argv[2] << "\n";
    assert(argc == 3);
    node_id = std::strtoul(argv[1], nullptr, 10);
    //unsigned int parent_id = std::stoll(std::string(argv[2]));

    std::cout << "im child with id " << node_id << "\n";

    void* context = zmq_ctx_new();
    from_rec = zmq_socket(context, ZMQ_PULL);
    to_result = zmq_socket(context, ZMQ_PUSH);
    to_rec_left = zmq_socket(context, ZMQ_PUSH);
    to_rec_right = zmq_socket(context, ZMQ_PUSH);
    form_result_left = zmq_socket(context, ZMQ_PULL);
    form_result_right = zmq_socket(context, ZMQ_PULL);

    int rc = zmq_bind(from_rec, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + node_id)).c_str());
	assert(rc == 0);
    rc = zmq_connect(to_result, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + node_id)).c_str());
    assert(rc == 0);

    std::cout << "hi\n";

    rc = pthread_mutex_init(mutex, nullptr);
    assert(rc == 0);
    std::cout << "hi\n";


    pthread_t rec;
    pthread_create(&rec, nullptr, thread_func_wait_rec, nullptr);
    pthread_t res_left;
    pthread_create(&res_left, nullptr, thread_func_wait_result_left, nullptr);
    pthread_t res_right;
    pthread_create(&res_right, nullptr, thread_func_wait_result_right, nullptr);
    pthread_t heartbeat;
    pthread_create(&res_right, nullptr, heartbeat_func, nullptr);



    rc = pthread_join(rec, NULL);
    assert(rc == 0);
    rc = pthread_join(res_left, NULL);
    assert(rc == 0);
    rc = pthread_join(res_right, NULL);
    assert(rc == 0);
    rc = pthread_join(heartbeat, NULL);
    assert(rc == 0);
    return 0;
}