//DONE WORKS

#include "server.h"
#define _POSIX_C_SOURCE 199309L
#include <time.h>
#include "debug.h"
#include "csapp.h"
#include "protocol.h"
#include "transaction.h"
#include "protocol.h"
#include "data.h"
#include "store.h"
#include <stdio.h>

/*
 * Client registry that should be used to track the set of
 * file descriptors of currently connected clients.
 */
// extern CLIENT_REGISTRY *client_registry;

/*
 * Thread function for the thread that handles client requests.
 *
 * @param  Pointer to a variable that holds the file descriptor for
 * the client connection.  This pointer must be freed once the file
 * descriptor has been retrieved.
 */
void *xacto_client_service(void *arg){
    struct timespec t;
    int fdNum = *(int*)arg;
    free(arg);

    //makes sure the file detaches after
    if(pthread_detach(pthread_self()) != 0) debug("error");
    creg_register(client_registry, fdNum);
    TRANSACTION *newTrans = trans_create();
    int end = 1;
    while(end){
        XACTO_PACKET *packet = Calloc(sizeof(XACTO_PACKET), sizeof(char));
        if(proto_recv_packet(fdNum, packet, NULL) == -1){
            debug("hello1");
            trans_abort(newTrans);
            debug("transaborted");
            end = 0;
        } else {
            switch(packet->type){
                case XACTO_GET_PKT:
                    debug("GET packet Recieved");
                    void **key11 = Calloc(sizeof(char), sizeof(void**));
                    XACTO_PACKET *packet11 = Calloc(sizeof(XACTO_PACKET), sizeof(char)); 
                    if(proto_recv_packet(fdNum, packet11, key11) != 0){
                        debug("hello6");
                        trans_abort(newTrans);
                        Free(packet);
                        Free(packet11);
                        Free(key11);
                        end = 0;
                        break;
                    }
                    debug("Recieved key, size %d", ntohl(packet11->size));
                    BLOB *blob11 = blob_create(*key11, ntohl(packet11->size));
                    KEY *key21 = key_create(blob11);
                    debug("temp");
                    if(store_get(newTrans, key21, &blob11) == TRANS_ABORTED){
                        debug("hello7");
                        trans_abort(newTrans);
                        Free(key11);
                        Free(packet11);
                        if(blob11 != NULL) Free(blob11->content);
                        Free(blob11);
                        Free(packet);
                        Free(key21);
                        end = 0;
                        break;
                    }
                    clock_gettime(CLOCK_MONOTONIC, &t);
                    packet->timestamp_sec = t.tv_sec;
                    packet->timestamp_nsec = t.tv_nsec;
                    packet->type = XACTO_REPLY_PKT;
                    packet->status = trans_get_status(newTrans);
                    packet->null = 0;
                    packet->size = 0;
                    
                    debug("2");
                    //sending reply
                    if(proto_send_packet(fdNum, packet, NULL) != 0){
                        debug("hello8");
                        trans_abort(newTrans);
                        Free(key11);
                        Free(packet11);
                        if(blob11 != NULL) Free(blob11->content);
                        Free(blob11);
                        Free(packet);
                        Free(key21);
                        end = 0;
                        break;
                    }

                    clock_gettime(CLOCK_MONOTONIC, &t);
                    packet->timestamp_sec = t.tv_sec;
                    packet->timestamp_nsec = t.tv_nsec;
                    packet->status = trans_get_status(newTrans);
                    packet->type = XACTO_VALUE_PKT;
                    
                    if(blob11 == NULL){
                        packet->null = 1;
                        packet->size = 0;
                        if(proto_send_packet(fdNum, packet, blob11) != 0){
                            debug("hello9");
                            trans_abort(newTrans);
                            Free(key11);
                            Free(packet11);
                            Free(packet);
                            Free(key21);
                            end = 0;
                            break;
                        }
                    } else {
                        if(blob11->size == 0){
                            debug("2");
                            packet->null = 1;
                            packet->size = 0;
                            if(proto_send_packet(fdNum, packet, blob11) != 0){
                                debug("hello9");
                                trans_abort(newTrans);
                                Free(key11);
                                Free(packet11);
                                if(blob11 != NULL) Free(blob11->content);
                                Free(blob11);
                                Free(packet);
                                Free(key21);
                                end = 0;
                                break;
                            }
                            debug("3");
                        } else {
                            debug("4");
                            packet->null = 0;
                            packet->size = blob11->size;
                            if(proto_send_packet(fdNum, packet, blob11->content) != 0){
                                debug("hello9");
                                trans_abort(newTrans);
                                Free(key11);
                                Free(packet11);
                                if(blob11 != NULL) Free(blob11->content);
                                Free(blob11);
                                Free(packet);
                                Free(key21);
                                end = 0;
                                break;
                            }
                            debug("5");
                        }
                    }
                    debug("reached end of get");
                    break;
                case XACTO_COMMIT_PKT:
                    if(trans_get_status(newTrans) == TRANS_ABORTED){
                        trans_abort(newTrans);
                        Free(packet);
                        break;
                    }

                    //reply is always NULL
                    packet->type = XACTO_REPLY_PKT;
                    packet->status = XACTO_PUT_PKT;
                    clock_gettime(CLOCK_MONOTONIC, &t);
                    packet->timestamp_sec = t.tv_sec;
                    packet->timestamp_nsec = t.tv_nsec;
                    packet->size = 0;
                    packet->null = 0;

                    if(proto_send_packet(fdNum, packet, NULL) != 0){
                        trans_abort(newTrans);
                        Free(packet);
                        break;
                    }
                    end = 0;
                    break;
                case XACTO_PUT_PKT:
                    void **key1 = Calloc(sizeof(void**), sizeof(char)); 
                    XACTO_PACKET *packet1 = Calloc(sizeof(XACTO_PACKET), sizeof(char)); 
                    int x = 0;
                    if(proto_recv_packet(fdNum, packet1, key1) != 0){
                        debug("hello2");
                        trans_abort(newTrans);
                        Free(key1);
                        Free(packet1);
                        Free(packet);
                        x = 1;
                        end = 0;
                        break;
                    }

                    void **key2 = Calloc(sizeof(void**), sizeof(char));
                    XACTO_PACKET *packet2 = Calloc(sizeof(XACTO_PACKET), sizeof(char));
                    if(proto_recv_packet(fdNum, packet2, key2) != 0){
                        debug("hello3");
                        trans_abort(newTrans);
                        Free(key1);
                        Free(packet1);
                        Free(key2);
                        Free(packet2);
                        Free(packet);
                        end = 0;
                        break;
                    }
                    if(x == 0){
                        BLOB *p1 = blob_create(*key1, htonl(packet1->size));
                        KEY *key3 = key_create(p1);
                        BLOB *p2 = blob_create(*key2, htonl(packet2->size));

                        if(store_put(newTrans, key3, p2) == TRANS_ABORTED){
                            debug("hello4");
                            trans_abort(newTrans);
                            Free(key1);
                            Free(packet1);
                            Free(key2);
                            Free(packet2);
                            Free(packet);
                            if(key1 != NULL) Free(p1->content);
                            Free(p1);
                            if(key2 != NULL) Free(p2->content);
                            Free(p2);
                            end = 0;
                            break;
                        } else {
                            //reply is always NULL
                            packet->type = XACTO_REPLY_PKT;
                            packet->status = trans_get_status(newTrans);
                            struct timespec t;
                            clock_gettime(CLOCK_MONOTONIC, &t);
                            packet->timestamp_sec = t.tv_sec;
                            packet->timestamp_nsec = t.tv_nsec;
                            packet->serial = packet->serial;
                            packet->size = 0;
                            packet->null = 0;

                            if(proto_send_packet(fdNum, packet, NULL) != 0){
                                debug("hello5");
                                trans_abort(newTrans);
                                Free(key1);
                                Free(packet1);
                                Free(key2);
                                Free(packet);
                                Free(packet2);
                                if(key1 != NULL) Free(p1->content);
                                Free(p1);
                                if(key2 != NULL) Free(p2->content);
                                Free(p2);
                                end = 0;
                                break;
                            }
                        }
                    }
                    break;
                default: 
                    break;
            }
        }
        if(end == 0) Free(packet);
    }
    return NULL;
} 