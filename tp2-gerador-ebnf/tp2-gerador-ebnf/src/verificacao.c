/*
 * verificacao.c - Verificacoes semanticas da gramatica (etapa 2).
 *
 * Ja implementados (use como modelo):
 *   - verif_indefinidos: nao terminais usados mas nao definidos;
 *   - verif_terminais:   terminais nomeados sem declaracao.
 *
 * Implementados neste modulo:
 *   - verif_inalcancaveis      (aviso)
 *   - verif_improdutivos       (erro)
 *   - verif_recursao_esquerda  (erro; direta e indireta)
 *
 * A duplicidade de definicoes ja e detectada durante a leitura
 * (ebnf_parser.c).
 */
#include <stdlib.h>
#include <string.h>
#include "verificacao.h"
#include "microc_mapa.h"
#include "util.h"

/* ------------------------------------------------------------------ */
/* Fornecido                                                          */
/* ------------------------------------------------------------------ */

static int verif_indefinidos(Gramatica *g)
{
    int i, erros = 0;
    for (i = 0; i < g->nnts; i++) {
        if (g->nts[i].regra == NULL) {
            diag_erro(g->nts[i].linha, g->nts[i].coluna,
                      "nao terminal %s usado mas nao definido", g->nts[i].nome);
            erros++;
        }
    }
    return erros;
}

static int verif_terminais(Gramatica *g, int modo_microc)
{
    int i, erros = 0, carregado = 0;
    Terminal *t;

    if (modo_microc) {
        carregado = microc_carregar_tokens_h(g->tokens_externos) == 0;
        if (!carregado)
            return 1;
        if (!microc_token_existe("UNDEF") || !microc_token_existe("END_OF_FILE")) {
            diag_erro(0, 0, "TokenType deve declarar UNDEF e END_OF_FILE");
            erros++;
        }
    }

    for (i = 1; i < g->nterms; i++) {
        t = &g->terms[i];
        if (modo_microc) {
            if (t->literal && microc_nome_literal(t->nome) == NULL) {
                diag_erro(t->linha, t->coluna, "literal %s nao corresponde a nenhum "
                          "token de Micro C", t->exibicao);
                erros++;
            } else if (carregado && !microc_token_existe(t->literal
                       ? microc_nome_literal(t->nome) : t->nome)) {
                diag_erro(t->linha, t->coluna, "terminal %s nao existe na enumeracao "
                          "TokenType de %s", t->nome, g->tokens_externos);
                erros++;
            }
        } else if (!t->declarado) {
            diag_erro(t->linha, t->coluna, "terminal %s nao declarado em TOKENS", t->nome);
            erros++;
        }
    }
    return erros;
}

/* ------------------------------------------------------------------ */
/* Alcancabilidade, produtividade e ciclos de chamadas a esquerda       */
/* ------------------------------------------------------------------ */

typedef struct {
    int *marcados;
    int mudou;
} Alcance;

static void marcar_referencia(Expr *e, void *ctx)
{
    Alcance *a = (Alcance *) ctx;
    if (e->tipo == E_NAOTERM && !a->marcados[e->simbolo]) {
        a->marcados[e->simbolo] = 1;
        a->mudou = 1;
    }
}

static int verif_inalcancaveis(Gramatica *g)
{
    int i;
    Alcance a;
    a.marcados = (int *) xcalloc((size_t) g->nnts, sizeof(int));
    a.marcados[g->inicial] = 1;
    do {
        a.mudou = 0;
        for (i = 0; i < g->nnts; i++)
            if (a.marcados[i])
                expr_percorrer(g->nts[i].regra, marcar_referencia, &a);
    } while (a.mudou);
    for (i = 0; i < g->nnts; i++)
        if (!a.marcados[i] && g->nts[i].regra != NULL)
            diag_aviso(g->nts[i].linha_def, 0,
                       "nao terminal %s inalcancavel a partir do simbolo inicial",
                       g->nts[i].nome);
    free(a.marcados);
    return 0;
}

