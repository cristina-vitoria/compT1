/* Emite reconhecedores e verifica comprimentos conhecidos, sem depender
 * do parser gerado ou de bibliotecas externas de expressoes regulares. */
#include <assert.h>
#include <stdio.h>
#include "automato.h"

typedef struct {
    const char *regex, *entrada;
    unsigned long comprimento;
} Caso;

static const Caso casos[] = {
    { "[0-9]+", "123abc", 3 },
    { "[0-9]+", "abc", 0 },
    { "[a-zA-Z_][a-zA-Z0-9_]*", "_id12+", 5 },
    { "a|ab", "abx", 2 },
    { "ab|a", "abx", 2 },
    { "ab|a", "ax", 1 },
    { "abc|a", "abx", 1 },
    { "(a|b)*abb", "aabbx", 4 },
    { "(a|b)*abb", "abbab", 3 },
    { "ab*c", "acx", 2 },
    { "ab+c", "abbbcx", 5 },
    { "ab?c", "abcx", 3 },
    { "ab?c", "acx", 2 },
    { "ab?c", "abbc", 0 },
    { "a(b|)c", "acx", 2 },
    { "a(b|)c", "abcx", 3 },
    { "[^a-z]+", "12Zab", 3 },
    { "[^\\n]+", "abc\ndef", 3 },
    { ".+", "abc\ndef", 3 },
    { ".+", "\nabc", 0 },
    { "[ \t\r\n]+", " \t\r\nx", 4 },
    { "\\n\\t\\r\\f\\v", "\n\t\r\f\vx", 5 },
    { "\\(a\\)\\+\\?\\*", "(a)+?*x", 6 },
    { "\\/\\/[^\\n]*", "// oi\nx", 5 },
    { "[]a]+", "]aa]b", 4 },
    { "[-a]+", "-aa--b", 5 },
    { "[a-]+", "-aa--b", 5 },
    { "[\\]a]+", "]aab", 3 },
    { "[\200-\377]+", "\200\377a", 2 },
    { "a(b?)*c", "abbbcx", 5 },
    { "a", "", 0 },
    { "a", "ab", 1 },
    { "a", "ba", 0 }
};

static const char *invalidas[] = {
    "", "a*", "a?", "(a|)", "(a*)+", "[z-a]", "[abc",
    "(ab", "ab)", "*ab", "a**", "a+?", "\\", "\\x", "[]"
};

static void string_c(FILE *f, const char *s)
{
    unsigned char c;
    fputc('"', f);
    while (*s) {
        c = (unsigned char) *s++;
        if (c == '"' || c == '\\')
            fprintf(f, "\\%c", c);
        else if (c < 32 || c > 126)
            fprintf(f, "\\%03o", c);
        else
            fputc(c, f);
    }
    fputc('"', f);
}

int main(int argc, char **argv)
{
    FILE *f, *tmp;
    size_t i;
    char nome[64];
    assert(argc == 2);
    f = fopen(argv[1], "w");
    assert(f != NULL);
    fputs("#include <stddef.h>\n#include <stdio.h>\n#include <assert.h>\n", f);
    for (i = 0; i < sizeof casos / sizeof casos[0]; i++) {
        sprintf(nome, "casa_%lu", (unsigned long) i);
        assert(regex_emitir(f, nome, casos[i].regex) == 0);
    }
    fputs("int main(void)\n{\n", f);
    for (i = 0; i < sizeof casos / sizeof casos[0]; i++) {
        fprintf(f, "    assert(casa_%lu(", (unsigned long) i);
        string_c(f, casos[i].entrada);
        fprintf(f, ") == %lu);\n", casos[i].comprimento);
    }
    fputs("    puts(\"ok     33 casos de maior prefixo das regex\");\n"
          "    return 0;\n}\n", f);
    assert(fclose(f) == 0);
    tmp = tmpfile();
    assert(tmp != NULL);
    for (i = 0; i < sizeof invalidas / sizeof invalidas[0]; i++)
        assert(regex_emitir(tmp, "invalida", invalidas[i]) != 0);
    fclose(tmp);
    puts("ok     15 regex invalidas ou anulaveis rejeitadas");
    return 0;
}
