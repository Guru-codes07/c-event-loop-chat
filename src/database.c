#include "database.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>


// declaring the global variables:
static sqlite3 *db = NULL;
static char last_error[256] = {0};

// helping functions :
/* Store error message for later retrieval */
static void set_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vsnprintf(last_error, sizeof(last_error), format, args);
    va_end(args);
    fprintf(stderr, "[DB ERROR] %s\n", last_error);
}

/* Execute SQL statement without return value */
static int exec_sql(const char *sql)
{
    char *errmsg = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    
    if (rc != SQLITE_OK) {
        set_error("SQL execution failed: %s", errmsg ? errmsg : "unknown error");
        sqlite3_free(errmsg);
        return -1;
    }
    
    return 0;
}

/* Create database schema */
static int create_schema(void)
{
    /* Public chat messages table */
    const char *create_messages = 
        "CREATE TABLE IF NOT EXISTS messages (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    sender_name TEXT NOT NULL,\n"
        "    message_text TEXT NOT NULL,\n"
        "    timestamp INTEGER NOT NULL,\n"
        "    message_id INTEGER,\n"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP\n" ");\n"
        "CREATE INDEX IF NOT EXISTS idx_messages_timestamp \n"
        "    ON messages(timestamp DESC);\n"
        "CREATE INDEX IF NOT EXISTS idx_messages_sender \n"
        "    ON messages(sender_name);";
    
    /* Private messages table */
    const char *create_private = 
        "CREATE TABLE IF NOT EXISTS private_messages (\n"
        "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
        "    sender_name TEXT NOT NULL,\n"
        "    recipient_name TEXT NOT NULL,\n"
        "    message_text TEXT NOT NULL,\n"
        "    timestamp INTEGER NOT NULL,\n"
        "    message_id INTEGER,\n"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP\n"
        ");\n"
        "CREATE INDEX IF NOT EXISTS idx_private_timestamp \n"
        "    ON private_messages(timestamp DESC);\n"
        "CREATE INDEX IF NOT EXISTS idx_private_users \n"
        "    ON private_messages(sender_name, recipient_name);";
    
    if (exec_sql(create_messages) < 0)
        return -1;
    
    if (exec_sql(create_private) < 0)
        return -1;
    
    return 0;
}

// creating or opening the database
int db_init(void)
{
    if (db != NULL) {
        set_error("Database already initialized");
        return -1;
    }
    
    /* Open or create database file */
    int rc = sqlite3_open(DB_FILE, &db);
    
    if (rc != SQLITE_OK) {
        set_error("Failed to open database: %s", sqlite3_errmsg(db));
        sqlite3_close(db);
        db = NULL;
        return -1;
    }
    
    /* Enable foreign keys */
    if (exec_sql("PRAGMA foreign_keys = ON") < 0)
        return -1;
    
    /* Set journal mode to WAL for better concurrency */
    if (exec_sql("PRAGMA journal_mode = WAL") < 0)
        return -1;
    
    /* Create schema */
    if (create_schema() < 0)
        return -1;
    
    printf("[DB] Database initialized: %s\n", DB_FILE);
    return 0;
}

int db_store_message(const char *sender_name, const char *message_text,time_t timestamp, uint32_t message_id)
{
    if (db == NULL) 
    {
        set_error("Database not initialized");
        return -1;
    }
    
    if (sender_name == NULL || message_text == NULL) 
    {
        set_error("Invalid parameters: sender_name or message_text is NULL");
        return -1;
    }
    
    sqlite3_stmt *stmt = NULL;
    const char *sql = 
    "INSERT INTO messages (sender_name, message_text, timestamp, message_id) "
    "VALUES (?, ?, ?, ?)";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) 
    {
        set_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
        return -1;
    }
    
    /* Bind parameters */
    sqlite3_bind_text(stmt, 1, sender_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, message_text, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)timestamp);
    sqlite3_bind_int(stmt, 4, (int)message_id);
    
    /* Execute */
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) 
    {
        set_error("Failed to insert message: %s", sqlite3_errmsg(db));
        return -1;
    }
    
    return 0;
}

int db_store_private_message(const char *sender_name, const char *recipient_name,const char *message_text, time_t timestamp,uint32_t message_id)
{
    if (db == NULL) 
    {
        set_error("Database not initialized");
        return -1;
    }
    
    if (sender_name == NULL || recipient_name == NULL || message_text == NULL) 
    {
        set_error("Invalid parameters: NULL argument provided");
        return -1;
    }
    
    sqlite3_stmt *stmt = NULL;
    const char *sql = 
        "INSERT INTO private_messages "
        "(sender_name, recipient_name, message_text, timestamp, message_id) "
        "VALUES (?, ?, ?, ?, ?)";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) 
    {
        set_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
        return -1;
    }
    
    /* Bind parameters */
    sqlite3_bind_text(stmt, 1, sender_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, recipient_name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, message_text, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64)timestamp);
    sqlite3_bind_int(stmt, 5, (int)message_id);
    
    /* Execute */
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) 
    {
        set_error("Failed to insert private message: %s", sqlite3_errmsg(db));
        return -1;
    }
    
    return 0;
}