static int expr_produtiva(const Expr *e, const int *produtivos)
{
    int i;
    switch (e->tipo) {
    case E_TERM: case E_VAZIO: case E_OPC: case E_REP:
        return 1;
    case E_NAOTERM:
        return produtivos[e->simbolo];
    case E_SEQ:
        for (i = 0; i < e->nfilhos; i++)
            if (!expr_produtiva(e->filhos[i], produtivos))
                return 0;
        return 1;
    case E_ALT:
        for (i = 0; i < e->nfilhos; i++)
            if (expr_produtiva(e->filhos[i], produtivos))
                return 1;
        return 0;
    }
    return 0;
}

static int verif_improdutivos(Gramatica *g)
{
    int i, mudou, erros = 0;
    int *produtivos = (int *) xcalloc((size_t) g->nnts, sizeof(int));
    do {
        mudou = 0;
        for (i = 0; i < g->nnts; i++) {
            if (!produtivos[i] && expr_produtiva(g->nts[i].regra, produtivos)) {
                produtivos[i] = 1;
                mudou = 1;
            }
        }
    } while (mudou);
    for (i = 0; i < g->nnts; i++) {
        if (!produtivos[i]) {
            diag_erro(g->nts[i].linha_def, 0,
                      "nao terminal %s improdutivo", g->nts[i].nome);
            erros++;
        }
    }
    free(produtivos);
    return erros;
}

/* A -> B quando a regra de A pode chamar B antes de consumir um token.
 * Uma sequencia contribui com todos os seus prefixos anulaveis. */
static void referencias_esquerda(const Expr *e, Bitset *dest)
{
    int i;
    if (e->tipo == E_NAOTERM) {
        bitset_add(dest, e->simbolo);
        return;
    }
    for (i = 0; i < e->nfilhos; i++) {
        referencias_esquerda(e->filhos[i], dest);
        if (e->tipo == E_SEQ && !e->filhos[i]->anulavel)
            break;
    }
}

int verif_recursao_esquerda(Gramatica *g)
{
    int i, j, k, erros = 0;
    int *direta = (int *) xcalloc((size_t) g->nnts, sizeof(int));
    Bitset *alcance = (Bitset *) xcalloc((size_t) g->nnts, sizeof(Bitset));
    for (i = 0; i < g->nnts; i++) {
        bitset_iniciar(&alcance[i], g->nnts);
        referencias_esquerda(g->nts[i].regra, &alcance[i]);
        direta[i] = bitset_tem(&alcance[i], i);
    }
    /* Fecho transitivo: um ciclo que volta ao proprio nao terminal
     * caracteriza recursao a esquerda, mesmo com prefixos anulaveis. */
    for (k = 0; k < g->nnts; k++)
        for (j = 0; j < g->nnts; j++)
            if (bitset_tem(&alcance[j], k))
                bitset_unir(&alcance[j], &alcance[k]);
    for (i = 0; i < g->nnts; i++) {
        if (bitset_tem(&alcance[i], i)) {
            diag_erro(g->nts[i].linha_def, 0,
                      "nao terminal %s tem recursao a esquerda %s",
                      g->nts[i].nome, direta[i] ? "direta" : "indireta");
            erros++;
        }
        bitset_liberar(&alcance[i]);
    }
    free(alcance);
    free(direta);
    return erros;
}

int verif_basica(Gramatica *g, int modo_microc)
{
    int erros = 0;
    if (g->inicial < 0) {
        diag_erro(0, 0, "a gramatica nao tem nenhuma regra");
        return 1;
    }
    erros += verif_indefinidos(g);
    erros += verif_terminais(g, modo_microc);
    erros += verif_inalcancaveis(g);
    if (erros == 0)
        erros += verif_improdutivos(g);
    return erros;
}
