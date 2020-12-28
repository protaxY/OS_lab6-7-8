#ifndef OS_LAB6_7_8_ZMQ_H
#define OS_LAB6_7_8_ZMQ_H

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <string>
#include <zmq.h>

const int PORT_BASE = 8000;

namespace zmq_std {
    template<class T>
    bool send_msg_dontwait(T* token, void* socket) {
        int rc;
        zmq_msg_t message;
        zmq_msg_init(&message);
        rc = zmq_msg_init_size(&message, sizeof(T));
        assert(rc == 0);
        rc = zmq_msg_init_data(&message, token, sizeof(T), NULL, NULL);
        assert(rc == 0);
        rc = zmq_msg_send(&message, socket, ZMQ_DONTWAIT);
        if (rc == -1) {
            zmq_msg_close(&message);
            return false;
        }
        assert(rc == sizeof(T));
        return true;
    }

    template<class T>
    bool recieve_msg_wait(T & reply_data, void* socket) {
        int rc = 0;
        zmq_msg_t reply;
        zmq_msg_init(&reply);
        rc = zmq_msg_recv(&reply, socket, 0);
        if (rc == -1) {
            zmq_msg_close(&reply);
            return false;
        }
        assert(rc == sizeof(T));
        reply_data = *(T*)zmq_msg_data(&reply);
        rc = zmq_msg_close(&reply);
        assert(rc == 0);
        return true;
    }
}

#endif //OS_LAB6_7_8_ZMQ_H
