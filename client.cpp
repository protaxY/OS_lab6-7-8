#include <string>
#include <iostream>
#include <vector>
#include <pthread.h>
#include <unistd.h>
#include "ZMQ.h"

unsigned int node_id;
bool left_child = false;
bool right_child = false;
int left_child_id = 0;
int right_child_id = 0;

void* context;

void* from_rec = nullptr;
void* to_rec_left = nullptr;
void* to_rec_right = nullptr;

void* form_result_left = nullptr;
void* form_result_right = nullptr;
void* to_result = nullptr;

pthread_mutex_t* mutex;
pthread_mutex_t* mutex_l;
pthread_mutex_t* mutex_r;

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
//42 - поиск правого кандидата
//43 - реконект ноды-предка
//44 - запрос id ноды-ребенка
//45 - получить id ноды-ребенка
//46 - id удаленного кандидата
//47 - нода удалена


struct Message{
    int type;
    unsigned int id;
    int data;
};

int cnt = 0;
std::vector<int> calculate_data;
bool alive = true;

void* thread_func_wait_rec(void*) {
    while (alive) {
        usleep(30000);
        Message msg;
        zmq_std::recieve_msg_wait(msg, from_rec);
        Message *msg_ptr = &msg;
        if (msg.id == node_id && msg.type == 20) {
            std::cout << "wait for " << msg.data << " elements\n";
            cnt = 0;
            calculate_data.clear();
            calculate_data.resize(msg.data);
        } else if (msg.id == node_id && msg.type == 21) {
            std::cout << calculate_data.size() - cnt - 1 << " remaining\n";
            calculate_data[cnt] = msg.data;
            ++cnt;
            if (cnt == calculate_data.size()) {
                std::cout << "got all data\n";
                int result = 0;
                for (int i = 0; i < calculate_data.size(); ++i) {
                    result += calculate_data[i];
                }
                msg.type = 22;
                msg.data = result;
                zmq_std::send_msg_dontwait(msg_ptr, to_result);
            }
        } else if (msg.id == node_id && msg.type == 30) {
            msg.type = 32;
            std::cout << "node alredy exists\n";
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
        } else if ((msg.id != node_id && msg.type == 30) &&
                   ((left_child == false && msg.id < node_id) || (right_child == false && msg.id > node_id))) {
            if (msg.id < node_id && left_child == false) {
                int rc = zmq_bind(form_result_left,
                                  ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + msg.id)).c_str());
                assert(rc == 0);

                rc = zmq_connect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + msg.id)).c_str());
                assert(rc == 0);
                left_child = true;
                left_child_id = msg.id;
                int id = fork();
                if (id == 0) {
                    if (execl("client", std::to_string(msg.id).c_str(), std::to_string(msg.id).c_str(),
                              std::to_string(node_id).c_str(), NULL) == -1) {
                        printf("execl error\n");
                    }
                }
            } else if (msg.id > node_id && right_child == false) {
                int rc = zmq_bind(form_result_right,
                                  ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + msg.id)).c_str());
                assert(rc == 0);
                rc = zmq_connect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + msg.id)).c_str());
                assert(rc == 0);
                right_child = true;
                right_child_id = msg.id;
                int id = fork();
                if (id == 0) {
                    if (execl("client", std::to_string(msg.id).c_str(), std::to_string(msg.id).c_str(),
                              std::to_string(node_id).c_str(), NULL) == -1) {
                        printf("execl error\n");
                    }
                }
            }
        } else if (msg.id == node_id && msg.type == 40) {
            {
                std::cout << node_id << " end\n";
                msg.type = 44;
                msg.data = node_id;
                pthread_mutex_lock(mutex);
                zmq_std::send_msg_dontwait(msg_ptr, to_result);
                pthread_mutex_unlock(mutex);
                zmq_disconnect(to_result, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + node_id)).c_str());
                zmq_disconnect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + right_child_id)).c_str());
                zmq_disconnect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + left_child_id)).c_str());
                zmq_unbind(from_rec, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + node_id)).c_str());
                zmq_unbind(form_result_left,
                           ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + left_child_id)).c_str());
                zmq_unbind(form_result_right,
                           ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + right_child_id)).c_str());

                zmq_close(to_rec_left);
                zmq_close(to_rec_right);
                zmq_close(form_result_left);
                zmq_close(form_result_right);
                zmq_close(to_result);
                zmq_close(from_rec);
                pthread_mutex_lock(mutex);
                alive = false;
                pthread_mutex_unlock(mutex);
                exit(0);
            }
        } else if (msg.id < node_id) {
            pthread_mutex_lock(mutex_l);
            zmq_std::send_msg_dontwait(msg_ptr, to_rec_left);
            pthread_mutex_unlock(mutex_l);
        } else if (msg.id > node_id) {
            pthread_mutex_lock(mutex_r);
            zmq_std::send_msg_dontwait(msg_ptr, to_rec_right);
            pthread_mutex_unlock(mutex_r);
        }
    }
    return NULL;
}

void* thread_func_wait_result_left(void*) {
    while (alive) {
        usleep(30000);
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
        } else if (msg.type == 44){
            right_child = false;
            zmq_unbind(form_result_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + left_child_id)).c_str());
            zmq_disconnect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + left_child_id)).c_str());

        }
        pthread_mutex_unlock(mutex);
    }
    return NULL;
}

void* thread_func_wait_result_right(void*) {
    while (alive) {
        usleep(30000);
        Message msg;
        zmq_std::recieve_msg_wait(msg, form_result_right);
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
        } else if (msg.type == 44){
            right_child = false;
            zmq_unbind(form_result_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + right_child_id)).c_str());
            zmq_disconnect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + right_child_id)).c_str());
        }
        pthread_mutex_unlock(mutex);
    }
    return NULL;
}

void* heartbeat_func(void*) {
    while (alive) {
        Message msg;
        msg.id = node_id;
        msg.type = 10;
        Message* msg_ptr = &msg;
        pthread_mutex_lock(mutex);
        zmq_std::send_msg_dontwait(msg_ptr, to_result);
        pthread_mutex_unlock(mutex);
        sleep(1);
    }
    return NULL;
}

int main (int argc, char** argv) {
    mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    mutex_l = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    mutex_r = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));

    assert(argc == 3);
    node_id = std::strtoul(argv[1], nullptr, 10);

    std::cout << "im child with id " << node_id << "\n";

    context = zmq_ctx_new();
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

    rc = pthread_mutex_init(mutex, nullptr);
    assert(rc == 0);
    rc = pthread_mutex_init(mutex_l, nullptr);
    assert(rc == 0);
    rc = pthread_mutex_init(mutex_r, nullptr);
    assert(rc == 0);

    pthread_t rec;
    pthread_create(&rec, nullptr, thread_func_wait_rec, nullptr);
    pthread_t res_left;
    pthread_create(&res_left, nullptr, thread_func_wait_result_left, nullptr);
    pthread_t res_right;
    pthread_create(&res_right, nullptr, thread_func_wait_result_right, nullptr);
    pthread_t heartbeat;
    pthread_create(&heartbeat, nullptr, heartbeat_func, nullptr);


    rc = pthread_detach(rec);
    assert(rc == 0);
    rc = pthread_detach(res_left);
    assert(rc == 0);
    rc = pthread_detach(res_right);
    assert(rc == 0);
    rc = pthread_detach(heartbeat);
    assert(rc == 0);

    while(alive){}
    return 0;
}