StoredMessage *db_get_recent_messages(int limit, int *out_count)
{
    if (db == NULL) 
    {
        set_error("Database not initialized");
        return NULL;
    }
    
    if (out_count == NULL) 
    {
        set_error("out_count pointer is NULL");
        return NULL;
    }
    
    /* Clamp limit */
    if (limit <= 0 || limit > DB_MAX_RESULTS)
        limit = DB_MAX_RESULTS;
    
    sqlite3_stmt *stmt = NULL;
    const char *sql = 
        "SELECT id, sender_name, message_text, timestamp, message_id "
        "FROM messages "
        "ORDER BY timestamp DESC "
        "LIMIT ?";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) 
    {
        set_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, limit);
    
    /* Count results */
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        count++;
    
    /* Reset and fetch data */
    sqlite3_reset(stmt);
    
    if (count == 0) 
    {
        sqlite3_finalize(stmt);
        *out_count = 0;
        return NULL;
    }
    
    StoredMessage *results = malloc(count * sizeof(StoredMessage));
    if (results == NULL) 
    {
        set_error("Memory allocation failed");
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    int index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < count) 
    {
        results[index].db_id = sqlite3_column_int64(stmt, 0);
        
        const char *sender = (const char *)sqlite3_column_text(stmt, 1);
        const char *text = (const char *)sqlite3_column_text(stmt, 2);
        
        strncpy(results[index].sender_name, sender, sizeof(results[index].sender_name) - 1);
        results[index].sender_name[sizeof(results[index].sender_name) - 1] = '\0';
        
        strncpy(results[index].message_text, text, sizeof(results[index].message_text) - 1);
        results[index].message_text[sizeof(results[index].message_text) - 1] = '\0';
        
        results[index].timestamp = (time_t)sqlite3_column_int64(stmt, 3);
        results[index].message_id = (uint32_t)sqlite3_column_int(stmt, 4);
        
        index++;
    }
    
    sqlite3_finalize(stmt);
    
    *out_count = count;
    return results;
}

StoredMessage *db_get_messages_by_date(int year, int month, int day, int *out_count)
{
    if (db == NULL) 
    {
        set_error("Database not initialized");
        return NULL;
    }
    
    if (out_count == NULL) 
    {
        set_error("out_count pointer is NULL");
        return NULL;
    }
    
    /* Create date range for the specified day */
    char date_start[32], date_end[32];
    snprintf(date_start, sizeof(date_start), "%04d-%02d-%02d 00:00:00", year, month, day);
    snprintf(date_end, sizeof(date_end), "%04d-%02d-%02d 23:59:59", year, month, day);
    
    sqlite3_stmt *stmt = NULL;
    const char *sql = 
        "SELECT id, sender_name, message_text, timestamp, message_id "
        "FROM messages "
        "WHERE datetime(timestamp, 'unixepoch') BETWEEN ? AND ? "
        "ORDER BY timestamp ASC";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) 
    {
        set_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, date_start, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, date_end, -1, SQLITE_STATIC);
    
    /* Count results */
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        count++;
    
    sqlite3_reset(stmt);
    
    if (count == 0) 
    {
        sqlite3_finalize(stmt);
        *out_count = 0;
        return NULL;
    }
    
    StoredMessage *results = malloc(count * sizeof(StoredMessage));
    if (results == NULL) 
    {
        set_error("Memory allocation failed");
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    int index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < count) 
    {
        results[index].db_id = sqlite3_column_int64(stmt, 0);
        
        const char *sender = (const char *)sqlite3_column_text(stmt, 1);
        const char *text = (const char *)sqlite3_column_text(stmt, 2);
        
        strncpy(results[index].sender_name, sender, sizeof(results[index].sender_name) - 1);
        results[index].sender_name[sizeof(results[index].sender_name) - 1] = '\0';
        
        strncpy(results[index].message_text, text, sizeof(results[index].message_text) - 1);
        results[index].message_text[sizeof(results[index].message_text) - 1] = '\0';
        
        results[index].timestamp = (time_t)sqlite3_column_int64(stmt, 3);
        results[index].message_id = (uint32_t)sqlite3_column_int(stmt, 4);
        
        index++;
    }
    
    sqlite3_finalize(stmt);
    
    *out_count = count;
    return results;
}

