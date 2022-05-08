
#include "chatServer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#define FALSE 0
#define TRUE 1


static int end_server = 0;

void intHandler(int SIG_INT) {
    /* use a flag to end_server to break the main loop */
    end_server=TRUE;
}

int main (int argc, char *argv[])
{
    if(argc!=2){
        printf("Usage: <port>\n");
        exit(EXIT_FAILURE);
    }
    for(int i=0;i<(int) strlen(argv[1]);i++){
        if(argv[1][i]>'9'||argv[1][i]<'0'){
            printf("Usage: <port>\n");
            exit(EXIT_FAILURE);
        }
    }
    if(atoi(argv[1])<0) {
        printf("Usage: <port>\n");
        exit(EXIT_FAILURE);
    }
    int iterator=0;
    signal(SIGINT, intHandler);
    conn_pool_t* pool = malloc(sizeof(conn_pool_t));
    init_pool(pool);
    /*************************************************************/
    /* Create an AF_INET stream socket to receive incoming      */
    /* connections on                                            */
    /*************************************************************/
    int wellcomeSockSd;
    struct sockaddr_in serv_addr;
    wellcomeSockSd=socket(AF_INET, SOCK_STREAM, 0);
    if(wellcomeSockSd < 0){
        perror("error: socket\n");
        exit(EXIT_FAILURE);
    }
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=INADDR_ANY;
    serv_addr.sin_port=htons((int)atoi(argv[1]));
    /*************************************************************/
    /* Set socket to be nonblocking. All of the sockets for      */
    /* the incoming connections will also be nonblocking since   */
    /* they will inherit that state from the listening socket.   */
    /*************************************************************/
    int on = 1;
    if(ioctl(wellcomeSockSd,(int)FIONBIO,(char *)&on)<0){
        perror("error: ioctl\n");
        exit(EXIT_FAILURE);
    }
    /*************************************************************/
    /* Bind the socket                                           */
    /*************************************************************/
    if(bind(wellcomeSockSd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("error: bind\n");
        exit(EXIT_FAILURE);
    }
    /*************************************************************/
    /* Set the listen back log                                   */
    /*************************************************************/
    if(listen(wellcomeSockSd, 5) < 0) {
        perror("error: listen\n");
        exit(EXIT_FAILURE);
    }
    add_conn(wellcomeSockSd,pool);
    /*************************************************************/
    /* Initialize fd_sets  			                             */
    /*************************************************************/
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->write_set);
    FD_SET(wellcomeSockSd,&pool->read_set);
    /*************************************************************/
    /* Loop waiting for incoming connects, for incoming data or  */
    /* to write data, on any of the connected sockets.           */
    /*************************************************************/
    do
    {
        iterator=0;
        /**********************************************************/
        /* Copy the master fd_set over to the working fd_set.     */
        /**********************************************************/
        pool->ready_write_set=pool->write_set;
        pool->ready_read_set=pool->read_set;
        /**********************************************************/
        /* Call select() 										  */
        /**********************************************************/
        printf("Waiting on select()...\nMaxFd %d\n", pool->maxfd);
        pool->nready=select(pool->maxfd+1,&pool->ready_read_set,&pool->ready_write_set,NULL,NULL);
        if(pool->nready<0){
            if(end_server==TRUE)
                break;
            else{
                perror("error: select\n");
                exit(EXIT_FAILURE);
            }
        }
        /**********************************************************/
        /* One or more descriptors are readable or writable.      */
        /* Need to determine which ones they are.                 */
        /**********************************************************/
        conn_t *temp=pool->conn_head;
        while(temp!=NULL)
        {
            /* Each time a ready descriptor is found, one less has  */
            /* to be looked for.  This is being done so that we     */
            /* can stop looking at the working set once we have     */
            /* found all of the descriptors that were ready         */

            /*******************************************************/
            /* Check to see if this descriptor is ready for read   */
            /*******************************************************/
            if (FD_ISSET(temp->fd,&pool->ready_read_set))
            {
                iterator++;
                /***************************************************/
                /* A descriptor was found that was readable		   */
                /* if this is the listening socket, accept one      */
                /* incoming connection that is queued up on the     */
                /*  listening socket before we loop back and call   */
                /* select again. 						            */
                /****************************************************/
                if(temp->fd==wellcomeSockSd){
                    int clientSockSd;
                    struct sockaddr_in client;
                    int cliLen= sizeof(cliLen);
                    clientSockSd=accept(wellcomeSockSd,(struct sockaddr*)&client,(socklen_t*)&cliLen);
                    if(clientSockSd<0){
                        printf("error: accept\n");
                        exit(EXIT_FAILURE);
                    }
                    add_conn(clientSockSd,pool);
                    printf("New incoming connection on sd %d\n", clientSockSd);
                }
                    /****************************************************/
                    /* If this is not the listening socket, an 			*/
                    /* existing connection must be readable				*/
                    /* Receive incoming data his socket             */
                    /****************************************************/
                else{
                    printf("Descriptor %d is readable\n", temp->fd);
                    int nBytes=0;
                    char* buffy=(char*)malloc(BUFFER_SIZE* sizeof(char));
                    if(buffy==NULL){
                        printf("error: Malloc\n");
                        exit(EXIT_FAILURE);
                    }
                    memset(buffy,'\0',BUFFER_SIZE);
                    nBytes=(int)read(temp->fd,buffy,BUFFER_SIZE-1);
                    if(nBytes==0){
                        printf("Connection closed for sd %d\n",temp->fd);
                        conn_t *toRemove=temp;
                        temp=temp->next;
                        remove_conn(toRemove->fd,pool);
                        free(buffy);
                        continue;
                    }
                    else{
                        printf("%d bytes received from sd %d\n", nBytes, temp->fd);
                        add_msg(temp->fd,buffy,nBytes,pool);
                    }
                    free(buffy);
                }
                /* If the connection has been closed by client 		*/
                /* remove the connection (remove_conn(...))    		*/

                /**********************************************/
                /* Data was received, add msg to all other    */
                /* connectios					  			  */
                /**********************************************/
            } /* End of if (FD_ISSET()) */
            /*******************************************************/
            /* Check to see if this descriptor is ready for write  */
            /*******************************************************/
            if (FD_ISSET(temp->fd,&pool->ready_write_set)&&temp->fd!=wellcomeSockSd) {
                iterator++;
                /* try to write all msgs in queue to sd */
                write_to_client(temp->fd,pool);
            }
            /*******************************************************/
            if(iterator==pool->nready) {
                iterator=0;
                break;
            }
            temp=temp->next;
        } /* End of loop through selectable descriptors */

    } while (end_server == FALSE);

    /*************************************************************/
    /* If we are here, Control-C was typed,						 */
    /* clean up all open connections					         */
    /*************************************************************/

    if(pool!=NULL) {
        conn_t *temp=pool->conn_head;
        while (temp!=NULL){
            conn_t *toDealloc=temp;
            temp=temp->next;
            remove_conn(toDealloc->fd,pool);
        }
        free(pool);
    }

    return 0;
}


