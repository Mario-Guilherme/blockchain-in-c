typedef struct message_item {
    char toWhom[120];
    char message[2400];
    unsigned int tries;
} message_item;


void* announce_length(list* in_list, li_node* in_item, void* data);
void* announce_existance(list* in_list, li_node* in_item, void* data);
int request_chain(char* address);
int compare_length(char* input);
int send_chain(char* address);
int chain_incoming(char* input);

int mine();

int insert_trans(char* input);
int insert_post(const char* input);

void verify_foreign_block(const char* input);
void register_new_node(char* input);

void process_message(const char* in_msg);
void* in_server();
int read_config();
void graceful_shutdown(int dummy);
int main(int argc, char* argv[]);
void* print_queue(void* data);
void setup_message(message_item* in_message);
void* inbound_executor();
void* process_inbound(list* in_list, li_node* input, void* data);