StoredPrivateMessage *db_get_private_history(const char *user1, const char *user2,int limit, int *out_count)
{
    if (db == NULL) 
    {
        set_error("Database not initialized");
        return NULL;
    }
    
    if (user1 == NULL || user2 == NULL || out_count == NULL) 
    {
        set_error("Invalid parameters: NULL argument provided");
        return NULL;
    }
    
    /* Clamp limit */
    if (limit <= 0 || limit > DB_MAX_RESULTS)
        limit = DB_MAX_RESULTS;
    
    sqlite3_stmt *stmt = NULL;
    const char *sql = 
        "SELECT id, sender_name, recipient_name, message_text, timestamp, message_id "
        "FROM private_messages "
        "WHERE (sender_name = ? AND recipient_name = ?) OR (sender_name = ? AND recipient_name = ?) "
        "ORDER BY timestamp DESC "
        "LIMIT ?";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) 
    {
        set_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
        return NULL;
    }
    
    sqlite3_bind_text(stmt, 1, user1, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, user2, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, user2, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, user1, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, limit);
    
    /* Count results */
    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW)
        count++;
    
    sqlite3_reset(stmt);
    
    if (count == 0) {
        sqlite3_finalize(stmt);
        *out_count = 0;
        return NULL;
    }
    
    StoredPrivateMessage *results = malloc(count * sizeof(StoredPrivateMessage));
    if (results == NULL) {
        set_error("Memory allocation failed");
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    int index = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && index < count) 
    {
        results[index].db_id = sqlite3_column_int64(stmt, 0);
        
        const char *sender = (const char *)sqlite3_column_text(stmt, 1);
        const char *recipient = (const char *)sqlite3_column_text(stmt, 2);
        const char *text = (const char *)sqlite3_column_text(stmt, 3);
        
        strncpy(results[index].sender_name, sender, sizeof(results[index].sender_name) - 1);
        results[index].sender_name[sizeof(results[index].sender_name) - 1] = '\0';
        
        strncpy(results[index].recipient_name, recipient, sizeof(results[index].recipient_name) - 1);
        results[index].recipient_name[sizeof(results[index].recipient_name) - 1] = '\0';
        
        strncpy(results[index].message_text, text, sizeof(results[index].message_text) - 1);
        results[index].message_text[sizeof(results[index].message_text) - 1] = '\0';
        
        results[index].timestamp = (time_t)sqlite3_column_int64(stmt, 4);
        results[index].message_id = (uint32_t)sqlite3_column_int(stmt, 5);
        
        index++;
    }
    
    sqlite3_finalize(stmt);
    
    *out_count = count;
    return results;
}

int db_cleanup_old_messages(int days_old)
{
    if (db == NULL) 
    {
        set_error("Database not initialized");
        return -1;
    }
    
    if (days_old <= 0) 
    {
        set_error("days_old must be positive");
        return -1;
    }
    
    time_t cutoff = time(NULL) - (days_old * 86400);  /* 86400 seconds/day */
    
    sqlite3_stmt *stmt = NULL;
    const char *sql = "DELETE FROM messages WHERE timestamp < ?";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) 
    {
        set_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)cutoff);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) 
    {
        set_error("Failed to delete old messages: %s", sqlite3_errmsg(db));
        return -1;
    }
    
    int deleted = sqlite3_changes(db);
    
    /* Also clean up private messages */
    stmt = NULL;
    sql = "DELETE FROM private_messages WHERE timestamp < ?";
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) 
    {
        set_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
        return deleted;
    }
    
    sqlite3_bind_int64(stmt, 1, (sqlite3_int64)cutoff);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc == SQLITE_DONE)
        deleted += sqlite3_changes(db);
    
    return deleted;
}

int db_get_stats(int64_t *out_msg_count, int64_t *out_pm_count)
{
    if (db == NULL) 
    {
        set_error("Database not initialized");
        return -1;
    }
    
    if (out_msg_count == NULL || out_pm_count == NULL) 
    {
        set_error("Output pointers are NULL");
        return -1;
    }
    
    /* Count public messages */
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT COUNT(*) FROM messages";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) 
    {
        set_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
        return -1;
    }
    
    if (sqlite3_step(stmt) != SQLITE_ROW) 
    {
        set_error("Failed to fetch message count");
        sqlite3_finalize(stmt);
        return -1;
    }
    
    *out_msg_count = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    
    /* Count private messages */
    stmt = NULL;
    sql = "SELECT COUNT(*) FROM private_messages";
    
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    
    if (rc != SQLITE_OK) 
    {
        set_error("Failed to prepare statement: %s", sqlite3_errmsg(db));
        return -1;
    }
    
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        set_error("Failed to fetch private message count");
        sqlite3_finalize(stmt);
        return -1;
    }
    
    *out_pm_count = sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    
    return 0;
}

int db_cleanup(void)
{
    if (db == NULL)
        return 0;  /* Already cleaned up */
    
    int rc = sqlite3_close(db);
    
    if (rc != SQLITE_OK) {
        set_error("Failed to close database: %s", sqlite3_errmsg(db));
        return -1;
    }
    
    db = NULL;
    printf("[DB] Database connection closed\n");
    return 0;
}

const char *db_get_error(void)
{
    return last_error[0] ? last_error : "No error";
}