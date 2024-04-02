#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<string.h>
#include<stdint.h>

typedef struct {
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
}InputBuffer;

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

// hard code table
typedef struct {
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];
}Row;

// 将地址 0 转换为一个指向 Struct 类型的指针。
//由于地址 0 是一个无效的指针，所以这个操作不会尝试去访问实际的内存地址，而是创建了一个临时的指针常量。
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)
const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
}MetaCommandResult;

typedef enum { 
    PREPARE_SUCCESS, 
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT
}PrepareResult;

typedef enum { STATEMENT_SELECT, STATEMENT_INSERT} StatementType;

typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL,
}ExecuteResult;

typedef struct {
    StatementType type;
    Row row_to_insert; // insert时才用到
}Statement;

// 设置一页长度
const uint32_t PAGE_SIZE = 4096;
#define TABLE_MAX_PAGES 100
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

typedef struct {
    uint32_t num_rows;
    void* pages[TABLE_MAX_PAGES];
}Table;

void print_row(Row* row) {
    // row是指针，所以row->id可以直接得到地址
    printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

void serialize_row(Row* source, void* destination) {
    // memcpy(void *dest, const void *src, size_t n)
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &source->username, USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &source->email, EMAIL_SIZE);
}

void deserialized_row(void* source, Row* destination) {
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

// 返回第row_num条row在内存中的位置
void* row_slot(Table* table, uint32_t row_num) {
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void* page = table->pages[page_num];
    if (page == NULL) {
        page = table->pages[page_num] = malloc(PAGE_SIZE);
    }
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    // 存储位置
    return page + byte_offset;
}

Table* new_table() {
    Table* table = (Table*)malloc(sizeof(Table));
    table->num_rows = 0;
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++) {
        table->pages[i] = NULL;
    }
    return table;
}

void free_table(Table* table) {
    for (uint32_t i = 0; table->pages[i]; i++) {
        free(table->pages[i]);
    }
    free(table);
}

InputBuffer* new_input_buffer() {
    InputBuffer *input_buffer = (InputBuffer*)malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;
    return input_buffer;
}

void close_input_buffer(InputBuffer* input_buffer) {
    free(input_buffer->buffer);
    free(input_buffer);
    exit(EXIT_SUCCESS);
}

MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table* table) {
    if(strcmp(input_buffer->buffer, ".exit") == 0) {
        close_input_buffer(input_buffer);
        free_table(table);
        exit(EXIT_SUCCESS);
    } else {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

void print_prompt() { printf("mysqlite > "); }

void read_input(InputBuffer* input_buffer) {
    size_t len = 0;
    ssize_t bytes_read = getline(&input_buffer->buffer, &input_buffer->buffer_length, stdin);
    if (bytes_read <= 0) {
        printf("error reading input\n");
        exit(EXIT_FAILURE);
    }
    // 处理换行符
    input_buffer->input_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = 0;
}

PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement) {
    if(strncmp(input_buffer->buffer, "insert", 6) == 0) {
        statement->type = STATEMENT_INSERT;
        int args_assigned = sscanf(input_buffer->buffer, "insert %d %s %s", &(statement->row_to_insert.id),
                            statement->row_to_insert.username, statement->row_to_insert.email);
        if (args_assigned < 3) 
            return PREPARE_SYNTAX_ERROR;
        return PREPARE_SUCCESS;
    }
    if (strcmp(input_buffer->buffer, "select") == 0) {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

ExecuteResult execute_insert(Statement* statement, Table* table) {
    if (table->num_rows >= TABLE_MAX_ROWS) {
        return EXECUTE_TABLE_FULL;
    }
    Row* row = &statement->row_to_insert;
    serialize_row(row, row_slot(table, table->num_rows));
    // 更新表的row数量
    table->num_rows += 1;
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement* statement, Table* table) {
    Row row;
    for (uint32_t i = 0; i < table->num_rows; i++) {
        deserialized_row(row_slot(table, i), &row);
        print_row(&row);
    }
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement* statement, Table* table) {
    switch(statement->type) {
        case STATEMENT_INSERT:
            return execute_insert(statement, table);
        case STATEMENT_SELECT:
            return execute_select(statement, table);
    }
}

int main(int argc, char* argv[]) {
    Table* table = new_table();
    InputBuffer* input_buffer = new_input_buffer();
    while (true)
    {
        print_prompt();
        read_input(input_buffer);
        if(input_buffer->buffer[0] == '.') {
            switch (do_meta_command(input_buffer, table))
            {
            case META_COMMAND_SUCCESS:
                continue;
            case META_COMMAND_UNRECOGNIZED_COMMAND:
                printf("unrecognized command '%s'\n", input_buffer->buffer);
                continue;
            }
        }
        Statement statement;
        switch(prepare_statement(input_buffer, &statement)) {
            case PREPARE_SUCCESS:
                break;
            case PREPARE_SYNTAX_ERROR:
                printf("Syntax error. Couldn't parse statement.\n");
                continue;
            case PREPARE_UNRECOGNIZED_STATEMENT:
                printf("Unrecognized statement command '%s'\n", input_buffer->buffer);
                continue;
        }
        switch (execute_statement(&statement, table)) {
            case EXECUTE_SUCCESS:
                printf("Executed!\n");
                break;
            case EXECUTE_TABLE_FULL:
                printf("Error: Table already full!\n");
                break;
        }
    }
}