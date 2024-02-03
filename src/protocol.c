//works DONE

#include "protocol.h"
#include "csapp.h"
#include "debug.h"

/*
 * Send a packet header, followed by an associated data payload, if any.
 * Multi-byte fields in the packet header are stored in network byte oder.
 *
 * @param fd    The file descriptor on which packet is to be sent.
 * 
 * @param pkt   The fixed-size part of the packet, with multi-byte fields
 *              in network byte order
 * 
 * @param data  The payload for data packet, or NULL.  A NULL value used
 *              here for a data packet specifies the transmission of a special null
 *              data value, which has no content.
 * 
 * @return      0 in case of successful transmission, -1 otherwise.  In the
 *              latter case, errno is set to indicate the error.
 */
int proto_send_packet(int fd, XACTO_PACKET *pkt, void *data){
    if(fd < 0 || pkt == NULL) return -1;
    int temp = 0;
    if(data != NULL) temp = pkt->size;

    pkt->timestamp_nsec = htonl(pkt->timestamp_nsec);
    pkt->timestamp_sec = htonl(pkt->timestamp_sec);
    pkt->size = htonl(pkt->size);
    
    // Write the return packet
    if(rio_writen(fd, pkt, sizeof(XACTO_PACKET)) < 0) {
        debug("wrong1");
        return -1;
    }
    if(temp != 0){
        if((rio_writen(fd, data, temp)) < 0 ){
            return -1;
        }
    }    
    return 0;
}


/*
 * Receive a packet, blocking until one is available.
 * The returned structure has its multi-byte fields in network byte order.
 *
 * @param fd    The file descriptor from which the packet is to be received.
 * 
 * @param pkt   Pointer to caller-supplied storage for the fixed-size
 *              portion of the packet.
 * 
 * @param datap Pointer to variable into which to store a pointer to any
 *              payload received.
 * 
 * @return      0 in case of successful reception, -1 otherwise.  In the
 *              latter case, errno is set to indicate the error.
 *
 * If the returned payload pointer is non-NULL, then the caller assumes
 * responsibility for freeing the storage.
 */
int proto_recv_packet(int fd, XACTO_PACKET *pkt, void **datap){
    // Read the fixed-size header from the server
    if(rio_readn(fd, pkt, sizeof(XACTO_PACKET)) < 1) {
        debug("wrong1");
        return -1;
    }
    // If length field of header is nonzero then read the payload from wire
    uint32_t x = htonl(pkt->size);
    if(!pkt->null && datap != NULL && x != 0) {
        char *temp = Calloc(x, sizeof(char));
        if(rio_readn(fd, temp, x) < 1) {
            Free(temp);
            debug("wrong2");
            return -1;
        }
        *datap = temp;
    }
    return 0;
}