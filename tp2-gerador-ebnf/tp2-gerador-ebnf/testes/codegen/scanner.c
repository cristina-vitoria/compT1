#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "scanner.h"

static FILE *entrada(const char *fonte)
{
    FILE *f = tmpfile();
    assert(f != NULL);
    assert(fputs(fonte, f) >= 0);
    rewind(f);
    scanner_iniciar(f);
    return f;
}

int main(void)
{
    FILE *f;
    Token t;
    f = entrada("\"a\\0b\" '\\0' '\\r' x-1");
    t = proximo_token();
    assert(t.tipo == STRINGCONST && t.tamanho == 3);
    assert(t.lexema[0] == 'a' && t.lexema[1] == 0 && t.lexema[2] == 'b');
    t = proximo_token();
    assert(t.tipo == CHARCONST && t.tamanho == 1 && t.lexema[0] == 0);
    t = proximo_token();
    assert(t.tipo == CHARCONST && t.lexema[0] == '\r');
    assert(proximo_token().tipo == ID);
    assert(proximo_token().tipo == MINUS);
    assert(proximo_token().tipo == INTEGERCONST);
    assert(proximo_token().tipo == END_OF_FILE);
    fclose(f);
    f = entrada("-1");
    t = proximo_token();
    assert(t.tipo == INTEGERCONST && strcmp(t.lexema, "-1") == 0);
    fclose(f);
    f = entrada("\"sem fim\n");
    t = proximo_token();
    assert(t.tipo == UNDEF && t.linha == 1);
    assert(strcmp(t.lexema, "String nao terminada") == 0);
    fclose(f);
    scanner_finalizar();
    f = entrada("/* nao termina");
    t = proximo_token();
    assert(t.tipo == UNDEF && t.linha == 1);
    assert(strcmp(t.lexema, "EOF em comentario") == 0);
    fclose(f);
    scanner_finalizar();
    f = tmpfile();
    assert(f != NULL);
    assert(fwrite("'\0'", 1, 3, f) == 3);
    rewind(f);
    scanner_iniciar(f);
    t = proximo_token();
    assert(t.tipo == UNDEF && strcmp(t.lexema, "Caractere nulo na entrada") == 0);
    fclose(f);
    scanner_finalizar();
    puts("ok     scanner Micro C: escapes, bytes nulos, posicao e reinicializacao");
    return 0;
}