int init_pool(conn_pool_t* pool) {
    //initialized all fields
    if(pool==NULL)
        return -1;
    pool->nr_conns=0;
    pool->nready=0;
    pool->maxfd=0;
    FD_ZERO(&pool->write_set);
    FD_ZERO(&pool->ready_write_set);
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->ready_read_set);
    pool->conn_head=NULL;
    return 0;
}

int add_conn(int sd, conn_pool_t* pool) {
    /*
     * 1. allocate connection and init fields
     * 2. add connection to pool
     * */
    conn_t *newConn=(conn_t*)malloc(sizeof(conn_t));
    newConn->fd=sd;
    newConn->prev=newConn->next=NULL;
    newConn->write_msg_head=newConn->write_msg_tail=NULL;
    if(pool->conn_head==NULL){
        pool->conn_head=newConn;
        FD_SET(sd,&pool->read_set);
    }else{
        conn_t *temp=pool->conn_head;
        while(temp->next!=NULL){
            temp=temp->next;
        }
        temp->next=newConn;
        newConn->prev=temp;
        FD_SET(sd,&pool->read_set);
    }
    if(sd>pool->maxfd)
        pool->maxfd=sd;
    pool->nr_conns++;
    return 0;
}

int remove_conn(int sd, conn_pool_t* pool) {
    /*
    * 1. remove connection from pool
    * 2. deallocate connection
    * 3. remove from sets
    * 4. update max_fd if needed
    */
    conn_t *temp=pool->conn_head;
    while(temp!=NULL){
        if(temp->fd==sd)
            break;
        temp=temp->next;
    }
    if(temp==NULL)
        return -1;
    if(temp->fd==pool->conn_head->fd){
        conn_t *toDealloc=temp;
        pool->conn_head=temp->next;
        while(toDealloc->write_msg_head!=NULL){
            msg_t *msgToDealloc=toDealloc->write_msg_head;
            toDealloc->write_msg_head=toDealloc->write_msg_head->next;
            free(msgToDealloc->message);
            msgToDealloc->message=NULL;
            free(msgToDealloc);
            msgToDealloc=NULL;
        }
        close(sd);
        free(toDealloc);
        toDealloc=NULL;
        FD_CLR(sd,&pool->read_set);
        pool->nr_conns--;
    }else{
        if(temp->next==NULL){
            temp->prev->next=NULL;
            while(temp->write_msg_head!=NULL){
                msg_t *msgToDealloc=temp->write_msg_head;
                temp->write_msg_head=temp->write_msg_head->next;
                free(msgToDealloc->message);
                msgToDealloc->message=NULL;
                free(msgToDealloc);
                msgToDealloc=NULL;
            }
            close(sd);
            free(temp);
            temp=NULL;
            FD_CLR(sd,&pool->read_set);
            FD_CLR(sd,&pool->write_set);
            pool->nr_conns--;
        }else{
            temp->prev->next=temp->next;
            temp->next->prev=temp->prev;
            while(temp->write_msg_head!=NULL){
                msg_t *msgToDealloc=temp->write_msg_head;
                temp->write_msg_head=temp->write_msg_head->next;
                free(msgToDealloc->message);
                msgToDealloc->message=NULL;
                free(msgToDealloc);
                msgToDealloc=NULL;
            }
            close(sd);
            free(temp);
            temp=NULL;
            FD_CLR(sd,&pool->read_set);
            FD_CLR(sd,&pool->write_set);
            pool->nr_conns--;
        }
    }
    conn_t *findMax=pool->conn_head;
    int max=0;
    while(findMax!=NULL){
        if(findMax->fd>max)
            max=findMax->fd;
        findMax=findMax->next;
    }
    pool->maxfd=max;
    return 0;
}

