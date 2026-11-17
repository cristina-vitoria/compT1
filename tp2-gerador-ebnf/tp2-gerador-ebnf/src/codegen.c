/*
 * codegen.c - Geracao do analisador sintatico (etapa 5).
 *
 * Estrutura do arquivo .c gerado:
 *
 *   1. includes e conjuntos constantes (conj_0, conj_1, ...)
 *   2. estado e primitivas: avancar, match, erro_sintatico, arvore
 *   3. modo proprio: analisador lexico gerado a partir de TOKENS
 *      modo Micro C: tokens obtidos de proximo_token() (scanner do TP1)
 *   4. leitura de um arquivo de tokens (opcao --tokens do analisador)
 *   5. um procedimento parse_X() por nao terminal
 *   6. main
 *
 * As construcoes EBNF sao traduzidas usando os conjuntos precalculados.
 * O modulo automato.c compila as regex em reconhecedores deterministas.
 */
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "codegen.h"
#include "automato.h"
#include "microc_mapa.h"
#include "util.h"

/* ------------------------------------------------------------------ */
/* Emissao de texto                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    FILE *f;
    int nivel;          /* nivel de indentacao (4 espacos por nivel) */
} Emissor;

/* Emite uma linha indentada; fmt segue printf. */
static void emit(Emissor *em, const char *fmt, ...)
{
    int i;
    va_list ap;
    for (i = 0; i < em->nivel; i++)
        fputs("    ", em->f);
    va_start(ap, fmt);
    vfprintf(em->f, fmt, ap);
    va_end(ap);
    fputc('\n', em->f);
}

/* Emite linhas fixas, sem interpretar '%'. A lista termina em NULL. */
static void emit_linhas(FILE *f, const char *const *linhas)
{
    for (; *linhas != NULL; linhas++) {
        fputs(*linhas, f);
        fputc('\n', f);
    }
}

/* Texto dentro de comentario C: evita fechar o comentario por engano. */
static void emitir_comentario(FILE *f, const char *s)
{
    for (; *s != '\0'; s++) {
        if (s[0] == '*' && s[1] == '/') {
            fputs("* /", f);
            s++;
        } else if (*s == '\n') {
            fputc(' ', f);
        } else {
            fputc(*s, f);
        }
    }
}

/* String C entre aspas, com escapes. */
static void emitir_string_c(FILE *f, const char *s)
{
    fputc('"', f);
    for (; *s != '\0'; s++) {
        unsigned char c = (unsigned char) *s;
        if (c == '"' || c == '\\')
            fprintf(f, "\\%c", c);
        else if (c == '\n')
            fputs("\\n", f);
        else if (c == '\t')
            fputs("\\t", f);
        else if (c < 32 || c > 126)
            fprintf(f, "\\%03o", c);
        else
            fputc(c, f);
    }
    fputc('"', f);
}

/* ------------------------------------------------------------------ */
/* Nomes dos tokens no codigo gerado                                  */
/* ------------------------------------------------------------------ */

static const char *const nomes_simbolos[][2] = {
    { "+", "MAIS" }, { "-", "MENOS" }, { "*", "VEZES" }, { "/", "BARRA" },
    { "%", "PORCENTO" }, { "(", "ABRE_PAR" }, { ")", "FECHA_PAR" },
    { "[", "ABRE_COL" }, { "]", "FECHA_COL" }, { "{", "ABRE_CHAVE" },
    { "}", "FECHA_CHAVE" }, { ";", "PV" }, { ",", "VIRGULA" },
    { ".", "PONTO" }, { ":", "DOIS_PONTOS" }, { "=", "IGUAL" },
    { "==", "IGUAL_IGUAL" }, { "!=", "DIFERENTE" }, { "<", "MENOR" },
    { ">", "MAIOR" }, { "<=", "MENOR_IGUAL" }, { ">=", "MAIOR_IGUAL" },
    { "&&", "E_LOGICO" }, { "||", "OU_LOGICO" }, { "!", "NAO" },
    { ":=", "ATRIBUICAO" }, { "->", "SETA" }
};

static int eh_palavra(const char *s)
{
    if (!isalpha((unsigned char) *s) && *s != '_')
        return 0;
    for (; *s != '\0'; s++)
        if (!isalnum((unsigned char) *s) && *s != '_')
            return 0;
    return 1;
}

static int nome_em_uso(const Gramatica *g, int ate, const char *nome)
{
    int i;
    for (i = 0; i < ate; i++)
        if (strcmp(g->terms[i].nome_c, nome) == 0)
            return 1;
    return 0;
}

