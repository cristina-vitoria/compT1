/* Compilacao propria de expressoes regulares, sem bibliotecas de regex. */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "automato.h"
#include "bitset.h"
#include "util.h"

#define ALFABETO 256
#define MAX_AFD 4096
#define MAX_GRUPOS_REGEX 256

typedef struct {
    int epsilon;
    int a, b;
    unsigned char caracteres[ALFABETO];
} EstadoAFN;

typedef struct {
    const char *regex;
    size_t pos;
    EstadoAFN *estados;
    int n, cap, erro, profundidade;
} Regex;

typedef struct { int inicio, fim; } Fragmento;

typedef struct {
    Bitset subconjunto;
    int destino[ALFABETO];
    int final;
} EstadoAFD;

typedef struct {
    EstadoAFD *estados;
    int n, cap, inicio;
} AFD;

static void erro(Regex *r, const char *mensagem)
{
    if (!r->erro)
        diag_erro(0, 0, "regex /%s/, posicao %lu: %s", r->regex,
                  (unsigned long) r->pos + 1, mensagem);
    r->erro = 1;
}

static int estado(Regex *r, int epsilon)
{
    EstadoAFN *e;
    if (r->n == r->cap) {
        r->cap = r->cap ? 2 * r->cap : 32;
        r->estados = (EstadoAFN *) xrealloc(r->estados,
                            (size_t) r->cap * sizeof(EstadoAFN));
    }
    e = &r->estados[r->n];
    memset(e, 0, sizeof *e);
    e->epsilon = epsilon;
    e->a = e->b = -1;
    return r->n++;
}

static Fragmento vazio(Regex *r)
{
    Fragmento f;
    f.inicio = estado(r, 1);
    f.fim = estado(r, 1);
    r->estados[f.inicio].a = f.fim;
    return f;
}

static int caractere(Regex *r)
{
    int c = (unsigned char) r->regex[r->pos];
    if (c == 0) {
        erro(r, "caractere ou escape incompleto");
        return 0;
    }
    r->pos++;
    if (c != '\\')
        return c;
    c = (unsigned char) r->regex[r->pos];
    if (!c) {
        erro(r, "escape incompleto");
        return 0;
    }
    r->pos++;
    switch (c) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case 'f': return '\f';
    case 'v': return '\v';
    default:
        if (isalnum(c))
            erro(r, "escape nao suportado (use classes explicitas)");
        return c;
    }
}

static Fragmento alternativa(Regex *r);

static Fragmento atomo(Regex *r)
{
    Fragmento f;
    int c, fim, i, inversa, primeiro;
    unsigned char classe[ALFABETO];
    memset(classe, 0, sizeof classe);
    c = (unsigned char) r->regex[r->pos];
    if (c == '(') {
        r->pos++;
        r->profundidade++;
        if (r->profundidade > MAX_GRUPOS_REGEX) {
            erro(r, "limite de aninhamento excedido");
            return vazio(r);
        }
        f = alternativa(r);
        r->profundidade--;
        if (r->regex[r->pos] != ')')
            erro(r, "esperado ')' no grupo");
        else
            r->pos++;
        return f;
    }
    if (c == '[') {
        r->pos++;
        inversa = r->regex[r->pos] == '^';
        if (inversa)
            r->pos++;
        primeiro = 1;
        while (r->regex[r->pos] && (!r->erro)
               && (r->regex[r->pos] != ']' || primeiro)) {
            c = caractere(r);
            primeiro = 0;
            if (r->regex[r->pos] == '-' && r->regex[r->pos + 1]
                && r->regex[r->pos + 1] != ']') {
                r->pos++;
                fim = caractere(r);
                if (fim < c)
                    erro(r, "intervalo invertido na classe");
                for (i = c; i <= fim; i++)
                    classe[i] = 1;
            } else {
                classe[c] = 1;
            }
        }
        if (r->regex[r->pos] != ']')
            erro(r, "classe nao terminada");
        else
            r->pos++;
        if (inversa)
            for (i = 1; i < ALFABETO; i++)
                classe[i] = (unsigned char) !classe[i];
    } else if (c == '.') {
        r->pos++;
        for (i = 1; i < ALFABETO; i++)
            classe[i] = (unsigned char) (i != '\n');
    } else {
        if (c == '*' || c == '+' || c == '?' || c == ']' || c == '}') {
            erro(r, "operador sem operando ou delimitador inesperado");
            r->pos++;
            return vazio(r);
        }
        c = caractere(r);
        classe[c] = 1;
    }
    f.inicio = estado(r, 0);
    f.fim = estado(r, 1);
    r->estados[f.inicio].a = f.fim;
    memcpy(r->estados[f.inicio].caracteres, classe, sizeof classe);
    return f;
}

