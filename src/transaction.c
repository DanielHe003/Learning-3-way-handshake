#include "transaction.h"
#include "csapp.h"
#include "debug.h" 

/*
 * Initialize the transaction manager.
 */
void trans_init(void){
    trans_list.id = 0;
    trans_list.refcnt = 0;
    trans_list.status = TRANS_PENDING;
    trans_list.depends = NULL;
    trans_list.waitcnt = 0;
    trans_list.next = &trans_list;
    trans_list.prev = &trans_list;
    sem_init(&trans_list.sem, 0, 0);
    pthread_mutex_init(&trans_list.mutex, NULL);
}

/*
 * Finalize the transaction manager.
 */
void trans_fini(void){
    TRANSACTION *transptr = &trans_list;
    TRANSACTION *start = &trans_list;
    while(transptr->next != start){
        TRANSACTION *prevptr = transptr->prev;
        prevptr->next = transptr->next;
        pthread_mutex_lock(&transptr->mutex);
        DEPENDENCY *dependent = transptr->depends;
        DEPENDENCY *next = transptr->depends->next;
        if(next != NULL){
            while(dependent != NULL && next != NULL){
                dependent->trans = NULL;
                dependent->next = NULL;
                Free(dependent);
                dependent = NULL;
                dependent = next;
                next = next->next;
            }
            if(dependent != NULL){
                dependent->trans = NULL;
                dependent->next = NULL;
                Free(dependent);
                dependent = NULL;
            }
        } else{    // pthread_mutex_lock(&cr->mutex);
            Free(dependent);
        }
        pthread_mutex_unlock(&transptr->mutex);
        pthread_mutex_destroy(&transptr->mutex);
        Free(transptr);
        transptr = transptr->next;
    }
}

/*
 * Create a new transaction.
 *
 * @return  A pointer to the new transaction (with reference count 1)
 * is returned if creation is successful, otherwise NULL is returned.
 */
int idNum = 0;

TRANSACTION *trans_create(void){
    TRANSACTION *trans = Calloc(sizeof(char), sizeof(TRANSACTION));
    if(trans == NULL) return NULL;
    if(pthread_mutex_init(&trans->mutex, NULL) < 0 || sem_init(&trans->sem, 0, 0) < 0){
        Free(trans);
        return NULL;
    }
    trans->depends = NULL;
    trans->id = idNum;
    idNum++;
    trans->next = &trans_list;
    trans->prev = trans_list.prev;
    trans_list.prev = trans;
    trans->refcnt = 1;
    trans->status = TRANS_PENDING;
    trans->waitcnt = 0;
    return trans;
}

/*
 * Increase the reference count on a transaction.
 *
 * @param tp  The transaction.
 * @param why  Short phrase explaining the purpose of the increase.
 * @return  The transaction pointer passed as the argument.
 */
TRANSACTION *trans_ref(TRANSACTION *tp, char *why){
    if(tp == NULL) return NULL;
    if(pthread_mutex_lock(&tp->mutex) < 0) return NULL;
    tp->refcnt++;
    if(pthread_mutex_unlock(&tp->mutex) < 0) return NULL;
    return tp;
}

/*
 * Decrease the reference count on a transaction.
 * If the reference count reaches zero, the transaction is freed.
 *
 * @param tp  The transaction.
 * @param why  Short phrase explaining the purpose of the decrease.
 */
void trans_unref(TRANSACTION *tp, char *why){
    if(tp == NULL) return;
    if(pthread_mutex_lock(&tp->mutex) < 0 || tp->refcnt <= 0) return;
    tp->refcnt--;
    if(tp->refcnt == 0){
        DEPENDENCY *dep = NULL;
        if(tp->depends != NULL) { 
            dep = tp->depends;
            while(dep->next != NULL){
                Free(dep);
                dep = dep->next;
            }
            Free(dep);
        }
        tp->prev = tp->next;
        tp->next->prev = tp->prev;
        if(pthread_mutex_unlock(&tp->mutex) < 0 || pthread_mutex_destroy(&tp->mutex) < 0) return;
        if(sem_destroy(&tp->sem) < 0) return;
        Free(tp);
    } else {
        if(pthread_mutex_unlock(&tp->mutex) < 0) return;
    }
}


/*
 * Add a transaction to the dependency set for this transaction.
 *
 * @param tp  The transaction to which the dependency is being added.
 * @param dtp  The transaction that is being added to the dependency set.
 */
