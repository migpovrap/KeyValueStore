// Shared constants between client and server.
// Max number client sessions connected to server simultaneously.
#define MAX_SESSION_COUNT 8 
#define STATE_ACCESS_DELAY_US  // delay to apply to server
#define MAX_PIPE_PATH_LENGTH 40
#define MAX_STRING_SIZE 40
// Max number of subscriptions a given client can have simultaneously.
#define MAX_NUMBER_SUB 10
#define SERVER_RESPONSE_SIZE 2 // size of the server response