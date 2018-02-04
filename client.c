#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>


#include "client_queue.h"
#include "nanomsg/include/nn.h"
#include "nanomsg/include/pipeline.h"
#include "data_containers/linked_list.h"
#include "data_containers/dict.h"
#include "blockchain.h"

//IDs
RSA* our_keys;
char* pub_key;
char* pri_key;
char asci_pub_key[500] = {0};

//Threads
pthread_t inbound_network_thread;
pthread_t outbound_network_thread;
pthread_mutex_t our_mutex;
int close_threads;


//Lists
list* other_nodes;
int number_of_nodes;
list* outbound_msg_queue; //holds outbound message structs
list* inbound_msg_queue; //holds recieved char* messages to execute

//Dicts
dict* posted_things;

//Sockets
int sock_in;
int sock_out;
char our_ip[300];



//Message item structure
typedef struct client_message_item {
    char toWhom[300];
    char message[2000];
    unsigned int tries;
    int status; //0 - unsent, 1 - sent, 2 - accepted
    char sig[513];
} client_message_item;

void setup_message(client_message_item* in_message) {
    in_message->status = -1;
    in_message->tries = 0;
    memset(in_message->toWhom, 0, sizeof(in_message->toWhom));
    return;
}

void display_help() {
    printf("Help/Command List: 'h'\n");
    printf("Transactions List: 't'\n");
    printf("Submit a post: 'n sender reciever amount'\n");
    printf("Post New Transaction: 'n sender reciever amount'\n");
    printf("Quit program: 'q'\n");
    return;
}

void quit_program() {
    printf("Shutting down. Good bye!\n");
    exit(0);
}

//bool val_trans_format(char* sender, char* recipient, char* amount) {
bool val_trans_format(char* recipient, char* amount) {

    /*
    for(int i = 0; i < strlen(sender); i++) {
        if(sender[i] == ':') {
            printf("You cannot use ':' in sender's or reciever's names.\n");
            return false;
        }
    }*/
    for(int i = 0; i < strlen(recipient); i++) {
        if(recipient[i] == ':') {
            printf("You cannot use ':' in senders or recipients names.\n");
            return false;
        }
    }

    if(isnumber(atoi(amount))) {
        printf("Enter a valid number amount to transfer.\n");
        return false;
    }

    return true;
}

int get_signature(char* output, char* input) {

    char our_message[5000];
    strcpy(our_message, input);


    char* our_sig;
    char* temp = strtok(our_message," ");
    while( (temp = strtok(NULL, " ")) != NULL) {
        our_sig = temp;
    }

    strcpy(output,our_sig);
    return 1;

}


//Send out post to everyone in other_nodes list
void* send_to_all(list* in_list, li_node* in_item, void* data) {
    
    if(in_item == NULL || in_item->size > 300) return NULL;

    //Get ip to send to
    char data_string[2000];
    memcpy(data_string,in_item->data,in_item->size);
    data_string[in_item->size] = 0;

    //Get message to send
    char* out_msg = (char*)data;

    if(strlen(out_msg) > 2000)
        return NULL;

   
    client_message_item announcement;
    setup_message(&announcement);

    //Get signature
    char output[513];
    get_signature(output,out_msg);
    //printf("GOT SIGNATURE: %s\n", output);

    strcpy(announcement.toWhom, data_string);
    strcpy(announcement.message, out_msg);
    strcpy(announcement.sig, output);

    pthread_mutex_lock(&our_mutex);
    li_append(outbound_msg_queue,&announcement,sizeof(announcement));
    pthread_mutex_unlock(&our_mutex);

    return NULL;
}

void post_post(char* input) {

    char note[3] = {0};

    sscanf(input, "%*s %s", note);
    
    char out_msg[2000] = {0};
    char seperator = ' ';
    //strcpy(out_msg, "P ");
    /*
    strcat(out_msg, asci_pub_key);
    strcat(out_msg, seperator);
    strcat(out_msg, note);
    */

    sprintf(out_msg,"P %ld%c%s%c%s%c", time(NULL),seperator, asci_pub_key, seperator, note, seperator);


    char sig[513] = {0};
    message_signature(sig,out_msg + 2,our_keys,pub_key);
    //strcat(out_msg, seperator);
    strcat(out_msg,sig);

    int status = -1;
    dict_insert(posted_things,sig,&status, sizeof(status));

    
    li_foreach(other_nodes,send_to_all, &out_msg);
    

    return;

}


