/* Verifica os atributos internos, inclusive dos filhos que nao
 * contribuem para o FIRST de uma sequencia. */
#include <assert.h>
#include <stdio.h>
#include "ebnf_parser.h"
#include "conjuntos.h"
#include "util.h"

static void conjunto(const Gramatica *g, const Bitset *b,
                     const char *const *nomes, int quantidade)
{
    int i, t;
    assert(bitset_tamanho(b) == quantidade);
    for (i = 0; i < quantidade; i++) {
        t = gram_busca_terminal(g, nomes[i], 1);
        if (t < 0)
            t = gram_busca_terminal(g, nomes[i], 0);
        assert(t >= 0);
        assert(bitset_tem(b, t));
    }
}

int main(void)
{
    const char *fonte = "RULES inicio = a b { c } 'z' ; "
                        "a = [ 'a' ] ; b = [ 'b' ] ; c = 'c' ;";
    const char *first_inicio[] = { "a", "b", "c", "z" };
    const char *follow_a[] = { "b", "c", "z" };
    const char *follow_b[] = { "c", "z" };
    const char *follow_c[] = { "c", "z" };
    const char *first_c[] = { "c" };
    const char *first_z[] = { "z" };
    const char *follow_inicio[] = { "$" };
    Gramatica *g = gram_criar("atributos.ebnf");
    Expr *seq, *rep;
    int a, b, c, z;

    diag_definir_arquivo(g->arquivo);
    assert(ebnf_analisar(g, fonte));
    a = gram_busca_naoterminal(g, "a");
    b = gram_busca_naoterminal(g, "b");
    c = gram_busca_naoterminal(g, "c");
    conj_iniciar(g);
    conj_anulavel(g);
    conj_first(g);
    conj_follow(g);
    assert(!g->nts[g->inicial].anulavel);
    assert(g->nts[a].anulavel && g->nts[b].anulavel);
    assert(!g->nts[c].anulavel);
    conjunto(g, &g->nts[g->inicial].first, first_inicio, 4);
    conjunto(g, &g->nts[g->inicial].follow, follow_inicio, 1);
    conjunto(g, &g->nts[a].follow, follow_a, 3);
    conjunto(g, &g->nts[b].follow, follow_b, 2);
    conjunto(g, &g->nts[c].follow, follow_c, 2);
    seq = g->nts[g->inicial].regra;
    assert(seq->tipo == E_SEQ && seq->nfilhos == 4);
    rep = seq->filhos[2];
    assert(rep->tipo == E_REP && rep->anulavel);
    conjunto(g, &rep->first, first_c, 1);
    conjunto(g, &rep->follow, first_z, 1);
    conjunto(g, &rep->filhos[0]->follow, follow_c, 2);
    conjunto(g, &seq->filhos[3]->first, first_z, 1);
    z = gram_busca_terminal(g, "z", 1);
    assert(!bitset_tem(&seq->filhos[3]->first, TERM_FIM));
    assert(bitset_tem(&seq->filhos[3]->first, z));
    conjunto(g, &seq->filhos[3]->follow, follow_inicio, 1);
    /* Reinicializar deve permitir repetir o calculo sem dados antigos. */
    conj_iniciar(g);
    conj_anulavel(g);
    conj_first(g);
    conj_follow(g);
    conjunto(g, &g->nts[g->inicial].first, first_inicio, 4);
    conjunto(g, &g->nts[c].follow, follow_c, 2);
    gram_liberar(g);
    puts("ok     atributos das subexpressoes EBNF");
    return 0;
}
