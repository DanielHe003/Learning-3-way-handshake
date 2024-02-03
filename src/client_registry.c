//DONE WORKS

#include "client_registry.h"
#include "csapp.h"
#include "debug.h"

/*
 * A client registry keeps track of the file descriptors for clients
 * that are currently connected.  Each time a client connects,
 * its file descriptor is added to the registry.  When the thread servicing
 * a client is about to terminate, it removes the file descriptor from
 * the registry.  The client registry also provides a function for shutting
 * down all client connections and a function that can be called by a thread
 * that wishes to wait for the client count to drop to zero.  Such a function
 * is useful, for example, in order to achieve clean termination:
 * when termination is desired, the "main" thread will shut down all client
 * connections and then wait for the set of registered file descriptors to
 * become empty before exiting the program.
 */
typedef struct client_registry {
    int fds;
    pthread_mutex_t mutex;
    sem_t sem;
    int count;
} CLIENT_REGISTRY;

/*
 * Initialize a new client registry.
 *
 * @return  the newly initialized client registry, or NULL if initialization
 * fails.
 */
CLIENT_REGISTRY *creg_init(){
    
    CLIENT_REGISTRY *head;
    if ((head  = calloc(sizeof(char),sizeof(CLIENT_REGISTRY))) == NULL)
        return NULL;
    if(pthread_mutex_init(&head->mutex, NULL) < 0 || sem_init(&head->sem, 0, 0) < 0){
        Free(head);
        return NULL;
    }
    head->count = 0;
    head->fds = -1;
    return head;
}

/*
 * Finalize a client registry, freeing all associated resources.
 * This method should not be called unless there are no currently
 * registered clients.
 *
 * @param cr  The client registry to be finalized, which must not
 * be referenced again.
 */
void creg_fini(CLIENT_REGISTRY *cr){
    if(cr == NULL) return;
    cr->fds = 0;
    cr->count = 0;
    pthread_mutex_destroy(&cr->mutex);
    sem_destroy(&cr->sem);
    Free(cr);
}

/*
 * Register a client file descriptor.
 *
 * @param cr  The client registry.
 * @param fd  The file descriptor to be registered.
 * @return 0 if registration is successful, otherwise -1.
 */
int creg_register(CLIENT_REGISTRY *cr, int fd){
    if(cr == NULL || pthread_mutex_lock(&cr->mutex) < 0) return -1;
    cr->count += 1;
    cr->fds = fd;
    pthread_mutex_unlock(&cr->mutex);
    return 0;
}
/*
 * Unregister a client file descriptor, removing it from the registry.
 * If the number of registered clients is now zero, then any threads that
 * are blocked in creg_wait_for_empty() waiting for this situation to occur
 * are allowed to proceed.  It is an error if the CLIENT is not currently
 * registered when this function is called.
 *
 * @param cr  The client registry.
 * @param fd  The file descriptor to be unregistered.
 * @return 0  if unregistration succeeds, otherwise -1.
 */
int creg_unregister(CLIENT_REGISTRY *cr, int fd){
    if(cr == NULL || fd < 0) return -1;
    if(pthread_mutex_lock(&cr->mutex) < 0) return -1;
    if(cr->fds == fd){
        cr->count--;
        cr->fds = -1;
        if(cr->count == 0){
            if (sem_post(&cr->sem) < 0) return -1;
        }
        pthread_mutex_unlock(&cr->mutex);
        return 0;
    }
    return -1;
}

/*
 * A thread calling this function will block in the call until
 * the number of registered clients has reached zero, at which
 * point the function will return.  Note that this function may be
 * called concurrently by an arbitrary number of threads.
 *
 * @param cr  The client registry.
 */
void creg_wait_for_empty(CLIENT_REGISTRY *cr){
    if(cr == NULL)return;
    while(cr->count != 0){
        sem_wait(&cr->sem);
        cr->count--;
        debug("%d",cr->count);
    }
}

/*
 * Shut down (using shutdown(2)) all the sockets for connections
 * to currently registered clients.  The file descriptors are not
 * unregistered by this function.  It is intended that the file
 * descriptors will be unregistered by the threads servicing their
 * connections, once those server threads have recognized the EOF
 * on the connection that has resulted from the socket shutdown.
 *
 * @param cr  The client registry.
 */
void creg_shutdown_all(CLIENT_REGISTRY *cr){
    if(cr == NULL) return;
    creg_unregister(cr, cr->fds);
    shutdown(cr->fds, SHUT_RDWR);
}