static Fragmento repeticao(Regex *r)
{
    Fragmento f = atomo(r), novo;
    int op = (unsigned char) r->regex[r->pos];
    if (!r->erro && (op == '*' || op == '+' || op == '?')) {
        r->pos++;
        novo.inicio = estado(r, 1);
        novo.fim = estado(r, 1);
        r->estados[novo.inicio].a = f.inicio;
        if (op != '+')
            r->estados[novo.inicio].b = novo.fim;
        r->estados[f.fim].a = novo.fim;
        if (op != '?')
            r->estados[f.fim].b = f.inicio;
        f = novo;
        op = (unsigned char) r->regex[r->pos];
        if (op == '*' || op == '+' || op == '?')
            erro(r, "quantificadores consecutivos nao suportados");
    }
    return f;
}

static Fragmento sequencia(Regex *r)
{
    Fragmento f = vazio(r), proximo;
    while (r->regex[r->pos] && r->regex[r->pos] != '|'
           && r->regex[r->pos] != ')' && !r->erro) {
        proximo = repeticao(r);
        r->estados[f.fim].a = proximo.inicio;
        f.fim = proximo.fim;
    }
    return f;
}

static Fragmento alternativa(Regex *r)
{
    Fragmento f = sequencia(r), direita, novo;
    while (r->regex[r->pos] == '|' && !r->erro) {
        r->pos++;
        direita = sequencia(r);
        novo.inicio = estado(r, 1);
        novo.fim = estado(r, 1);
        r->estados[novo.inicio].a = f.inicio;
        r->estados[novo.inicio].b = direita.inicio;
        r->estados[f.fim].a = novo.fim;
        r->estados[direita.fim].a = novo.fim;
        f = novo;
    }
    return f;
}

/* Fecho epsilon com pilha: cada estado e visitado no maximo uma vez. */
static void fecho(const Regex *r, Bitset *b, int *pilha)
{
    int i, n = 0, destino, k;
    for (i = bitset_proximo(b, 0); i >= 0; i = bitset_proximo(b, i + 1))
        pilha[n++] = i;
    while (n) {
        i = pilha[--n];
        if (!r->estados[i].epsilon)
            continue;
        for (k = 0; k < 2; k++) {
            destino = k == 0 ? r->estados[i].a : r->estados[i].b;
            if (destino >= 0 && !bitset_tem(b, destino)) {
                bitset_add(b, destino);
                pilha[n++] = destino;
            }
        }
    }
}

static int registrar(AFD *d, const Bitset *b, int fim)
{
    int i;
    EstadoAFD *e;
    for (i = 0; i < d->n; i++)
        if (memcmp(d->estados[i].subconjunto.w, b->w,
                   (size_t) b->npalavras * sizeof(unsigned long)) == 0)
            return i;
    if (d->n == MAX_AFD)
        return -1;
    if (d->n == d->cap) {
        d->cap = d->cap ? 2 * d->cap : 16;
        d->estados = (EstadoAFD *) xrealloc(d->estados,
                                    (size_t) d->cap * sizeof(EstadoAFD));
    }
    e = &d->estados[d->n];
    memset(e, 0, sizeof *e);
    bitset_iniciar(&e->subconjunto, b->n);
    bitset_copiar(&e->subconjunto, b);
    e->final = bitset_tem(b, fim);
    return d->n++;
}

static void liberar_afd(AFD *d)
{
    int i;
    for (i = 0; i < d->n; i++)
        bitset_liberar(&d->estados[i].subconjunto);
    free(d->estados);
}

static int determinizar(Regex *r, Fragmento f, AFD *d)
{
    int i, c, n, destino, ok = 1;
    int *pilha = (int *) xmalloc((size_t) r->n * sizeof(int));
    Bitset b;
    bitset_iniciar(&b, r->n);
    bitset_add(&b, f.inicio);
    fecho(r, &b, pilha);
    if (bitset_tem(&b, f.fim)) {
        erro(r, "padrao anulavel: tokens e SKIP devem consumir caracteres");
        ok = 0;
    } else {
        registrar(d, &b, f.fim);
    }
    for (i = 0; i < d->n && ok; i++) {
        for (c = 1; c < ALFABETO; c++) {
            bitset_limpar(&b);
            for (n = bitset_proximo(&d->estados[i].subconjunto, 0); n >= 0;
                 n = bitset_proximo(&d->estados[i].subconjunto, n + 1)) {
                if (!r->estados[n].epsilon && r->estados[n].caracteres[c])
                    bitset_add(&b, r->estados[n].a);
            }
            fecho(r, &b, pilha);
            destino = registrar(d, &b, f.fim);
            if (destino < 0) {
                erro(r, "limite de 4096 estados do AFD excedido");
                ok = 0;
                break;
            }
            d->estados[i].destino[c] = destino;
        }
    }
    free(pilha);
    bitset_liberar(&b);
    return ok;
}