/* Define g->terms[i].nome_c para todos os terminais. */
static void nomear_terminais(Gramatica *g, int microc)
{
    int i, tentativa;
    size_t k;
    char buf[256];
    Terminal *t;

    for (i = 0; i < g->nterms; i++) {
        t = &g->terms[i];
        free(t->nome_c);
        if (i == TERM_FIM) {
            t->nome_c = xstrdup(microc ? "END_OF_FILE" : "T_FIM");
            continue;
        }
        if (microc) {
            t->nome_c = xstrdup(t->literal ? microc_nome_literal(t->nome) : t->nome);
            continue;
        }
        buf[0] = '\0';
        if (!t->literal) {
            sprintf(buf, "T_%.200s", t->nome);
        } else if (eh_palavra(t->nome) && strlen(t->nome) < 200) {
            sprintf(buf, "T_%s", t->nome);
            for (k = 2; buf[k] != '\0'; k++)
                buf[k] = (char) toupper((unsigned char) buf[k]);
        } else {
            for (k = 0; k < sizeof(nomes_simbolos) / sizeof(nomes_simbolos[0]); k++)
                if (strcmp(nomes_simbolos[k][0], t->nome) == 0)
                    sprintf(buf, "T_%s", nomes_simbolos[k][1]);
            if (buf[0] == '\0')
                sprintf(buf, "T_LIT_%d", i);
        }
        tentativa = 0;
        while (nome_em_uso(g, i, buf))
            sprintf(buf, "T_GERADO_%d_%d", i, tentativa++);
        t->nome_c = xstrdup(buf);
    }
}

/* ------------------------------------------------------------------ */
/* Conjuntos constantes                                               */
/* ------------------------------------------------------------------ */

#define MAX_COMPARACOES 3   /* ate 3 tokens: tok.tipo == A || ...; acima: pertence */

static Bitset *conjuntos = NULL;
static int nconj = 0, capconj = 0;

static int bitset_igual(const Bitset *a, const Bitset *b)
{
    int i;
    for (i = 0; i < a->n; i++)
        if (bitset_tem(a, i) != bitset_tem(b, i))
            return 0;
    return 1;
}

/* Registra um conjunto e devolve seu numero N (emitido como conj_N).
 * Conjuntos iguais compartilham a mesma constante. */
static int registrar_conjunto(const Bitset *b)
{
    int i;
    for (i = 0; i < nconj; i++)
        if (bitset_igual(&conjuntos[i], b))
            return i;
    if (nconj == capconj) {
        capconj = capconj ? 2 * capconj : 16;
        conjuntos = (Bitset *) xrealloc(conjuntos, (size_t) capconj * sizeof(Bitset));
    }
    bitset_iniciar(&conjuntos[nconj], b->n);
    bitset_copiar(&conjuntos[nconj], b);
    return nconj++;
}

static void liberar_conjuntos(void)
{
    int i;
    for (i = 0; i < nconj; i++)
        bitset_liberar(&conjuntos[i]);
    free(conjuntos);
    conjuntos = NULL;
    nconj = capconj = 0;
}

static void emitir_conjuntos(FILE *f, const Gramatica *g)
{
    int i, t;
    for (i = 0; i < nconj; i++) {
        fprintf(f, "static const int conj_%d[] = { ", i);
        for (t = bitset_proximo(&conjuntos[i], 0); t >= 0;
             t = bitset_proximo(&conjuntos[i], t + 1))
            fprintf(f, "%s, ", g->terms[t].nome_c);
        fputs("-1 };\n", f);
    }
}

/* Escreve a condicao "o token corrente pertence a b":
 *   tok.tipo == T_A                     (um token)
 *   tok.tipo == T_A || tok.tipo == T_B  (ate MAX_COMPARACOES)
 *   pertence(tok.tipo, conj_N)          (mais tokens) */
static void emitir_teste(FILE *f, const Gramatica *g, const Bitset *b)
{
    int t, n = bitset_tamanho(b), primeiro = 1;
    if (n == 0) {
        fputs("0", f);
    } else if (n <= MAX_COMPARACOES) {
        for (t = bitset_proximo(b, 0); t >= 0; t = bitset_proximo(b, t + 1)) {
            fprintf(f, "%stok.tipo == %s", primeiro ? "" : " || ", g->terms[t].nome_c);
            primeiro = 0;
        }
    } else {
        fprintf(f, "pertence(tok.tipo, conj_%d)", registrar_conjunto(b));
    }
}

/* Linha "<antes><teste><depois>", ex.: emit_teste(em, "while (", g, b, ") {") */
static void emit_teste(Emissor *em, const char *antes, const Gramatica *g,
                       const Bitset *b, const char *depois)
{
    int i;
    for (i = 0; i < em->nivel; i++)
        fputs("    ", em->f);
    fputs(antes, em->f);
    emitir_teste(em->f, g, b);
    fputs(depois, em->f);
    fputc('\n', em->f);
}

/* Linha "erro_sintatico(conj_N);" com N = conjunto dos tokens esperados. */
static void emit_erro(Emissor *em, const Bitset *esperados)
{
    emit(em, "erro_sintatico(conj_%d);", registrar_conjunto(esperados));
}

/* ------------------------------------------------------------------ */
/* Traducao das expressoes (esquema da secao 4 do enunciado)          */
/* ------------------------------------------------------------------ */

