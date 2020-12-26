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
int left_child_id = 0;
int right_child_id = 0;

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
    //std::vector<int> task;
};

int cnt = 0;
std::vector<int> calculate_data;
bool alive = true;

void* thread_func_wait_rec(void*) {
    while (alive) {
        usleep(30000);
        Message msg;
        zmq_std::recieve_msg_wait(msg, from_rec);
        std::cout <<  strerror(zmq_errno()) << "\n";
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
        } else if ((msg.id != node_id && msg.type == 30) && ((left_child == false && msg.id < node_id) || (right_child == false && msg.id > node_id))){
            std::cout << "ok\n";
            if (msg.id < node_id && left_child == false){
                int rc = zmq_bind(form_result_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + msg.id)).c_str());
                std::cout <<  strerror(zmq_errno()) << "\n";
                assert(rc == 0);

                rc = zmq_connect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + msg.id)).c_str());
                assert(rc == 0);
                left_child = true;
                left_child_id = msg.id;
                int id = fork();
                if (id == 0){
                    if (execl("client", std::to_string(msg.id).c_str(), std::to_string(msg.id).c_str() ,std::to_string(node_id).c_str(), NULL) == -1){
                        printf("execl error\n");
                    }
                }
            } else if (msg.id > node_id && right_child == false){
                int rc = zmq_bind(form_result_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + msg.id)).c_str());
                std::cout <<  strerror(zmq_errno()) << "\n";
                assert(rc == 0);
                rc = zmq_connect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + msg.id)).c_str());
                assert(rc == 0);
                right_child = true;
                right_child_id = msg.id;
                int id = fork();
                if (id == 0){
                    if (execl("client", std::to_string(msg.id).c_str(), std::to_string(msg.id).c_str() ,std::to_string(node_id).c_str(), NULL) == -1){
                        printf("execl error\n");
                    }
                }
            }
        } else if (msg.id == node_id && msg.type == 40){
            std::cout << "del\n";
            if (left_child == false && right_child == false){
                std::cout << "im gay\n";
                msg.type = 44;
                msg.data = node_id;
                pthread_mutex_lock(mutex);
                zmq_std::send_msg_dontwait(msg_ptr, to_result);
                pthread_mutex_unlock(mutex);
                std::cout << "del\n";
                //zmq_close(from_rec);
                zmq_disconnect(to_result, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + node_id)).c_str());
                zmq_disconnect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + left_child_id)).c_str());
                zmq_disconnect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + right_child_id)).c_str());
                zmq_unbind(from_rec, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + node_id)).c_str());
                zmq_unbind(form_result_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + left_child_id)).c_str());
                zmq_unbind(form_result_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + right_child_id)).c_str());

                zmq_close(to_rec_left);
                zmq_close(to_rec_right);
                zmq_close(form_result_left);
                zmq_close(form_result_right);
                zmq_close(to_result);
                zmq_close(from_rec);
                alive = false;
            } else if (left_child == false){
                msg.type = 42;
                pthread_mutex_lock(mutex_r);
                zmq_std::send_msg_dontwait(msg_ptr, to_rec_right);
                pthread_mutex_unlock(mutex_r);
            } else {

                msg.type = 41;
                pthread_mutex_lock(mutex_l);
                zmq_std::send_msg_dontwait(msg_ptr, to_rec_left);
                pthread_mutex_unlock(mutex_l);
            }
        } else if (msg.type == 41){
            if (right_child == true){
                std::cout << "resend\n";
                pthread_mutex_lock(mutex_r);
                zmq_std::send_msg_dontwait(msg_ptr, to_rec_right);
                pthread_mutex_unlock(mutex_r);
                continue;
            } else if (right_child == false && left_child == true){
                std::cout << "got\n";
                msg.type = 43;
                msg.data = left_child_id;
                pthread_mutex_lock(mutex);
                zmq_std::send_msg_dontwait(msg_ptr, to_result);
                pthread_mutex_unlock(mutex);
                msg.type = 46;
                msg.data = node_id;
                pthread_mutex_lock(mutex);
                zmq_std::send_msg_dontwait(msg_ptr, to_result);
                pthread_mutex_unlock(mutex);

                zmq_disconnect(to_result, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + node_id)).c_str());
                zmq_disconnect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + left_child_id)).c_str());
                zmq_disconnect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + right_child_id)).c_str());
                zmq_unbind(from_rec, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + node_id)).c_str());
                zmq_unbind(form_result_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + left_child_id)).c_str());
                zmq_unbind(form_result_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + right_child_id)).c_str());

                zmq_close(to_rec_left);
                zmq_close(to_rec_right);
                zmq_close(form_result_left);
                zmq_close(form_result_right);
                zmq_close(to_result);
                zmq_close(from_rec);
                alive = false;
            } else {
                std::cout << "got2\n";
                msg.type = 44;
                msg.data = node_id;
                pthread_mutex_lock(mutex);
                zmq_std::send_msg_dontwait(msg_ptr, to_result);
                pthread_mutex_unlock(mutex);
                msg.type = 46;
                msg.data = node_id;
                pthread_mutex_lock(mutex);
                zmq_std::send_msg_dontwait(msg_ptr, to_result);
                pthread_mutex_unlock(mutex);

                zmq_disconnect(to_result, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + node_id)).c_str());
                zmq_disconnect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + left_child_id)).c_str());
                zmq_disconnect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + right_child_id)).c_str());
                zmq_unbind(from_rec, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + node_id)).c_str());
                zmq_unbind(form_result_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + left_child_id)).c_str());
                zmq_unbind(form_result_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + right_child_id)).c_str());

                zmq_close(to_rec_left);
                zmq_close(to_rec_right);
                zmq_close(form_result_left);
                zmq_close(form_result_right);
                zmq_close(to_result);
                zmq_close(from_rec);
                alive = false;
            }

        } else if (msg.type == 42) {
            if (left_child == true) {
                pthread_mutex_lock(mutex_l);
                zmq_std::send_msg_dontwait(msg_ptr, to_rec_left);
                pthread_mutex_unlock(mutex_l);
                continue;
            } else if (left_child == false && right_child == true) {
                msg.type = 43;
                msg.data = right_child_id;
                pthread_mutex_lock(mutex);
                zmq_std::send_msg_dontwait(msg_ptr, to_result);
                pthread_mutex_unlock(mutex);
                msg.type = 46;
                msg.data = node_id;
                pthread_mutex_lock(mutex);
                zmq_std::send_msg_dontwait(msg_ptr, to_result);
                pthread_mutex_unlock(mutex);

                zmq_disconnect(to_result, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + node_id)).c_str());
                zmq_disconnect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + left_child_id)).c_str());
                zmq_disconnect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + right_child_id)).c_str());
                zmq_unbind(from_rec, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + node_id)).c_str());
                zmq_unbind(form_result_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + left_child_id)).c_str());
                zmq_unbind(form_result_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + right_child_id)).c_str());

                zmq_close(to_rec_left);
                zmq_close(to_rec_right);
                zmq_close(form_result_left);
                zmq_close(form_result_right);
                zmq_close(to_result);
                zmq_close(from_rec);
                alive = false;
            } else {
                msg.type = 44;
                msg.data = node_id;
                pthread_mutex_lock(mutex);
                zmq_std::send_msg_dontwait(msg_ptr, to_result);
                pthread_mutex_unlock(mutex);
                msg.type = 46;
                msg.data = node_id;
                pthread_mutex_lock(mutex);
                zmq_std::send_msg_dontwait(msg_ptr, to_result);
                pthread_mutex_unlock(mutex);

                zmq_disconnect(to_result, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + node_id)).c_str());
                zmq_disconnect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + left_child_id)).c_str());
                zmq_disconnect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + right_child_id)).c_str());
                zmq_unbind(from_rec, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + node_id)).c_str());
                zmq_unbind(form_result_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + left_child_id)).c_str());
                zmq_unbind(form_result_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + right_child_id)).c_str());

                zmq_close(to_rec_left);
                zmq_close(to_rec_right);
                zmq_close(form_result_left);
                zmq_close(form_result_right);
                zmq_close(to_result);
                zmq_close(from_rec);
                alive = false;
            }
        }


        else if (msg.id < node_id){
            std::cout << "left\n";
            pthread_mutex_lock(mutex_l);
            zmq_std::send_msg_dontwait(msg_ptr, to_rec_left);
            pthread_mutex_unlock(mutex_l);
        } else if (msg.id > node_id){
            std::cout << "right\n";
            pthread_mutex_lock(mutex_r);
            zmq_std::send_msg_dontwait(msg_ptr, to_rec_right);
            pthread_mutex_unlock(mutex_r);
        }
    }
    std::cout << "end\n";
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
        } else if (msg.type == 43){
            std::cout << "got4\n";
            zmq_disconnect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + left_child_id)).c_str());
            zmq_unbind(form_result_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + left_child_id)).c_str());

