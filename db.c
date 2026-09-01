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
	uint32_t cols;
	char *data;
} Table;

Table **tables = NULL;
int tables_count = 0;

char *
tokenise (char *str, char ***tokens, int *token_count)
{
	char c;
	int i = 0, in_quotes = 0, done = 0, token_index = 0;

	*token_count = 0;

	for (i = 0; !done; i++) {
		c = str[i];
		switch (c) {
		case '\0':
			(*token_count)++;
			done = 1;
		case ' ': /* fallthrough */
			(*token_count)++;
			break;
		}
	}

	*tokens = calloc(*token_count, sizeof **tokens);
	*token_count = 0, done = 0;

	for (i = 0; !done; i++) {
		c = str[i];
		switch (c) {
		case '\0':
			if (in_quotes) return "tokenise: Unclosed string";
			(*tokens)[*token_count] = realloc((*tokens)[*token_count], token_index + 1);
			(*tokens)[*token_count][token_index] = '\0';
			(*token_count)++;
			done = 1;
			break;
		case ' ':
			if (in_quotes) {
				(*tokens)[*token_count] = realloc((*tokens)[*token_count], token_index + 1);
				(*tokens)[*token_count][token_index] = ' ';
				token_index++;
			} else {
				(*tokens)[*token_count] = realloc((*tokens)[*token_count], token_index + 1);
				(*tokens)[*token_count][token_index] = '\0';
				token_index = 0;
				(*token_count)++;
			}
			break;
		case '"':
			if (i > 0 && str[i - 1] == '\\') {
				(*tokens)[*token_count] = realloc((*tokens)[*token_count], token_index + 1);
				(*tokens)[*token_count][token_index] = c;
				token_index++;
			} else {
				in_quotes = !in_quotes;
			}
			break;
		case '\\':
			if (i > 0 && str[i - 1] == '\\') {
				(*tokens)[*token_count] = realloc((*tokens)[*token_count], token_index + 1);
				(*tokens)[*token_count][token_index] = c;
				token_index++;
			}
			break;
		default:
			(*tokens)[*token_count] = realloc((*tokens)[*token_count], token_index + 1);
			(*tokens)[*token_count][token_index] = c;
			token_index++;
		}
	}

	return NULL;
}

char *
tableNew(Table *table, char *name, char *text[], int cols) {
	if (cols % 2 != 0)
		return "Invalid Schema";

	*table = (Table) {
		.name = strdup(name),
		.rows = 0,
		.cols = cols / 2,
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
	char **tokens = NULL, *err;
	int i;

	if (!(err = tokenise(text, &tokens, &i)))
		return err;

	Table *t = malloc(sizeof *t);
	
	if (!(err = tableNew(t, tokens[0], &tokens[1], i - 1)))
		return err;

	tables = realloc(tables, sizeof *tables * (tables_count + 1));
	tables[tables_count] = t;
	tables_count++;

	return NULL;
}

char *
tableInsert(Table *t, char **items, size_t items_count)
{
	if (items_count != t->cols)
		return "ins: Incorrect number of columns";

	size_t row_size = 0;
	for (uint32_t i = 0; i < t->cols; i++) {
		switch (t->schema[i].type) {
		case TYPE_BOOL:
			row_size += sizeof(int);
			break;
		case TYPE_BYTES:
		case TYPE_STR:
			row_size += sizeof(char *);
			break;
		case TYPE_NUM:
			row_size += sizeof(double);
			break;
		}
	}

	t->data = realloc(t->data, row_size * (t->rows + 1));
	char *insert = &t->data[t->rows * row_size];

	for (uint32_t i = 0; i < t->cols; i++) {
		switch (t->schema[i].type) {
		case TYPE_BOOL:
			if (items[i][0] == 't')
				*(int *)insert = 0;
			insert += sizeof(int);
			break;
		case TYPE_BYTES: /* fallthrough */
		case TYPE_STR:
			*(char **)insert = strdup(items[i]);
			insert += sizeof(int);
			break;
		case TYPE_NUM:
			*(double *)insert = atof(items[i]);
			insert += sizeof(double);
			break;
		}
	}

	t->rows++;
	return "ins: Success";
}

char *
commandInsert(char *row)
{
	char **tokens = NULL, *err;
	int i, j;

	if (!(err = tokenise(row, &tokens, &i)))
		return err;

	Table *t = NULL;
	for (j = 0; j < tables_count; j++)
		if (strcmp(tables[j]->name, tokens[0]) == 0)
			t = tables[j];

	if (!t) {
		char *err = malloc(sizeof("ins: No table named ") + strlen(tokens[0])); /* sizeof includes null at end */
		strcpy(err, "ins: No table named ");
		strcat(err, tokens[0]);
		return err;
	}

	return tableInsert(t, &tokens[1], i - 1);
}

int main(void) {
	printf("%s", commandNew(strdup("ages age n name s")));
	printf("%s", commandInsert(strdup("ages 10 \"joe smith\" 67 bob")));
	printf("%10s", tables[0]->data);
	return 0;
}