void post_transaction(char* input) {

    char recipient[32];
    char amount[32];

    sscanf(input, "%*s %s %s", recipient, amount);

    int the_amount = atoi(amount);
    sprintf(amount,"%010d", the_amount);
    
    if(!val_trans_format(recipient, amount))
        return;

    char out_msg[2000] = {0};
    char seperator = ' ';
    /*
    strcpy(out_msg, "T ");

    strcat(out_msg, asci_pub_key);
    strcat(out_msg, seperator);
    
    strcat(out_msg, recipient);
    strcat(out_msg, seperator);
    strcat(out_msg, amount);
    */

    sprintf(out_msg,"T %ld%c%s%c%s%c%010d%c", time(NULL),seperator, asci_pub_key, seperator, recipient, seperator, the_amount, seperator);

    char sig[513] = {0};
    message_signature(sig,out_msg + 2, our_keys, pub_key);
    //strcat(out_msg, seperator);
    strcat(out_msg,sig);

    int status = -1;
    dict_insert(posted_things,sig,&status, sizeof(status));

    li_foreach(other_nodes,send_to_all, &out_msg);


    return;
}

//Read configuration file
int read_config2() {
    FILE* config = fopen("node.cfg", "r");
    if(config == NULL) return 0;

    number_of_nodes = 0;
    char buffer[120] = {0};
    while (fgets(buffer, sizeof(buffer), config)) {
        if(buffer[strlen(buffer) -1] == '\n') buffer[strlen(buffer) -1] = 0;
        li_append(other_nodes, buffer, strlen(buffer) + 1);
        number_of_nodes++;
    }
    printf("NUMBER OF NODES: %d\n", number_of_nodes);
    fclose(config);

    return 0;
}

//Done to each message in 'outbound_msg_queue'. input is of type client_message_item struct
void* process_outbound(list* in_list, li_node* input, void* data) {

    if(input == NULL) return NULL;

    client_message_item* our_message = (client_message_item*)input->data;

    sock_out = nn_socket(AF_SP, NN_PUSH);
    assert (sock_out >= 0);
    int timeout = 100;
    assert (nn_setsockopt(sock_out, NN_SOL_SOCKET, NN_SNDTIMEO, &timeout, sizeof(timeout)) >= 0);

    //printf("Sending to: %s: '%.10s'",our_message->toWhom, our_message->message);
    if(nn_connect (sock_out, our_message->toWhom) < 0){
        printf("Connection Error.\n");
        nn_close(sock_out);
    }
    int bytes = nn_send (sock_out, our_message->message, strlen(our_message->message), 0);
    //printf("Bytes sent: %d\n", bytes);
    usleep(100000);
    nn_close(sock_out);

    if(bytes > 0 || our_message->tries == 0){
        li_delete_node(in_list, input);


        int current_status = *((int*)dict_access(posted_things,our_message->sig));

        if(current_status == -1) {
            int status = 0;
            dict_insert(posted_things,our_message->sig,&status, sizeof(status));
        }

    }
    else our_message->tries++;

    return NULL;
}


int check_on_unaccepted(bt_node* current_node) {

    if(current_node == NULL) return 0;

    int value = *((int*)current_node->data);
    if( value == -1 || (value >= (number_of_nodes / 2.0)) ) return 1;

    char out_msg[1500];
    sprintf(out_msg,"V %s %s", our_ip, current_node->key);
    li_foreach(other_nodes,send_to_all, &out_msg);

    return 1;

}


//Outbound thread function - tried to send everything in outbound message queue
void* out_server() {
    while(true) {
        pthread_mutex_lock(&our_mutex);
        li_foreach(outbound_msg_queue, process_outbound, NULL);
        if(close_threads)
            return NULL;

        //Check on un verified messages
        dict_foreach(posted_things,check_on_unaccepted, NULL);        
        pthread_mutex_unlock(&our_mutex);
    }
}

