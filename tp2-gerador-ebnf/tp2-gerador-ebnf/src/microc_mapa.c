/*
 * microc_mapa.c - Correspondencia com os tokens de Micro C (ver microc_mapa.h).
 *
 * A tabela de literais segue o tokens.h do livro-texto (secao 2.3.1):
 *
 *   UNDEF, ID, END_OF_FILE, INTEGERCONST, CHARCONST, STRINGCONST,
 *   PLUS, MINUS, MUL, DIV, MOD, EQ, NEQ, LT, GT, LEQ, GEQ, AND, OR, NOT,
 *   ASSIGN, SEMICOLON, COMMA, LPAREN, RPAREN, LBRACE, RBRACE,
 *   LBRACKET, RBRACKET, MAIN, IF, ELSE, FOR, RETURN, INT, CHAR, PRINT
 *
 * Se o seu tokens.h do TP1 usar outros nomes, ajuste a tabela.
 *
 * A enumeracao externa e lida sem depender dos valores numericos dos tokens.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "microc_mapa.h"
#include "util.h"

static const char *const literais[][2] = {
    { "+",  "PLUS" },      { "-",  "MINUS" },     { "*",  "MUL" },
    { "/",  "DIV" },       { "%",  "MOD" },
    { "==", "EQ" },        { "!=", "NEQ" },       { "<",  "LT" },
    { ">",  "GT" },        { "<=", "LEQ" },       { ">=", "GEQ" },
    { "&&", "AND" },       { "||", "OR" },        { "!",  "NOT" },
    { "=",  "ASSIGN" },    { ";",  "SEMICOLON" }, { ",",  "COMMA" },
    { "(",  "LPAREN" },    { ")",  "RPAREN" },    { "{",  "LBRACE" },
    { "}",  "RBRACE" },    { "[",  "LBRACKET" },  { "]",  "RBRACKET" },
    { "main", "MAIN" },    { "if", "IF" },        { "else", "ELSE" },
    { "for", "FOR" },      { "return", "RETURN" },{ "int", "INT" },
    { "char", "CHAR" },    { "print", "PRINT" }
};

static char **nomes = NULL;
static int nnomes = 0, capnomes = 0;

const char *microc_nome_literal(const char *literal)
{
    size_t i;
    for (i = 0; i < sizeof(literais) / sizeof(literais[0]); i++)
        if (strcmp(literais[i][0], literal) == 0)
            return literais[i][1];
    return NULL;
}

static void adicionar_nome(const char *s, size_t n)
{
    if (nnomes == capnomes) {
        capnomes = capnomes ? 2 * capnomes : 64;
        nomes = (char **) xrealloc(nomes, (size_t) capnomes * sizeof(char *));
    }
    nomes[nnomes++] = xstrndup(s, n);
}

/* Le apenas os elementos lexicos necessarios para localizar o typedef.
 * Comentarios e literais C nao podem fornecer nomes de enumeradores. */
typedef struct {
    const char *inicio;
    size_t tamanho;
    int tipo;
} TokenC;

static TokenC proximo_c(const char **fonte)
{
    const char *p = *fonte, *inicio;
    TokenC t;
    int aspa;
    for (;;) {
        while (isspace((unsigned char) *p))
            p++;
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/'))
                p++;
            if (!*p)
                break;
            p += 2;
        } else if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n')
                p++;
        } else {
            break;
        }
    }
    inicio = p;
    t.tipo = (unsigned char) *p;
    if (isalpha((unsigned char) *p) || *p == '_') {
        do { p++; } while (isalnum((unsigned char) *p) || *p == '_');
        t.tipo = 'i';
    } else if (*p == '\'' || *p == '"') {
        aspa = *p++;
        while (*p && *p != aspa) {
            if (*p == '\\' && p[1])
                p++;
            p++;
        }
        if (*p)
            p++;
        t.tipo = 'q';
    } else if (*p) {
        p++;
    }
    t.inicio = inicio;
    t.tamanho = (size_t) (p - inicio);
    *fonte = p;
    return t;
}

static int palavra_c(TokenC t, const char *nome)
{
    return t.tipo == 'i' && t.tamanho == strlen(nome)
        && strncmp(t.inicio, nome, t.tamanho) == 0;
}

int microc_carregar_tokens_h(const char *caminho)
{
    char *fonte;
    const char *p;
    TokenC t;
    int espera_nome, nivel;

    microc_liberar();
    fonte = caminho != NULL ? ler_arquivo(caminho) : NULL;
    if (fonte == NULL) {
        diag_erro(0, 0, "nao foi possivel abrir cabecalho de tokens %s",
                  caminho != NULL ? caminho : "(ausente)");
        return -1;
    }
    p = fonte;
    while ((t = proximo_c(&p)).tipo != 0) {
        if (!palavra_c(t, "typedef"))
            continue;
        t = proximo_c(&p);
        if (!palavra_c(t, "enum"))
            continue;
        t = proximo_c(&p);
        if (t.tipo == 'i')
            t = proximo_c(&p); /* tag opcional */
        if (t.tipo != '{')
            continue;
        espera_nome = 1;
        nivel = 0;
        while ((t = proximo_c(&p)).tipo != 0) {
            if (t.tipo == '}' && nivel == 0)
                break;
            if (espera_nome) {
                if (t.tipo != 'i')
                    break;
                adicionar_nome(t.inicio, t.tamanho);
                espera_nome = 0;
            } else if (t.tipo == ',' && nivel == 0) {
                espera_nome = 1;
            } else if (t.tipo == '(' || t.tipo == '[' || t.tipo == '{') {
                nivel++;
            } else if (t.tipo == ')' || t.tipo == ']' || t.tipo == '}') {
                nivel--;
            }
        }
        if (t.tipo == '}' && palavra_c(proximo_c(&p), "TokenType")
            && proximo_c(&p).tipo == ';' && nnomes > 0) {
            free(fonte);
            return 0;
        }
        microc_liberar();
    }
    diag_erro(0, 0, "enumeracao typedef enum { ... } TokenType nao encontrada em %s",
              caminho);
    free(fonte);
    return -1;
}

int microc_token_existe(const char *nome)
{
    int i;
    for (i = 0; i < nnomes; i++)
        if (strcmp(nomes[i], nome) == 0)
            return 1;
    return 0;
}

void microc_liberar(void)
{
    int i;
    for (i = 0; i < nnomes; i++)
        free(nomes[i]);
    free(nomes);
    nomes = NULL;
    nnomes = capnomes = 0;
}
