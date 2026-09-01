#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

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

	if ((err = tokenise(text, &tokens, &i)))
		return err;

	Table *t = malloc(sizeof *t);
	
	if ((err = tableNew(t, tokens[0], &tokens[1], i - 1)))
		return err;

	tables = realloc(tables, sizeof *tables * (tables_count + 1));
	tables[tables_count] = t;
	tables_count++;

	return "new: Success\n";
}

size_t
typeSize(Type t) {
	switch (t) {
	case TYPE_BOOL:
		return sizeof(int);
	case TYPE_BYTES:
	case TYPE_STR:
		return sizeof(char *);
	case TYPE_NUM:
		return sizeof(double);
	}
}

size_t
tableRowSize(Table *t)
{
	size_t row_size = 0;
	for (uint32_t i = 0; i < t->cols; i++) {
		row_size += typeSize(t->schema[i].type);
	}

	return row_size;
}

char *
tableInsert(Table *t, char **items, uint32_t items_count)
{
	if (items_count / 2 != t->cols)
		return "ins: Incorrect number of columns\n";

	size_t row_size = tableRowSize(t);

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
	return "ins: Success\n";
}

char *
commandInsert(char *row)
{
	char **tokens = NULL, *err;
	int i, j;

	if ((err = tokenise(row, &tokens, &i)))
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

char *
tableGet(Table *table, char **criteria, int criteria_count)
{
	size_t row_size = tableRowSize(table);

	for (uint32_t i = 0; i < table->rows; i++) {
		int passed = 1;
		for (int j = 0; j < criteria_count; j++) {
			char *col = criteria[j];
			char *mid = strchr(col, '=');
			char *val = mid + 1;

			if (!mid) return "get: Invalid criteria";

			*mid = '\0';

			int found = 0;
			char *current_cell = &table->data[row_size * i];
			for (uint32_t k = 0; k < table->cols; k++) {
				char *cell = current_cell;
				current_cell += typeSize(table->schema[k].type);

				if (strcmp(table->schema[k].name, col) != 0)
					continue;

				found = 1;
				switch (table->schema[k].type) {
				case TYPE_STR:
				case TYPE_BYTES:
					if (strcmp(cell, val) != 0)
						passed = 0;
					break;
				case TYPE_NUM:
					if (*(double *)cell != strtod(val, NULL))
						passed = 0;
					break;
				case TYPE_BOOL:
					if (cell[0] != val[0])
						passed = 0;
					break;
				}
				break;
			}
			
			if (!found)
				return "get: No column matching criteria";

			if (!passed)
				break;

			/* todo: return all rows */
			char ret[256];
			snprintf(ret, 256, "get: index %d\n", i);
			return strdup(ret);
		}
	}

	return "";
}

char *
commandGet(char *cmd)
{
	char **tokens;
	int token_count, j;
	tokenise(cmd, &tokens, &token_count);

	Table *t = NULL;
	for (j = 0; j < tables_count; j++) {
		if (strcmp(tables[j]->name, tokens[0]) == 0)
			t = tables[j];
	}

	if (!t) {
		char *err = malloc(sizeof("get: No table named ") + strlen(tokens[0])); /* sizeof includes null at end */
		strcpy(err, "get: No table named ");
		strcat(err, tokens[0]);
		return err;
	}

	return tableGet(t, &tokens[1], token_count - 1);
}


int main(void) {
	printf("%s", commandNew(strdup("ages age n name s")));
	printf("%s", commandInsert(strdup("ages 21 \"joe smith\" 10 bob")));
	printf("%s", commandGet(strdup("ages name=\"joe smith\" age=21")));
	return 0;
}