int print_posts_trans(bt_node* current_node) {

    int value = *((int*)current_node->data);

    printf("- '%.50s...' ",current_node->key);
    if( value == -1) printf("Unsent");
    else if( value == 0) printf("Sent");
    else if(value >= (number_of_nodes / 2.0)) printf("Accepted");

    printf("    %d", value );
    printf("\n");

    return 1;
}


void print_posted_items() {

    printf("SENT OUT:\n");
    printf("message & status\n");
    printf("-----------------------------------------\n");
    dict_foreach(posted_things, print_posts_trans, NULL);
    return;
}

int trans_or_post_acceptance( char* input) {

    int* current_status= (int*)dict_access(posted_things, input);

    *current_status = *current_status + 1;

    return 1;
}

void process_message(const char* in_msg) {


    if(in_msg == NULL) return;

    char to_process[30000] = {0};
    strcpy(to_process, in_msg);

    char* token = strtok(to_process," ");

    if(!strcmp(token, "V"))
        trans_or_post_acceptance(to_process + 2);

    return;

}

void* in_server() {

    sock_in = nn_socket (AF_SP, NN_PULL);
    assert (sock_in >= 0);
    assert (nn_bind (sock_in, our_ip) >= 0);
    int timeout = 500;
    assert (nn_setsockopt (sock_in, NN_SOL_SOCKET, NN_RCVTIMEO, &timeout, sizeof (timeout)) >= 0);

    printf("Inbound Socket Ready!\n");

    char buf[30000];

    while(true) {
        int bytes = nn_recv(sock_in, buf, sizeof(buf), 0);
        
        pthread_mutex_lock(&our_mutex);
        if(bytes > 0) {
            buf[bytes] = 0;
            printf("\nRecieved %d bytes: \"%s\"\n", bytes, buf);
                process_message(buf);
        }

        if(close_threads)
                return 0;
        pthread_mutex_unlock(&our_mutex);            

    }
    return 0;
}



int main(void) {
    
    //Setup
    printf("Blockchain in C: Client v0.1 by DG\n'h' for help/commandlist\n");
    char buffer[120] = {0};

    other_nodes = list_create();
    outbound_msg_queue = list_create();
    inbound_msg_queue = list_create();

    posted_things = dict_create();

    strcpy(our_ip, "ipc:///tmp/pipeline_001.ipc");

    //read_config();
    read_config2();

    //Create Keys
    //Try to read keys first
    char pri_file[500];
    sprintf(pri_file, "client_pri.pem");
    char pub_file[500];
    sprintf(pub_file,"client_pub.pem");
    int keys_good = read_keys(&our_keys,pri_file, pub_file);
    if(keys_good == 0){
        RSA_free(our_keys);
        our_keys = NULL;

    }
    create_keys(&our_keys,&pri_key,&pub_key);

    //Otherwise Generate our pri/pub address keys
    if(keys_good) {
        printf("Read Keypair:\n\n");
    }
    else {
        write_keys(&our_keys,pri_file,pub_file);
        printf("Created Keypair (Now Saved):\n\n");
    } 
    strip_pub_key(asci_pub_key, pub_key);
    printf("%s%s\n\n", pri_key, pub_key);

    char buffer2[500];
    strcpy(buffer2, pub_key);


    //Threads
    pthread_mutex_t our_mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_create(&outbound_network_thread, NULL, out_server, NULL);
    pthread_create(&inbound_network_thread, NULL, in_server, NULL);
    close_threads = 0;

    //Wait for command
    while(true) {
        printf("b-in-c>");
        fgets(buffer, 120, stdin);
        buffer[strlen(buffer) - 1] = 0;

        if( !strcmp("h",buffer) || !strcmp("help",buffer) )
            display_help();

        else if( !strcmp("q", buffer) || !strcmp("quit", buffer) || !strcmp("exit", buffer) )
            quit_program();

        else if( !strcmp("t", buffer) )
            print_posted_items();
        else if(buffer[0] == 'n')
            post_transaction(buffer);
        else if(buffer[0] == 'p')
            post_post(buffer);
        else if( !strcmp("", buffer))
            ;
        else
            printf("Command not recognized.\n");

    }

    return 0;
}