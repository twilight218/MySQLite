#ifndef HEADER_H
#define HEADER_H

#include<stdio.h>
#include<stdint.h>

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

// 设置一页长度
const uint32_t PAGE_SIZE = 4096;
#define TABLE_MAX_PAGES 100
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

typedef struct {
    char* buffer;
    size_t buffer_length;
    ssize_t input_length;
}InputBuffer;

typedef enum {
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
}MetaCommandResult;

typedef enum { 
    PREPARE_SUCCESS, 
    PREPARE_NEGATIVE_ID,
    PREPARE_SYNTAX_ERROR,
    PREPARE_STRING_TOO_LONG,
    PREPARE_UNRECOGNIZED_STATEMENT
}PrepareResult;

typedef enum { 
    STATEMENT_SELECT, 
    STATEMENT_INSERT
} StatementType;

typedef enum {
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL,
}ExecuteResult;

typedef struct {
    StatementType type;
    Row row_to_insert; // insert时才用到
}Statement;

typedef struct {
    uint32_t num_rows;
    void* pages[TABLE_MAX_PAGES];
}Table;

// 打印一行
void print_row(Row* row);
// 序列化row，存在内存中
void serialize_row(Row* source, void* destination);
// 反序列化row，从内存读取一行
void deserialized_row(void* source, Row* destination);
// 根据row_num找到在内存中的位置
void* row_slot(Table* table, uint32_t row_num);
// 新建table
Table* new_table();
// 释放table
void free_table(Table* table);
InputBuffer* new_input_buffer();
void close_input_buffer(InputBuffer* input_buffer);

MetaCommandResult do_meta_command(InputBuffer* input_buffer, Table* table);
void print_prompt();
void read_input(InputBuffer* input_buffer);
// 解析输入内容，准备insert
PrepareResult prepare_insert(InputBuffer* input_buffer, Statement* statement);
// 准备statement
PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement);
// 执行insert
ExecuteResult execute_insert(Statement* statement, Table* table);
// 执行select
ExecuteResult execute_select(Statement* statement, Table* table);
// 执行statement
ExecuteResult execute_statement(Statement* statement, Table* table);

#endif