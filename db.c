#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { TYPE_NUM, TYPE_STR, TYPE_BOOL, TYPE_BYTES } Type;

typedef struct {
  Type type;
  char *name;
} Col;

typedef struct {
  Col *schema;
  char *name;

  uint32_t rows;
  void *data;
} Table;

Table **tables = NULL;
size_t tables_count = 0;

char *
tableNew(Table *table, char *name, char *text[], int cols) {
  if (cols % 2 != 0)
    return "Invalid Schema";

  *table = (Table) {
    .name = strdup(name),
    .rows = 0,
    .schema = malloc(sizeof *table->schema * cols / 2),
    .data = NULL
  };

  for (int i = 0; i < cols; i += 2) {
    table->schema[i / 2].name = strdup(text[i]);

    switch (text[i / 2 + 1][0]) {
    case 'n':
      table->schema[i / 2].type = TYPE_NUM;
      break;
    case 's':
      table->schema[i / 2].type = TYPE_STR;
      break;
    case 'b':
      table->schema[i / 2].type = TYPE_BOOL;
      break;
    case 'x':
      table->schema[i / 2].type = TYPE_BYTES;
      break;
    }
  }

  return NULL;
}

char *
commandNew(char *text)
{
  char *ctx, *token, **tokens = NULL, *err;
  size_t i;

  for (i = 0, token = strtok_r(text, " ", &ctx);
       token;
       i++, token = strtok_r(NULL, " ", &ctx)) {
    tokens = realloc(tokens, sizeof *tokens * (i + 1));
    tokens[i] = strdup(token);
  }

  Table *t = malloc(sizeof *t);
  
  if ((err = tableNew(t, tokens[0], &tokens[1], i - 1)) != NULL)
    return err;

  tables = realloc(tables, sizeof *tables * (tables_count + 1));
  tables[tables_count] = t;
  tables_count++;

  return NULL;
}

int main(void) {
  commandNew(strdup("ages age n name s"));

  printf("%s", tables[0]->name);
  printf("%s", tables[0]->schema[0].name);
  printf("%s", tables[0]->schema[1].name);
  return 0;
}
