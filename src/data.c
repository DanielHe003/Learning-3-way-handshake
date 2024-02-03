// DONE WORKS

#include "csapp.h"
#include "data.h"
#include "store.h"
#include "debug.h"
#include "transaction.h"
/*
 * Create a blob with given content and size.
 * The content is copied, rather than shared with the caller.
 * The returned blob has one reference, which becomes the caller's
 * responsibility.
 *
 * @param content  The content of the blob.
 * @param size  The size in bytes of the content.
 * @return  The new blob, which has reference count 1.
 */
BLOB *blob_create(char *content, size_t size){
    if(size < 0) return NULL;
    BLOB *blob = Calloc(sizeof(BLOB), size);
    if(pthread_mutex_init(&blob->mutex, NULL) < 0 || blob == NULL) {
        Free(blob);
        return NULL;
    }
    blob->refcnt = 1;
    if(content != NULL){
        blob->content = Calloc(sizeof(char), size+1);
        memcpy(blob->content, content, size);
        blob->prefix = Calloc(sizeof(char), size+1);
        memcpy(blob->prefix, content, size);
        blob->size = size;
    } else {
        blob->content = NULL;
        blob->prefix = NULL;
        blob->size = 0;
    }
    return blob;
}

/*
 * Increase the reference count on a blob.
 *
 * @param bp  The blob.
 * @param why  Short phrase explaining the purpose of the increase.
 * @return  The blob pointer passed as the argument.
 */
BLOB *blob_ref(BLOB *bp, char *why){
    if(bp == NULL) return NULL;
    if(pthread_mutex_lock(&bp->mutex) < 0) return NULL;
    bp->refcnt += 1;
    if(pthread_mutex_unlock(&bp->mutex) < 0) return NULL;
    return bp;
}

/*
 * Decrease the reference count on a blob.
 * If the reference count reaches zero, the blob is freed.
 *
 * @param bp  The blob.
 * @param why  Short phrase explaining the purpose of the decrease.
 */
void blob_unref(BLOB *bp, char *why){
    if(bp == NULL || bp->refcnt < 1 || pthread_mutex_lock(&bp->mutex) < 0) return;
    bp->refcnt -= 1;
    if(bp->refcnt == 0){
        if(bp->content != NULL) {
            Free(bp->content);
        }
        if(bp->prefix != NULL){
            Free(bp->prefix);
        }
        pthread_mutex_unlock(&bp->mutex);
        pthread_mutex_destroy(&bp->mutex);
        Free(bp);
    } else {
        pthread_mutex_unlock(&bp->mutex);
    }
}

/*
 * Compare two blobs for equality of their content.
 *
 * @param bp1  The first blob.
 * @param bp2  The second blob.
 * @return 0 if the blobs have equal content, nonzero otherwise.
 */
int blob_compare(BLOB *bp1, BLOB *bp2){
    if(bp1 == NULL || bp2 == NULL) return -1;
    if(pthread_mutex_lock(&bp1->mutex) < 0 || pthread_mutex_lock(&bp2->mutex) < 0) return -1;
    int max = 0;
    if(bp1->size > bp2->size) {
        max = bp1->size;
    } else {
        max = bp2->size;
    }
    int comp = strncmp(bp1->content, bp2->content, max);
    if(pthread_mutex_unlock(&bp1->mutex) < 0 || pthread_mutex_unlock(&bp2->mutex) < 0) return -1;
    return comp;
}

/*
 * Hash function for hashing the content of a blob.
 * 
 * @param bp  The blob.
 * @return  Hash of the blob.
 */
int blob_hash(BLOB *bp){
    if(bp == NULL) return -1;
    int x = 0;
    if(bp->content != NULL) x = *(bp->content) % NUM_BUCKETS;
    return x;
}

/*
 * Create a key from a blob.
 * The key inherits the caller's reference to the blob.
 *
 * @param bp  The blob.
 * @return  the newly created key.
 */
KEY *key_create(BLOB *bp){
    if(bp == NULL) return NULL;
    if(pthread_mutex_lock(&bp->mutex) < 0) return NULL;

    KEY *key = Calloc(sizeof(KEY), sizeof(char));
    key->blob = bp;
    key->hash = blob_hash(bp);

    if(pthread_mutex_unlock(&bp->mutex) < 0) return NULL;
    return key;
}

/*
 * Dispose of a key, decreasing the reference count of the contained blob.
 * A key must be disposed of only once and must not be referred to again
 * after it has been disposed.
 *
 * @param kp  The key.
 */
void key_dispose(KEY *kp){
    if(kp == NULL) return;
    blob_unref(kp->blob, "Dereferencing");
    Free(kp);
}

/*
 * Compare two keys for equality.
 *
 * @param kp1  The first key.
 * @param kp2  The second key.
 * @return  0 if the keys are equal, otherwise nonzero.
 */
int key_compare(KEY *kp1, KEY *kp2){
    if(kp1 == NULL && kp2 == NULL) return 0;
    if(kp1 == NULL || kp2 == NULL) return -1;
    if((kp1->hash == kp2->hash)){
        return blob_compare(kp1->blob, kp2->blob);
    }
    return -1;
}

/*
 * Create a version of a blob for a specified creator transaction.
 * The version inherits the caller's reference to the blob.
 * The reference count of the creator transaction is increased to
 * account for the reference that is stored in the version.
 *
 * @param tp  The creator transaction.
 * @param bp  The blob.
 * @return  The newly created version.
 */
VERSION *version_create(TRANSACTION *tp, BLOB *bp){
    if(tp == NULL) tp = trans_create();
    VERSION *vp = Calloc(sizeof(char), sizeof(VERSION));
    vp->blob = bp;
    vp->creator = trans_ref(tp, "CREATED VERSION");
    vp->next = NULL;
    vp->prev = NULL;
    return vp;
}

/*
 * Dispose of a version, decreasing the reference count of the
 * creator transaction and contained blob.  A version must be
 * disposed of only once and must not be referred to again once
 * it has been disposed.
 *
 * @param vp  The version to be disposed.
 */
void version_dispose(VERSION *vp){
    if(vp == NULL) return;
    trans_unref(vp->creator, "dereferencing");
    vp->creator = NULL;
    vp->next = NULL;
    vp->prev = NULL;
    blob_unref(vp->blob,"dereferencing");
    vp->blob = NULL;
    Free(vp);
    vp = NULL;
}