void trans_add_dependency(TRANSACTION *tp, TRANSACTION *dtp){
    if(tp == NULL || dtp == NULL) return;
    if(pthread_mutex_lock(&tp->mutex) < 0) return;
    DEPENDENCY *dep = Calloc(sizeof(char), sizeof(DEPENDENCY));
    dep->trans = dtp;
    dep->next = NULL;
    if(tp->depends == NULL){
        tp->depends = dep;
    } else {
        DEPENDENCY *depptr = tp->depends;
        while(depptr->next != NULL){
            //check if it contains it already
            if(depptr->trans == dtp) {
                pthread_mutex_unlock(&tp->mutex);
                return;
            }
            depptr = depptr->next;
        }
        depptr->next = dep;
    }
    trans_ref(dtp, NULL);
    if(pthread_mutex_unlock(&tp->mutex) < 0) return;
    tp->waitcnt++;

}

/*
 * Try to commit a transaction.  Committing a transaction requires waiting
 * for all transactions in its dependency set to either commit or abort.
 * If any transaction in the dependency set abort, then the dependent
 * transaction must also abort.  If all transactions in the dependency set
 * commit, then the dependent transaction may also commit.
 *
 * In all cases, this function consumes a single reference to the transaction
 * object.
 *
 * @param tp  The transaction to be committed.
 * @return  The final status of the transaction: either TRANS_ABORTED,
 * or TRANS_COMMITTED.
 */
TRANS_STATUS trans_commit(TRANSACTION *tp){
    if(tp == NULL || tp->status == TRANS_ABORTED) return TRANS_ABORTED;
    DEPENDENCY *dep = tp->depends;

    //waits for the whole thing to finish
    while(dep != NULL){
        TRANSACTION *trans = dep->trans;
        //semwait but with csapp wrapper
        P(&trans->sem);
        dep = dep->next;
    }

    //check for aborted at all. Set to beginning
    dep = tp->depends;
    while(dep != NULL){
        if(dep->trans->status == TRANS_ABORTED) return trans_abort(tp);
        dep = dep->next;
    }

    int count = tp->waitcnt;

    while(count > 0){
        //sempost in csapp
        V(&tp->sem);
        pthread_mutex_lock(&tp->mutex);
        count--;
        pthread_mutex_unlock(&tp->mutex);
    }
    tp->status = TRANS_COMMITTED;
    trans_unref(tp, "commited");
    return TRANS_COMMITTED;
}

/*
 * Abort a transaction.  If the transaction has already committed, it is
 * a fatal error and the program crashes.  If the transaction has already
 * aborted, no change is made to its state.  If the transaction is pending,
 * then it is set to the aborted state, and any transactions dependent on
 * this transaction must also abort.
 *
 * In all cases, this function consumes a single reference to the transaction
 * object.
 *
 * @param tp  The transaction to be aborted.
 * @return  TRANS_ABORTED.
 */
TRANS_STATUS trans_abort(TRANSACTION *tp){
    if(tp == NULL || tp->status == TRANS_ABORTED) {
        trans_unref(tp, "transaborted");
        return TRANS_ABORTED;
    }
    if(tp->status == TRANS_COMMITTED) {
        trans_unref(tp, NULL);
        abort();
    }
    if(tp->status == TRANS_PENDING) tp->status = TRANS_ABORTED;

    while(tp->waitcnt > 0){
        V(&tp->sem);
        pthread_mutex_lock(&tp->mutex);
        tp->waitcnt--;
        pthread_mutex_unlock(&tp->mutex);
    }

    DEPENDENCY *ptr = tp->depends;
    while(ptr != NULL){
        ptr->trans->status = TRANS_ABORTED;
        ptr = ptr->next;
    }
    trans_unref(tp, NULL);
debug("huh");
    return TRANS_ABORTED;
}

/*
 * Get the current status of a transaction.
 * If the value returned is TRANS_PENDING, then we learn nothing,
 * because unless we are holding the transaction mutex the transaction
 * could be aborted at any time.  However, if the value returned is
 * either TRANS_COMMITTED or TRANS_ABORTED, then that value is the
 * stable final status of the transaction.
 *
 * @param tp  The transaction.
 * @return  The status of the transaction, as it was at the time of call.
 */
TRANS_STATUS trans_get_status(TRANSACTION *tp){
    pthread_mutex_lock(&tp->mutex);
    TRANS_STATUS x = tp->status;
    pthread_mutex_unlock(&tp->mutex);
    return x;
}

/*
 * Print information about a transaction to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 *
 * @param tp  The transaction to be shown.
 */
void trans_show(TRANSACTION *tp){
    fprintf(stderr, "id = %d, refcnt = %d, status = %d, waitcnt = %d\n", tp->id, tp->refcnt, tp->status, tp->waitcnt);
}

/*
 * Print information about all transactions to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 */
void trans_show_all(void){
    TRANSACTION *ptr = trans_list.next;
    while(ptr != &trans_list){
        trans_show(ptr);
        ptr = ptr->next;
    }
}
 