//            zmq_close(to_rec_left);
//            zmq_close(form_result_left);

            int rc = zmq_bind(form_result_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + msg.data)).c_str());
            assert(rc == 0);
            rc = zmq_connect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + msg.data)).c_str());
            assert(rc == 0);
            left_child = msg.data;
//            msg.type = 46;
//            pthread_mutex_lock(mutex);
//            zmq_std::send_msg_dontwait(msg_ptr, to_result);
//            pthread_mutex_unlock(mutex);
        } else if (msg.type == 44) {
            left_child = false;
            zmq_disconnect(to_rec_left, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + left_child_id)).c_str());
            zmq_unbind(form_result_left,("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 +  left_child_id)).c_str());

//            zmq_close(to_rec_left);
//            zmq_close(form_result_left);

            //node_id = msg.data;
        } else if (msg.type == 46 && msg.id == node_id){
            std::cout << "got3\n";
            node_id = msg.data;
            msg.type = 43;
            msg.data = node_id;
            pthread_mutex_lock(mutex);
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
            pthread_mutex_unlock(mutex);
        } else if (msg.type == 46){
            pthread_mutex_lock(mutex);
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
            pthread_mutex_unlock(mutex);
        }
        pthread_mutex_unlock(mutex);
    }
    std::cout << "end\n";
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
        } else if (msg.type == 43){
            std::cout << "got4\n";
            zmq_disconnect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + right_child_id)).c_str());
            zmq_unbind(form_result_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + right_child_id)).c_str());

            //zmq_close(from_rec);
            zmq_close(to_rec_right);
            zmq_close(form_result_right);
            int rc = zmq_bind(form_result_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + msg.data)).c_str());
            assert(rc == 0);
            rc = zmq_connect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + msg.data)).c_str());
            assert(rc == 0);
