/* Adaptador do scanner Flex do TP1 para o analisador gerado. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scanner.h"
#include "scanner_interno.h"

void scanner_iniciar(FILE *entrada)
{
    microc_preparar(entrada);
}

Token proximo_token(void)
{
    Token t;
    const char *texto;
    size_t n;
    int tipo;

    free(microc_yylval.symbol);
    memset(&microc_yylval, 0, sizeof microc_yylval);
    memset(&t, 0, sizeof t);
    tipo = microc_yylex();
    t.tipo = (TokenType) tipo;
    t.linha = linha_token;
    if (tipo == ID || tipo == INTEGERCONST || tipo == CHARCONST
        || tipo == STRINGCONST) {
        texto = microc_yylval.symbol;
        n = microc_yylval.tamanho;
    } else {
        texto = tipo == END_OF_FILE ? "" : tipo == UNDEF
              ? microc_yylval.error_msg : microc_yytext;
        n = texto != NULL ? strlen(texto) : 0;
    }
    if (n >= sizeof t.lexema) {
        t.tipo = UNDEF;
        texto = "Lexema excede limite de 99 bytes";
        n = strlen(texto);
    }
    if (n && texto != NULL)
        memcpy(t.lexema, texto, n);
    t.lexema[n] = '\0';
    t.tamanho = n;
    return t;
}

void scanner_finalizar(void)
{
    microc_liberar_recursos();
    microc_yylex_destroy();
}
