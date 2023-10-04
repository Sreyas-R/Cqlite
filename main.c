#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include<string.h>
#define COLUMN_EMAIL_SIZE 255
#define COLUMN_USERNAME_SIZE 32
//size of nullpointer of Struct ,  it wont take any specific instance
#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0)->Attribute)


typedef enum { STATEMENT_INSERT, STATEMENT_SELECT } StatementType;

//Page size = 4kb

//Stores commands
typedef struct
{
    char *buffer;
    size_t buffer_length;
    ssize_t input_length;
} InputBuffer;

typedef struct{
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];
}Row;

const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);

const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;   //username comes after id
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

const uint32_t PAGE_SIZE = 4096;
#define TABLE_MAX_PAGES 100
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;


typedef struct{
    StatementType type; //Enum ->(insert , select)
    Row row_to_insert;  //Only used by insert statement(loc)
}Statement;

typedef struct{
    uint32_t num_rows;
    void* pages[TABLE_MAX_PAGES];
}Table;
//We use enums as c doesnt support exception handling
typedef enum{
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED
} MetaCommandResult;
//Statement type
typedef enum { 
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED_STATEMENT,
    PREPARE_SYNTAX_ERROR
} PrepareResult;
typedef enum { EXECUTE_SUCCESS, EXECUTE_TABLE_FULL } ExecuteResult;

//Functiom to create a new input_buffer , returns a struct 
InputBuffer *new_input_buffer()
{
    InputBuffer *input_buffer = (InputBuffer *)malloc(sizeof(InputBuffer));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}

Table* new_table()
{
    Table* table = (Table*)malloc(sizeof(Table));
    table->num_rows = 0;

    for(uint32_t i = 0 ;i<TABLE_MAX_PAGES ;i++)
    {
        table->pages[i] = NULL;
    }
    return table;
}
//finds location of row in memory
void *row_slot(Table* table , uint32_t row_num)
{
    uint32_t page_num =row_num / ROWS_PER_PAGE;
    //Page associated with current row_num
    void* page = table->pages[page_num];

    //If page is null allocate memory for this
    if(page == NULL)
    {
        //allocate page only when we try to access mem
        page = table->pages[page_num] = malloc(PAGE_SIZE);
    }

    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;


    //Void* pointing to location of row_num in memory
    return page + byte_offset;
}
void print_prompt()
{
    printf("DB >");
}

void read_input(InputBuffer* input_buffer)
{
    //Storing data in address of buffer , length -> address of buffer_length
    ssize_t bytes_read = getline(&(input_buffer->buffer) , &(input_buffer->buffer_length) , stdin);
    
    if(bytes_read <= 0)
    {
        printf("Error Reading input \n");
        exit(EXIT_FAILURE);
    }

    input_buffer->input_length = bytes_read - 1;    //0 indexing
    //null termination of string
    input_buffer->buffer[bytes_read - 1] = 0;
}

//Clear memory allocated
void close_input_buffer(InputBuffer* input_buffer)
{
    // free(input_buffer->buffer);
    free(input_buffer);
}

void free_table(Table* table)
{
    for(uint32_t i = 0; table->pages[i] ;i++)
    {
        free(table->pages[i]);
    }
    free(table);
}
//Getting content from source  and serializing into destination(Table)
void serialize_row(Row* source , void* destination)
{
    //New buffer , where to copy from , size
    memcpy(destination + ID_OFFSET , &(source->id) , ID_SIZE);
    memcpy(destination + USERNAME_OFFSET , &(source->username) , USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET , &(source->email) , EMAIL_SIZE);
}

void deserialize_row(void* source , Row* destination)
{
    memcpy(&(destination->id) , source + ID_OFFSET , ID_SIZE);
    memcpy(&(destination->username) , source + USERNAME_OFFSET , USERNAME_SIZE);
    memcpy(&(destination->email) , source + EMAIL_OFFSET , EMAIL_SIZE);
}
MetaCommandResult do_meta_command(InputBuffer* input_buffer)
{
    if(strcmp(input_buffer->buffer , ".exit") == 0)
    {
        exit(EXIT_SUCCESS);
        close_input_buffer(input_buffer);
    }
    else
    {
        return META_COMMAND_UNRECOGNIZED;
    }
}

PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement)
{
    //Compares atmost 6 characters
    if(strncmp(input_buffer->buffer , "insert" ,6) == 0)
    {
        statement->type = STATEMENT_INSERT;
        //Reads from input_buffer->buffer and stores in statement->row_to_insert
        int args_assigned = sscanf(
            input_buffer->buffer , "insert %d %s %s" , &(statement->row_to_insert.id),
            statement->row_to_insert.username , statement->row_to_insert.email);

            if(args_assigned < 3)
            {
                return PREPARE_SYNTAX_ERROR;
            }
        
        return PREPARE_SUCCESS;
    }
    if(strcmp(input_buffer->buffer , "select" ) == 0)
    {
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }

    return PREPARE_UNRECOGNIZED_STATEMENT;

}

void print_row(Row* row) {
  printf("(%d, %s, %s)\n", row->id, row->username, row->email);
}

//INSERT
ExecuteResult execute_insert(Statement* statement , Table* table)
{
    if(table->num_rows >= TABLE_MAX_ROWS)
    {
        return EXECUTE_TABLE_FULL;
    }
    //statement->row_to_insert is a Row object storing insert values
    Row* row_to_insert = &(statement->row_to_insert);
    serialize_row(row_to_insert ,row_slot(table , table->num_rows));
    table->num_rows +=1;
    
    return EXECUTE_SUCCESS;
}
//SELECT
ExecuteResult execute_select(Statement* statement , Table* table)
{
    Row row;
    for(uint32_t i = 0 ; i< table->num_rows ; i++)
    {
        deserialize_row(row_slot(table , i) , &row);
        print_row(&row);
    }

    return EXECUTE_SUCCESS;
}
//Calls either insert/select
ExecuteResult execute_statement(Statement * statement , Table* table)
{
    switch(statement->type)
    {
        case (STATEMENT_INSERT):
            return execute_insert(statement , table);

        case (STATEMENT_SELECT):
            return execute_select(statement ,table);
    }
}
int main(int argc, char *argv[])
{
    Table* table = new_table();
    InputBuffer *input_buffer = new_input_buffer();
    
    while (true)
    {
        print_prompt();
        read_input(input_buffer);

        if(input_buffer->buffer[0] == '.')
        {
            switch(do_meta_command(input_buffer)) {
                case (META_COMMAND_SUCCESS):            //Meta commands -> start with .
                    continue;
                case(META_COMMAND_UNRECOGNIZED):
                    printf("Unrecognized Command '%s' \n" , input_buffer->buffer);
                    continue;

            }
        }
        Statement statement;
        switch(prepare_statement(input_buffer , &statement)){//Func returns type of statement based on input_buffer->buffer contents
    
            case (PREPARE_SUCCESS): //Input is valid(select , insert) and stored in statement(isnert only)
                break;
            case(PREPARE_SYNTAX_ERROR):
                printf("Syntax Error , Couldn't parse this statement \n");
                continue;
            case(PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecognized keyword at start '%s' \n" , input_buffer->buffer);
                continue;
        }

        //Pass statement as parameter
        switch(execute_statement(&statement , table))
        {
            case(EXECUTE_SUCCESS):
                printf("Executed. \n");
                break;
            case(EXECUTE_TABLE_FULL):
                printf("Table Full \n");
                break;
        }
    }
}