static void gen_expr(Emissor *em, const Gramatica *g, const Expr *e)
{
    int i, anulavel = -1, escolhas = 0;
    switch (e->tipo) {
    case E_TERM:
        emit(em, "match(%s);", g->terms[e->simbolo].nome_c);
        break;
    case E_NAOTERM:
        emit(em, "parse_%s();", g->nts[e->simbolo].nome);
        break;
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++)
            gen_expr(em, g, e->filhos[i]);
        break;
    case E_ALT:
        for (i = 0; i < e->nfilhos; i++) {
            if (e->filhos[i]->anulavel) {
                anulavel = i;
                continue;
            }
            emit_teste(em, escolhas ? "else if (" : "if (", g,
                       &e->filhos[i]->first, ") {");
            em->nivel++;
            gen_expr(em, g, e->filhos[i]);
            em->nivel--;
            emit(em, "}");
            escolhas++;
        }
        if (escolhas) {
            emit(em, "else {");
            em->nivel++;
        }
        if (anulavel >= 0)
            gen_expr(em, g, e->filhos[anulavel]);
        else
            emit_erro(em, &e->first);
        if (escolhas) {
            em->nivel--;
            emit(em, "}");
        }
        break;
    case E_OPC: case E_REP:
        emit_teste(em, e->tipo == E_OPC ? "if (" : "while (", g,
                   &e->filhos[0]->first, ") {");
        em->nivel++;
        gen_expr(em, g, e->filhos[0]);
        em->nivel--;
        emit(em, "}");
        break;
    case E_VAZIO:
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Leitura do arquivo de tokens (opcao --tokens do analisador gerado) */
/* ------------------------------------------------------------------ */

/* Emite "static Token token_de_arquivo(void)", que le o proximo token
 * do arquivo aberto em arquivo_tokens, no formato gravado pelo lexer do
 * TP1 (livro-texto, secao 3.3.2):
 *
 *   Token: tipo = <numero>, lexema = '<texto>', linha = <numero>
 *
 * Ao chegar ao fim do arquivo devolve um token TOKEN_FIM. */
static void gen_token_de_arquivo(FILE *f, const Gramatica *g, int microc)
{
    int i;
    static const char *const linhas[] = {
        "/* Le o formato numerico do livro e os nomes impressos pelo TP1. */",
        "static void erro_arquivo_tokens(unsigned long linha)",
        "{",
        "    fprintf(stderr, \"arquivo de tokens, linha %lu: formato invalido\\n\", linha);",
        "    exit(EXIT_FAILURE);",
        "}",
        "",
        "static char *brancos_tokens(char *p)",
        "{",
        "    while (isspace((unsigned char) *p)) p++;",
        "    return p;",
        "}",
        "",
        "static char *campo_tokens(char *p, const char *nome, unsigned long linha)",
        "{",
        "    size_t n = strlen(nome);",
        "    p = brancos_tokens(p);",
        "    if (*p == ',') p = brancos_tokens(p + 1);",
        "    if (strncmp(p, nome, n) != 0) erro_arquivo_tokens(linha);",
        "    p = brancos_tokens(p + n);",
        "    if (*p != '=') erro_arquivo_tokens(linha);",
        "    return brancos_tokens(p + 1);",
        "}",
        "",
        "static int inteiro_tokens(char **p, unsigned long linha)",
        "{",
        "    char *fim;",
        "    long n;",
        "    errno = 0;",
        "    n = strtol(*p, &fim, 10);",
        "    if (fim == *p || errno == ERANGE || n < 1 || n > INT_MAX)",
        "        erro_arquivo_tokens(linha);",
        "    *p = fim;",
        "    return (int) n;",
        "}",
        "",
        "static Token token_de_arquivo(void)",
        "{",
        "    static unsigned long linha_arquivo = 0;",
        "    static int ultima_linha = 1;",
        "    Token t;",
        "    char linha[4096], tipo[128], *p, *fim, *numero_fim;",
        "    size_t n;",
        "    long numero;",
        "    memset(&t, 0, sizeof t);",
        "    t.tipo = TOKEN_FIM;",
        "    t.linha = ultima_linha;",
        "    while (fgets(linha, sizeof linha, arquivo_tokens) != NULL) {",
        "        linha_arquivo++;",
        "        n = strlen(linha);",
        "        if (n == sizeof linha - 1 && linha[n - 1] != '\\n')",
        "            erro_arquivo_tokens(linha_arquivo);",
        "        p = brancos_tokens(linha);",
        "        if (*p == '\\0') continue;",
        "        if (strncmp(p, \"Token:\", 6) != 0) erro_arquivo_tokens(linha_arquivo);",
        "        p = campo_tokens(p + 6, \"tipo\", linha_arquivo);",
        "        fim = p;",
        "        while (*fim && *fim != ',' && !isspace((unsigned char) *fim)) fim++;",
        "        n = (size_t) (fim - p);",
        "        if (n == 0 || n >= sizeof tipo) erro_arquivo_tokens(linha_arquivo);",
        "        memcpy(tipo, p, n);",
        "        tipo[n] = '\\0';",
        "        errno = 0;",
        "        numero = strtol(tipo, &numero_fim, 10);",
        "        if (*numero_fim == '\\0' && numero_fim != tipo) {",
        "            if (errno == ERANGE || numero < 0 || numero > INT_MAX)",
        "                erro_arquivo_tokens(linha_arquivo);",
        "            t.tipo = numero;",
        "        } else {",
        "            t.tipo = tipo_de_nome(tipo);",
        "            if ((int) t.tipo == -1) erro_arquivo_tokens(linha_arquivo);",
        "        }",
        "        p = campo_tokens(fim, \"lexema\", linha_arquivo);",
        "        if (*p != '\\'') erro_arquivo_tokens(linha_arquivo);",
        "        p++;",
        "        fim = strrchr(p, '\\'');",
        "        if (fim == NULL) erro_arquivo_tokens(linha_arquivo);",
        "        n = (size_t) (fim - p);",
        "        if (n >= sizeof t.lexema) erro_arquivo_tokens(linha_arquivo);",
        "        memcpy(t.lexema, p, n);",
        "        t.lexema[n] = '\\0';",
        "        t.tamanho = n;",
        "        p = campo_tokens(fim + 1, \"linha\", linha_arquivo);",
        "        t.linha = inteiro_tokens(&p, linha_arquivo);",
        "        if (*brancos_tokens(p) != '\\0') erro_arquivo_tokens(linha_arquivo);",
        "        ultima_linha = t.linha;",
        "        if ((int) t.tipo == TOKEN_FIM) {",
        "            while (fgets(linha, sizeof linha, arquivo_tokens) != NULL) {",
        "                linha_arquivo++;",
        "                if (*brancos_tokens(linha) != '\\0') erro_arquivo_tokens(linha_arquivo);",
        "            }",
        "            if (ferror(arquivo_tokens)) erro_arquivo_tokens(linha_arquivo + 1);",
        "        }",
        "        return t;",
        "    }",
        "    if (ferror(arquivo_tokens)) erro_arquivo_tokens(linha_arquivo + 1);",
        "    return t;",
        "}",
        "",
        NULL
    };
    fputs("static int tipo_de_nome(const char *nome)\n{\n", f);
    for (i = 0; i < g->nterms; i++) {
        fputs("    if (strcmp(nome, ", f);
        emitir_string_c(f, g->terms[i].nome_c);
        fprintf(f, ") == 0) return %s;\n", g->terms[i].nome_c);
        if (!g->terms[i].literal) {
            fputs("    if (strcmp(nome, ", f);
            emitir_string_c(f, g->terms[i].nome);
            fprintf(f, ") == 0) return %s;\n", g->terms[i].nome_c);
        }
    }
    if (microc)
        fputs("    if (strcmp(nome, \"UNDEF\") == 0) return UNDEF;\n", f);
    fputs("    return -1;\n}\n\n", f);
    for (i = 0; linhas[i] != NULL; i++) {
        fprintf(f, "%s\n", linhas[i]);
        if (!microc && strcmp(linhas[i], "    t.linha = ultima_linha;") == 0)
            fputs("    t.coluna = 1;\n", f);
        if (!microc && strcmp(linhas[i], "        t.linha = inteiro_tokens(&p, linha_arquivo);") == 0)
            fputs("        p = brancos_tokens(p);\n"
                  "        if (*p != '\\0') {\n"
                  "            p = campo_tokens(p, \"coluna\", linha_arquivo);\n"
                  "            t.coluna = inteiro_tokens(&p, linha_arquivo);\n"
                  "        }\n", f);
    }
}

/* ------------------------------------------------------------------ */
/* Partes fixas do codigo gerado                                      */
/* ------------------------------------------------------------------ */

static const char *const rt_estado[] = {
    "/* ---- estado do analisador ---- */",
    "",
    "static FILE *entrada_fonte = NULL;",
    "static Token tok;                  /* token corrente (lookahead) */",
    "static int imprimir_arvore = 0;    /* opcao --tree */",
    "static int nivel_arvore = 0;",
    "static FILE *arquivo_tokens = NULL; /* opcao --tokens */",
    "",
    "static Token token_de_arquivo(void);",
    "",
    NULL
};

static const char *const rt_primitivas[] = {
    "/* ---- primitivas ---- */",
    "",
    "int pertence(int tipo, const int *conjunto)",
    "{",
    "    for (; *conjunto != -1; conjunto++)",
    "        if (*conjunto == tipo)",
    "            return 1;",
    "    return 0;",
    "}",
    "",
    "/* Reporta o erro com os tokens esperados (lista terminada em -1) e encerra. */",
    "void erro_sintatico(const int *esperados)",
    "{",
    "    int i;",
    "    escrever_posicao();",
    "    if ((int) tok.tipo == TOKEN_FIM)",
    "        fprintf(stderr, \"encontrado fim de arquivo, \");",
    "    else",
    "        fprintf(stderr, \"encontrado '%s', \", tok.lexema);",
    "    if (esperados[0] != -1 && esperados[1] == -1) {",
    "        fprintf(stderr, \"esperado %s\\n\", nome_token(esperados[0]));",
    "    } else {",
    "        fprintf(stderr, \"esperado um de {\");",
    "        for (i = 0; esperados[i] != -1; i++)",
    "            fprintf(stderr, \"%s%s\", i > 0 ? \", \" : \"\", nome_token(esperados[i]));",
    "        fprintf(stderr, \"}\\n\");",
    "    }",
    "    exit(EXIT_FAILURE);",
    "}",
    "",
    "static void recuo(void)",
    "{",
    "    int i;",
    "    for (i = 0; i < nivel_arvore; i++)",
    "        fputs(\"  \", stdout);",
    "}",
    "",
    "void arvore_entra(const char *nao_terminal)",
    "{",
    "    if (imprimir_arvore) {",
    "        recuo();",
    "        printf(\"%s\\n\", nao_terminal);",
    "    }",
    "    nivel_arvore++;",
    "}",
    "",
    "void arvore_sai(void)",
    "{",
    "    nivel_arvore--;",
    "}",
    "",
    "/* Consome o token corrente se for do tipo esperado; senao, erro. */",
    "void match(int esperado)",
    "{",
    "    int lista[2];",
    "    if ((int) tok.tipo != esperado) {",
    "        lista[0] = esperado;",
    "        lista[1] = -1;",
    "        erro_sintatico(lista);",
    "    }",
    "    if (SUPORTE_ARVORE && imprimir_arvore) {",
    "        recuo();",
    "        if (nome_token(tok.tipo)[0] == '\\'')",
    "            printf(\"%s\\n\", nome_token(tok.tipo));",
    "        else",
    "            printf(\"%s '%s'\\n\", nome_token(tok.tipo), tok.lexema);",
    "    }",
    "    avancar();",
    "}",
    "",
    NULL
};

/* Lexico proprio: partes fixas antes e depois das funcoes de regex. */
static const char *const rt_lexico_estado[] = {
    "/* ---- analisador lexico gerado a partir de TOKENS ---- */",
    "",
    "static char *fonte = NULL;",
    "static size_t pos = 0;",
    "static int linha = 1, coluna = 1;",
    "",
    "static char *ler_tudo(FILE *f)",
    "{",
    "    size_t cap = 4096, n = 0, lidos;",
    "    char *buf = (char *) malloc(cap), *novo;",
    "    if (buf == NULL) {",
    "        fprintf(stderr, \"memoria insuficiente\\n\");",
    "        exit(EXIT_FAILURE);",
    "    }",
    "    while ((lidos = fread(buf + n, 1, cap - n - 1, f)) > 0) {",
    "        n += lidos;",
    "        if (cap - n - 1 == 0) {",
    "            cap *= 2;",
    "            novo = (char *) realloc(buf, cap);",
    "            if (novo == NULL) {",
    "                free(buf);",
    "                fprintf(stderr, \"memoria insuficiente\\n\");",
    "                exit(EXIT_FAILURE);",
    "            }",
    "            buf = novo;",
    "        }",
    "    }",
    "    if (ferror(f) || memchr(buf, 0, n) != NULL) {",
    "        fprintf(stderr, \"erro de leitura ou byte nulo na entrada\\n\");",
    "        free(buf);",
    "        exit(EXIT_FAILURE);",
    "    }",
    "    buf[n] = '\\0';",
    "    return buf;",
    "}",
    "",
    "static void consumir(size_t n)",
    "{",
    "    for (; n > 0 && fonte[pos] != '\\0'; n--, pos++) {",
    "        if (fonte[pos] == '\\n') {",
    "            linha++;",
    "            coluna = 1;",
    "        } else {",
    "            coluna++;",
    "        }",
    "    }",
    "}",
    "",
    NULL
};

static void escrever_cabecalho_c(FILE *f, const OpcoesGeracao *op)
{
    fputs("/*\n * Analisador sintatico descendente recursivo gerado a partir de ", f);
    emitir_comentario(f, op->origem);
    fputs(".\n * Arquivo gerado automaticamente: nao edite.\n */\n", f);
}

/* ------------------------------------------------------------------ */
/* Arquivo .h                                                         */
/* ------------------------------------------------------------------ */

static int gerar_h(FILE *f, const Gramatica *g, const OpcoesGeracao *op)
{
    int i;

    escrever_cabecalho_c(f, op);
    fputs("#ifndef PARSER_GERADO_H\n#define PARSER_GERADO_H\n\n#include <stddef.h>\n\n", f);
    if (op->microc) {
        fputs("#include \"scanner.h\"   /* scanner do TP1: Token, TokenType, proximo_token */\n\n", f);
        fputs("#define TOKEN_FIM END_OF_FILE\n\n", f);
    } else {
        fputs("typedef enum {\n", f);
        for (i = 0; i < g->nterms; i++) {
            fprintf(f, "    %s%s   /* ", g->terms[i].nome_c, i + 1 < g->nterms ? "," : "");
            emitir_comentario(f, g->terms[i].exibicao);
            fputs(" */\n", f);
        }
        fputs("} TipoToken;\n\n", f);
        fputs("typedef struct {\n    int tipo;\n    char lexema[100];\n"
              "    int linha;\n    int coluna;\n    size_t tamanho;\n} Token;\n\n", f);
        fputs("#define TOKEN_FIM T_FIM\n\n", f);
    }
    fputs("const char *nome_token(int tipo);\n", f);
    fputs("int  pertence(int tipo, const int *conjunto);\n", f);
    fputs("void avancar(void);\n", f);
    fputs("void match(int esperado);\n", f);
    fputs("void erro_sintatico(const int *esperados);\n", f);
    fputs("void arvore_entra(const char *nao_terminal);\n", f);
    fputs("void arvore_sai(void);\n\n", f);
    for (i = 0; i < g->nnts; i++)
        if (g->nts[i].regra != NULL)
            fprintf(f, "void parse_%s(void);\n", g->nts[i].nome);
    fputs("\n#endif\n", f);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Arquivo .c                                                         */
/* ------------------------------------------------------------------ */

static void gerar_nome_token(FILE *f, const Gramatica *g)
{
    int i;
    fputs("const char *nome_token(int tipo)\n{\n    switch (tipo) {\n", f);
    for (i = 0; i < g->nterms; i++) {
        fprintf(f, "    case %s: return ", g->terms[i].nome_c);
        emitir_string_c(f, i == TERM_FIM ? "fim de arquivo" : g->terms[i].exibicao);
        fputs(";\n", f);
    }
    fputs("    default: return \"token desconhecido\";\n    }\n}\n\n", f);
}

static void gerar_posicao_e_avancar(FILE *f, int microc)
{
    fputs("static void escrever_posicao(void)\n{\n", f);
    if (microc)
        fputs("    fprintf(stderr, \"linha %d: \", tok.linha);\n", f);
    else
        fputs("    fprintf(stderr, \"linha %d, coluna %d: \", tok.linha, tok.coluna);\n", f);
    fputs("}\n\n", f);

    fputs("/* Le o proximo token para tok. */\nvoid avancar(void)\n{\n", f);
    fputs("    if (arquivo_tokens != NULL)\n        tok = token_de_arquivo();\n    else\n", f);
    if (microc) {
        fputs("        tok = proximo_token();\n", f);
        fputs("    if (tok.tipo == UNDEF) {\n", f);
        fputs("        fprintf(stderr, \"linha %d: erro lexico: %s\\n\", tok.linha, tok.lexema);\n", f);
        fputs("        exit(EXIT_FAILURE);\n    }\n", f);
    } else {
        fputs("        tok = proximo_token_gerado();\n", f);
    }
    fputs("}\n\n", f);
}

static int gerar_lexico(FILE *f, const Gramatica *g)
{
    int i, erros = 0;
    char nome[300];

    emit_linhas(f, rt_lexico_estado);

    for (i = 0; i < g->nskips; i++) {
        sprintf(nome, "casa_skip_%d", i);
        fputs("/* SKIP: ", f);
        emitir_comentario(f, g->skips[i]);
        fputs(" */\n", f);
        if (regex_emitir(f, nome, g->skips[i]) != 0)
            erros++;
    }
    for (i = 1; i < g->nterms; i++) {
        if (g->terms[i].literal)
            continue;
        sprintf(nome, "casa_%.250s", g->terms[i].nome_c);
        fprintf(f, "/* %s: ", g->terms[i].nome);
        emitir_comentario(f, g->terms[i].regex ? g->terms[i].regex : "");
        fputs(" */\n", f);
        if (regex_emitir(f, nome, g->terms[i].regex ? g->terms[i].regex : "")
            != 0)
            erros++;
    }

    /* literais: comparacao direta, com prioridade em caso de empate */
    fputs("static const char *const texto_literais[] = {\n", f);
    for (i = 1; i < g->nterms; i++)
        if (g->terms[i].literal) {
            fputs("    ", f);
            emitir_string_c(f, g->terms[i].nome);
            fputs(",\n", f);
        }
    fputs("    NULL\n};\n\nstatic const int tipo_literais[] = {\n", f);
    for (i = 1; i < g->nterms; i++)
        if (g->terms[i].literal)
            fprintf(f, "    %s,\n", g->terms[i].nome_c);
    fputs("    -1\n};\n\n", f);

    fputs("/* Reconhece o maior prefixo da entrada; em caso de empate vence\n"
          " * o literal (palavras reservadas tem prioridade sobre ID). */\n", f);
    fputs("static Token proximo_token_gerado(void)\n{\n", f);
    fputs("    Token t;\n    size_t n, melhor;\n    int i, tipo;\n\n", f);
    fputs("    do {\n        melhor = 0;\n", f);
    for (i = 0; i < g->nskips; i++)
        fprintf(f, "        if ((n = casa_skip_%d(fonte + pos)) > melhor) melhor = n;\n", i);
    fputs("        if (melhor > 0) consumir(melhor);\n"
          "    } while (melhor > 0);\n\n", f);
    fputs("    memset(&t, 0, sizeof t);\n", f);
    fputs("    t.linha = linha;\n    t.coluna = coluna;\n", f);
    fputs("    if (fonte[pos] == '\\0') {\n        t.tipo = T_FIM;\n        return t;\n    }\n", f);
    fputs("    melhor = 0;\n    tipo = -1;\n", f);
    fputs("    for (i = 0; texto_literais[i] != NULL; i++) {\n", f);
    fputs("        n = strlen(texto_literais[i]);\n", f);
    fputs("        if (n > melhor && strncmp(fonte + pos, texto_literais[i], n) == 0) {\n", f);
    fputs("            melhor = n;\n            tipo = tipo_literais[i];\n        }\n    }\n", f);
    for (i = 1; i < g->nterms; i++)
        if (!g->terms[i].literal)
            fprintf(f, "    if ((n = casa_%s(fonte + pos)) > melhor) {\n"
                       "        melhor = n;\n        tipo = %s;\n    }\n",
                    g->terms[i].nome_c, g->terms[i].nome_c);
    fputs("    if (melhor == 0) {\n", f);
    fputs("        fprintf(stderr, \"linha %d, coluna %d: erro lexico: caractere "
          "invalido '%c'\\n\",\n                linha, coluna, fonte[pos]);\n", f);
    fputs("        exit(EXIT_FAILURE);\n    }\n", f);
    fputs("    if (melhor >= sizeof t.lexema) {\n"
          "        fprintf(stderr, \"linha %d, coluna %d: lexema excede limite de 99 bytes\\n\", linha, coluna);\n"
          "        exit(EXIT_FAILURE);\n    }\n    n = melhor;\n    t.tamanho = n;\n", f);
    fputs("    memcpy(t.lexema, fonte + pos, n);\n    t.lexema[n] = '\\0';\n", f);
    fputs("    t.tipo = tipo;\n    consumir(melhor);\n    return t;\n}\n\n", f);
    return erros;
}

static void gerar_procedimentos(FILE *f, const Gramatica *g, const OpcoesGeracao *op)
{
    int i;
    Emissor em;
    em.f = f;
    em.nivel = 0;

    fputs("/* ---- procedimentos (um por nao terminal) ---- */\n\n", f);
    for (i = 0; i < g->nnts; i++) {
        if (g->nts[i].regra == NULL)
            continue;
        fprintf(f, "/* %s = ", g->nts[i].nome);
        {
            /* a regra em EBNF, sem "* /" */
            FILE *tmp = tmpfile();
            if (tmp != NULL) {
                char *texto;
                size_t n;
                expr_imprimir(tmp, g, g->nts[i].regra);
                n = (size_t) ftell(tmp);
                texto = (char *) xmalloc(n + 1);
                rewind(tmp);
                n = fread(texto, 1, n, tmp);
                texto[n] = '\0';
                emitir_comentario(f, texto);
                free(texto);
                fclose(tmp);
            }
        }
        fputs(" ; */\n", f);
        fprintf(f, "void parse_%s(void)\n{\n", g->nts[i].nome);
        em.nivel = 1;
        if (op->arvore)
            emit(&em, "arvore_entra(\"%s\");", g->nts[i].nome);
        gen_expr(&em, g, g->nts[i].regra);
        if (op->arvore)
            emit(&em, "arvore_sai();");
        fputs("}\n\n", f);
    }
}

static void gerar_finalizacao(FILE *f, int microc)
{
    if (microc)
        fputs("static int scanner_ativo = 0;\n\n", f);
    fputs("static void finalizar(void)\n{\n"
          "    if (arquivo_tokens != NULL) fclose(arquivo_tokens);\n", f);
    if (microc)
        fputs("    if (scanner_ativo) scanner_finalizar();\n", f);
    else
        fputs("    free(fonte);\n", f);
    fputs("    if (entrada_fonte != NULL && entrada_fonte != stdin)\n"
          "        fclose(entrada_fonte);\n}\n\n", f);
}

static void gerar_main(FILE *f, const Gramatica *g, const OpcoesGeracao *op)
{
    fputs("/* ---- programa principal ---- */\n\n", f);
    fputs("int main(int argc, char **argv)\n{\n", f);
    fputs("    int i, fim[2];\n    const char *arquivo = NULL, *tokens = NULL;\n", f);
    fputs("    FILE *entrada = stdin;\n\n", f);
    fputs("    if (atexit(finalizar) != 0) return EXIT_FAILURE;\n\n", f);
    fputs("    for (i = 1; i < argc; i++) {\n", f);
    fputs("        if (strcmp(argv[i], \"--tree\") == 0) {\n", f);
    fputs("            imprimir_arvore = 1;\n", f);
    fputs("        } else if (strcmp(argv[i], \"--tokens\") == 0 && i + 1 < argc) {\n", f);
    fputs("            tokens = argv[++i];\n", f);
    fputs("        } else if (argv[i][0] == '-' && argv[i][1] != '\\0') {\n", f);
    fputs("            fprintf(stderr, \"uso: %s [--tree] [--tokens arquivo.tok | "
          "programa]\\n\", argv[0]);\n", f);
    fputs("            return 2;\n        } else {\n            arquivo = argv[i];\n"
          "        }\n    }\n", f);
    fputs("    if (imprimir_arvore && !SUPORTE_ARVORE)\n", f);
    fputs("        fprintf(stderr, \"aviso: gere o analisador com --tree para "
          "imprimir a arvore\\n\");\n\n", f);
    fputs("    if (tokens != NULL) {\n", f);
    fputs("        arquivo_tokens = fopen(tokens, \"r\");\n", f);
    fputs("        if (arquivo_tokens == NULL) {\n", f);
    fputs("            fprintf(stderr, \"nao foi possivel abrir %s\\n\", tokens);\n", f);
    fputs("            return 2;\n        }\n    } else {\n", f);
    fputs("        if (arquivo != NULL && (entrada = fopen(arquivo, \"r\")) == NULL) {\n", f);
    fputs("            fprintf(stderr, \"nao foi possivel abrir %s\\n\", arquivo);\n", f);
    fputs("            return 2;\n        }\n", f);
    fputs("        entrada_fonte = entrada;\n", f);
    if (op->microc)
        fputs("        scanner_ativo = 1;\n        scanner_iniciar(entrada);\n", f);
    else
        fputs("        fonte = ler_tudo(entrada);\n", f);
    fputs("    }\n\n", f);
    fputs("    avancar();\n", f);
    fprintf(f, "    parse_%s();\n", g->nts[g->inicial].nome);
    fputs("    if (tok.tipo != TOKEN_FIM) {\n", f);
    fputs("        fim[0] = TOKEN_FIM;\n        fim[1] = -1;\n", f);
    fputs("        erro_sintatico(fim);\n    }\n", f);
    fputs("    printf(\"entrada aceita\\n\");\n\n", f);
    fputs("    return 0;\n}\n", f);
}

static const char *nome_base(const char *caminho)
{
    const char *b = strrchr(caminho, '/');
    return b != NULL ? b + 1 : caminho;
}

static int gerar_c(FILE *f, const Gramatica *g, const OpcoesGeracao *op)
{
    FILE *corpo;
    int c, erros = 0;

    /* Os procedimentos sao gerados primeiro num arquivo temporario porque
     * registram os conjuntos (conj_N), que precisam vir antes no .c. */
    corpo = tmpfile();
    if (corpo == NULL) {
        diag_erro(0, 0, "nao foi possivel criar arquivo temporario");
        return -1;
    }
    gerar_procedimentos(corpo, g, op);

    escrever_cabecalho_c(f, op);
    fputs("#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <ctype.h>\n#include <errno.h>\n#include <limits.h>\n", f);
    fprintf(f, "#include \"%s.h\"\n\n", nome_base(op->prefixo));
    fprintf(f, "#define SUPORTE_ARVORE %d\n\n", op->arvore ? 1 : 0);

    fputs("/* ---- conjuntos de tokens ---- */\n\n", f);
    emitir_conjuntos(f, g);
    fputc('\n', f);

    emit_linhas(f, rt_estado);
    if (!op->microc)
        erros += gerar_lexico(f, g);
    gerar_finalizacao(f, op->microc);
    gerar_nome_token(f, g);
    gerar_posicao_e_avancar(f, op->microc);
    emit_linhas(f, rt_primitivas);
    gen_token_de_arquivo(f, g, op->microc);

    rewind(corpo);
    while ((c = fgetc(corpo)) != EOF)
        fputc(c, f);
    fclose(corpo);

    gerar_main(f, g, op);
    return erros == 0 ? 0 : -1;
}

static int gravar_saida(FILE *origem, const char *prefixo, const char *extensao)
{
    char *caminho = (char *) xmalloc(strlen(prefixo) + strlen(extensao) + 1);
    FILE *destino;
    int c, falhou;
    sprintf(caminho, "%s%s", prefixo, extensao);
    destino = fopen(caminho, "w");
    if (destino == NULL) {
        diag_erro(0, 0, "nao foi possivel criar %s", caminho);
        free(caminho);
        return -1;
    }
    rewind(origem);
    while ((c = fgetc(origem)) != EOF)
        fputc(c, destino);
    falhou = ferror(origem) || ferror(destino);
    if (fclose(destino) != 0)
        falhou = 1;
    if (falhou) {
        diag_erro(0, 0, "falha ao gravar %s", caminho);
        remove(caminho);
    }
    free(caminho);
    return falhou ? -1 : 0;
}

int codegen_gerar(Gramatica *g, const OpcoesGeracao *op)
{
    int r = -1;
    FILE *h = tmpfile(), *c = tmpfile();
    if (h == NULL || c == NULL) {
        diag_erro(0, 0, "nao foi possivel criar arquivos temporarios");
    } else {
        nomear_terminais(g, op->microc);
        if (gerar_h(h, g, op) == 0 && gerar_c(c, g, op) == 0
            && !ferror(h) && !ferror(c)) {
            r = gravar_saida(h, op->prefixo, ".h");
            if (r == 0)
                r = gravar_saida(c, op->prefixo, ".c");
        }
    }
    if (h != NULL) fclose(h);
    if (c != NULL) fclose(c);
    liberar_conjuntos();
    return r;
}
