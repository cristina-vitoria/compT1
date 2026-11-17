#ifndef MICROC_SCANNER_INTERNO_H
#define MICROC_SCANNER_INTERNO_H

#include <stdio.h>
#include <stddef.h>
#include "tokens.h"

typedef struct {
    char *symbol;
    size_t tamanho;
    const char *error_msg;
} MicrocValor;

extern MicrocValor microc_yylval;
extern int linha_token;
int microc_yylex(void);
int microc_yylex_destroy(void);
extern char *microc_yytext;
void microc_preparar(FILE *entrada);
void microc_liberar_recursos(void);

#endif