int add_msg(int sd,char* buffer,int len,conn_pool_t* pool) {

    /*
     * 1. add msg_t to write queue of all other connections
     * 2. set each fd to check if ready to write
     */
    conn_t *temp=pool->conn_head;
    while(temp!=NULL){
        if(temp->fd!=sd&&temp->fd!=pool->conn_head->fd){
            if(temp->write_msg_head==NULL){//case messege queue is empty
                msg_t *msg=(msg_t*)malloc(sizeof(msg_t));
                if(msg==NULL)
                    return -1;
                msg->next=NULL;
                msg->prev=NULL;
                msg->size=len;
                msg->message=(char*)malloc((len+1)* sizeof(char));
                if(msg->message==NULL)
                    return -1;
                memset(msg->message,'\0',len+1);
                strcpy(msg->message,buffer);
                temp->write_msg_head=msg;
                temp->write_msg_tail=msg;
            }else{
                msg_t *msg=(msg_t*)malloc(sizeof(msg_t));
                if(msg==NULL)
                    return -1;
                msg->next=NULL;
                msg->prev=temp->write_msg_tail;
                temp->write_msg_tail->next=msg;
                temp->write_msg_tail=msg;
                msg->size=len;
                msg->message=(char*)malloc((len+1)* sizeof(char));
                memset(msg->message,'\0',len+1);
                strcpy(msg->message,buffer);
            }
            FD_SET(temp->fd,&pool->write_set);
        }
        temp=temp->next;
    }
    return 0;
}

int write_to_client(int sd,conn_pool_t* pool) {

    /*
     * 1. write all msgs in queue
     * 2. deallocate each writen msg
     * 3. if all msgs were writen successfully, there is nothing else to write to this fd... */
    int nBytes=0;
    conn_t *tempConn=pool->conn_head;
    while(tempConn != NULL){
        if(tempConn->fd == sd){
            msg_t *msgToWrite=tempConn->write_msg_head;
            while(msgToWrite!=NULL){
                nBytes=0;
                nBytes=(int )write(sd,msgToWrite->message,msgToWrite->size);
                if(nBytes<0)
                    continue;
                msg_t *toDeallocate=msgToWrite;
                msgToWrite=msgToWrite->next;
                free(toDeallocate->message);
                toDeallocate->message=NULL;
                free(toDeallocate);
                toDeallocate=NULL;
            }
            tempConn->write_msg_head = NULL;
            tempConn->write_msg_tail = NULL;
            FD_CLR(tempConn->fd, &pool->write_set);
            return 0;
        }
        tempConn=tempConn->next;
    }
    return 1;
}