//            msg.type = 46;
//            pthread_mutex_lock(mutex);
//            zmq_std::send_msg_dontwait(msg_ptr, to_result);
//            pthread_mutex_unlock(mutex);

            right_child = msg.data;
        } else if (msg.type == 44){
            //std::cout << "disconnect\n";
            right_child = false;
            zmq_unbind(form_result_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + 1000 + right_child_id)).c_str());
            zmq_disconnect(to_rec_right, ("tcp://127.0.0.1:" + std::to_string(PORT_BASE + right_child_id)).c_str());

            zmq_close(to_rec_right);
            zmq_close(form_result_right);

            //node_id = msg.data;
        } else if (msg.type == 46 && msg.id == node_id){
            node_id = msg.data;
            msg.type = 43;
            msg.data = node_id;
            pthread_mutex_lock(mutex);
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
            pthread_mutex_unlock(mutex);
        } else if (msg.type == 46){
            pthread_mutex_lock(mutex);
            zmq_std::send_msg_dontwait(msg_ptr, to_result);
            pthread_mutex_unlock(mutex);
        }
        pthread_mutex_unlock(mutex);
    }
    std::cout << "end\n";
    return NULL;
}

[[noreturn]] void* heartbeat_func(void*) {
    while (alive) {
        sleep(3);
        //std::cout << "sending\n";
        Message msg;
        msg.id = node_id;
        msg.type = 10;
        Message* msg_ptr = &msg;
        pthread_mutex_lock(mutex);
        zmq_std::send_msg_dontwait(msg_ptr, to_result);
        pthread_mutex_unlock(mutex);
    }
    std::cout << "end\n";
    return NULL;
}

int main (int argc, char** argv) {
    mutex = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    mutex_l = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));
    mutex_r = (pthread_mutex_t*) malloc(sizeof(pthread_mutex_t));

    assert(argc == 3);
    node_id = std::strtoul(argv[1], nullptr, 10);

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

//    rc = pthread_join(rec, NULL);
//    assert(rc == 0);
//    rc = pthread_join(res_left, NULL);
//    assert(rc == 0);
//    std::cout << "im dead " << node_id << "\n";
//    rc = pthread_join(res_right, NULL);
//    assert(rc == 0);
//    rc = pthread_join(heartbeat, NULL);
//    assert(rc == 0);
//    std::cout << "im dead " << node_id << "\n";
    while(alive){

    }
    return 0;
}