static int mesma_classe(const AFD *d, int a, int b, const int *classes)
{
    int c;
    if (classes[a] != classes[b])
        return 0;
    for (c = 1; c < ALFABETO; c++)
        if (classes[d->estados[a].destino[c]] != classes[d->estados[b].destino[c]])
            return 0;
    return 1;
}

/* Refinamento de particoes de Moore; inclui o estado morto implicito
 * dos subconjuntos vazios. Todos os estados do AFD sao alcancaveis. */
static AFD minimizar(const AFD *d)
{
    AFD minimo;
    int i, j, nclasses = 2, novas, c;
    int *classes = (int *) xmalloc((size_t) d->n * sizeof(int));
    int *proximas = (int *) xmalloc((size_t) d->n * sizeof(int));
    int *representantes = (int *) xmalloc((size_t) d->n * sizeof(int));
    for (i = 0; i < d->n; i++)
        classes[i] = d->estados[i].final;
    do {
        novas = 0;
        for (i = 0; i < d->n; i++) {
            for (j = 0; j < novas; j++)
                if (mesma_classe(d, i, representantes[j], classes))
                    break;
            if (j == novas)
                representantes[novas++] = i;
            proximas[i] = j;
        }
        memcpy(classes, proximas, (size_t) d->n * sizeof(int));
        i = nclasses;
        nclasses = novas;
    } while (i != nclasses);
    memset(&minimo, 0, sizeof minimo);
    minimo.n = nclasses;
    minimo.inicio = classes[0];
    minimo.estados = (EstadoAFD *) xcalloc((size_t) nclasses, sizeof(EstadoAFD));
    for (i = 0; i < nclasses; i++) {
        j = representantes[i];
        minimo.estados[i].final = d->estados[j].final;
        for (c = 1; c < ALFABETO; c++)
            minimo.estados[i].destino[c] = classes[d->estados[j].destino[c]];
    }
    free(classes);
    free(proximas);
    free(representantes);
    return minimo;
}

static void emitir_afd(FILE *f, const char *nome, const AFD *d)
{
    int i, c, ate, destino, morto = -1, primeiro;
    for (i = 0; i < d->n; i++) {
        if (d->estados[i].final)
            continue;
        for (c = 1; c < ALFABETO; c++)
            if (d->estados[i].destino[c] != i)
                break;
        if (c == ALFABETO)
            morto = i;
    }
    fprintf(f, "/* AFD minimo: %d estados (incluindo o estado morto). */\n", d->n);
    fprintf(f, "static size_t %s(const char *s)\n{\n", nome);
    fprintf(f, "    static const unsigned char finais[] = { ");
    for (i = 0; i < d->n; i++)
        fprintf(f, "%s%d", i ? ", " : "", d->estados[i].final);
    fprintf(f, " };\n    int estado = %d;\n", d->inicio);
    fputs("    size_t i = 0, melhor = 0;\n    unsigned int c;\n\n"
          "    for (;;) {\n        c = (unsigned char) s[i];\n"
          "        if (c == 0) return melhor;\n        switch (estado) {\n", f);
    for (i = 0; i < d->n; i++) {
        if (i == morto)
            continue;
        fprintf(f, "        case %d:\n", i);
        primeiro = 1;
        for (c = 1; c < ALFABETO; c = ate + 1) {
            destino = d->estados[i].destino[c];
            for (ate = c; ate + 1 < ALFABETO
                 && d->estados[i].destino[ate + 1] == destino; ate++)
                ;
            if (destino == morto)
                continue;
            fprintf(f, "            %sif (", primeiro ? "" : "else ");
            if (c == ate)
                fprintf(f, "c == %d", c);
            else
                fprintf(f, "c >= %d && c <= %d", c, ate);
            fprintf(f, ") estado = %d;\n", destino);
            primeiro = 0;
        }
        fprintf(f, "            %sreturn melhor;\n", primeiro ? "" : "else ");
        fputs("            break;\n", f);
    }
    fputs("        default: return melhor;\n        }\n        i++;\n"
          "        if (finais[estado]) melhor = i;\n    }\n}\n\n", f);
}

int regex_emitir(FILE *f, const char *nome, const char *regex)
{
    Regex r;
    AFD d, minimo;
    Fragmento frag;
    memset(&r, 0, sizeof r);
    memset(&d, 0, sizeof d);
    r.regex = regex;
    frag = alternativa(&r);
    if (!r.erro && r.regex[r.pos])
        erro(&r, "delimitador inesperado");
    if (!r.erro && determinizar(&r, frag, &d)) {
        minimo = minimizar(&d);
        emitir_afd(f, nome, &minimo);
        liberar_afd(&minimo);
    }
    liberar_afd(&d);
    free(r.estados);
    return r.erro ? -1 : 0;
}
