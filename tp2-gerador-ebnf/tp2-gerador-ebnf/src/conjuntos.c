/*
 * conjuntos.c - Calculo de anulavel, FIRST e FOLLOW (etapa 3).
 *
 * Fornecido: conj_iniciar e conj_imprimir.
 *
 * conj_anulavel, conj_first e conj_follow sao
 * calculados por iteracao ate ponto fixo: repita uma passada sobre todas
 * as regras enquanto algum conjunto (ou flag) mudar. bitset_unir devolve
 * 1 quando o conjunto de destino muda, o que ajuda a detectar o ponto fixo.
 */
#include <stdio.h>
#include <string.h>
#include "conjuntos.h"
#include "util.h"

/* ------------------------------------------------------------------ */
/* Fornecido                                                          */
/* ------------------------------------------------------------------ */

static void iniciar_expr(Expr *e, void *ctx)
{
    int n = *(int *) ctx;
    bitset_liberar(&e->first);
    bitset_liberar(&e->follow);
    bitset_iniciar(&e->first, n);
    bitset_iniciar(&e->follow, n);
    e->anulavel = 0;
}

void conj_iniciar(Gramatica *g)
{
    int i, n = g->nterms;
    for (i = 0; i < g->nnts; i++) {
        bitset_liberar(&g->nts[i].first);
        bitset_liberar(&g->nts[i].follow);
        bitset_iniciar(&g->nts[i].first, n);
        bitset_iniciar(&g->nts[i].follow, n);
        g->nts[i].anulavel = 0;
        expr_percorrer(g->nts[i].regra, iniciar_expr, &n);
    }
}

void conj_imprimir(FILE *f, const Gramatica *g)
{
    int i;
    size_t largura = strlen("NAO TERMINAL");
    for (i = 0; i < g->nnts; i++)
        if (strlen(g->nts[i].nome) > largura)
            largura = strlen(g->nts[i].nome);
    fprintf(f, "%-*s  ANULAVEL  FIRST / FOLLOW\n", (int) largura, "NAO TERMINAL");
    for (i = 0; i < g->nnts; i++) {
        fprintf(f, "%-*s  %-8s  FIRST  = ", (int) largura, g->nts[i].nome,
                g->nts[i].anulavel ? "sim" : "nao");
        gram_imprimir_conjunto(f, g, &g->nts[i].first);
        fprintf(f, "\n%-*s            FOLLOW = ", (int) largura, "");
        gram_imprimir_conjunto(f, g, &g->nts[i].follow);
        fputc('\n', f);
    }
}

/* ------------------------------------------------------------------ */
/* Iteracoes monotonas sobre as regras e suas subexpressoes             */
/* ------------------------------------------------------------------ */

static int anulavel_expr(Expr *e, const Gramatica *g)
{
    int i, valor = 0, mudou = 0;
    for (i = 0; i < e->nfilhos; i++)
        mudou |= anulavel_expr(e->filhos[i], g);
    switch (e->tipo) {
    case E_TERM: break;
    case E_NAOTERM: valor = g->nts[e->simbolo].anulavel; break;
    case E_VAZIO: case E_OPC: case E_REP: valor = 1; break;
    case E_SEQ:
        valor = 1;
        for (i = 0; i < e->nfilhos; i++)
            valor &= e->filhos[i]->anulavel;
        break;
    case E_ALT:
        for (i = 0; i < e->nfilhos; i++)
            valor |= e->filhos[i]->anulavel;
        break;
    }
    if (valor && !e->anulavel) {
        e->anulavel = 1;
        mudou = 1;
    }
    return mudou;
}

void conj_anulavel(Gramatica *g)
{
    int i, mudou;
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            Expr *e = g->nts[i].regra;
            mudou |= anulavel_expr(e, g);
            if (e->anulavel && !g->nts[i].anulavel) {
                g->nts[i].anulavel = 1;
                mudou = 1;
            }
        }
    } while (mudou);
}

static int first_expr(Expr *e, const Gramatica *g)
{
    int i, mudou = 0;
    /* Todos os filhos precisam de FIRST, inclusive os que estao depois
     * de um prefixo nao anulavel em uma sequencia. */
    for (i = 0; i < e->nfilhos; i++)
        mudou |= first_expr(e->filhos[i], g);
    switch (e->tipo) {
    case E_TERM:
        if (!bitset_tem(&e->first, e->simbolo)) {
            bitset_add(&e->first, e->simbolo);
            mudou = 1;
        }
        break;
    case E_NAOTERM:
        mudou |= bitset_unir(&e->first, &g->nts[e->simbolo].first);
        break;
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++) {
            mudou |= bitset_unir(&e->first, &e->filhos[i]->first);
            if (!e->filhos[i]->anulavel)
                break;
        }
        break;
    case E_ALT: case E_OPC: case E_REP:
        for (i = 0; i < e->nfilhos; i++)
            mudou |= bitset_unir(&e->first, &e->filhos[i]->first);
        break;
    case E_VAZIO: break;
    }
    return mudou;
}

void conj_first(Gramatica *g)
{
    int i, mudou;
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            mudou |= first_expr(g->nts[i].regra, g);
            mudou |= bitset_unir(&g->nts[i].first, &g->nts[i].regra->first);
        }
    } while (mudou);
}

static int follow_expr(Expr *e, Gramatica *g)
{
    int i, j, mudou = 0;
    switch (e->tipo) {
    case E_NAOTERM:
        mudou |= bitset_unir(&g->nts[e->simbolo].follow, &e->follow);
        break;
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++) {
            for (j = i + 1; j < e->nfilhos; j++) {
                mudou |= bitset_unir(&e->filhos[i]->follow, &e->filhos[j]->first);
                if (!e->filhos[j]->anulavel)
                    break;
            }
            if (j == e->nfilhos)
                mudou |= bitset_unir(&e->filhos[i]->follow, &e->follow);
        }
        break;
    case E_ALT: case E_OPC: case E_REP:
        for (i = 0; i < e->nfilhos; i++) {
            mudou |= bitset_unir(&e->filhos[i]->follow, &e->follow);
            if (e->tipo == E_REP)
                mudou |= bitset_unir(&e->filhos[i]->follow, &e->filhos[i]->first);
        }
        break;
    case E_TERM: case E_VAZIO: break;
    }
    for (i = 0; i < e->nfilhos; i++)
        mudou |= follow_expr(e->filhos[i], g);
    return mudou;
}

void conj_follow(Gramatica *g)
{
    int i, mudou;
    bitset_add(&g->nts[g->inicial].follow, TERM_FIM);
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            mudou |= bitset_unir(&g->nts[i].regra->follow, &g->nts[i].follow);
            mudou |= follow_expr(g->nts[i].regra, g);
        }
    } while (mudou);
}
