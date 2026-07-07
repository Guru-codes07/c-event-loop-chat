#ifndef DATABASE_H
#define DATABASE_H
#include<sqlite3.h>
#include<time.h>
#include<stdint.h>
#define DB_FILE "chat_history.db"
#define DB_MAX_RESULTS 100

/* data structures that are we gonna implement: */
typedef struct
{
    int64_t db_id;            // to store the database ID
    char sender_name[67];     // to store the sender name
    char message_text[1024];  // to store the text messages
    time_t timestamp;         // to store the time when the message is being sent 
    uint32_t message_id;      // client message id 
}StoredMessage;

/* data struct for private message */
typedef struct
{
    int64_t db_id;
    char sender_name[67];
    char recepient_name[67];
    char message_text[1024];
    time_t timestamp;
    uint32_t message_id;
}StorePrivateMessage;

/* Function Declarations */

/* initialising the database .
creating and opening the database.
create tables if they dont exist. */
int db_init(void);

/* store a public chat message */
int db_store_mesage(const char *sender_name,const char *message_text,time_t timestamp,uint32_t message_id);

/* store a private chat message */
int db_store_private_message(const char *sender_name,const char *recipient_name,const char *message_text,time_t timestamp,uint32_t messsasge_id);









#endif