#ifndef AUTOMATO_H
#define AUTOMATO_H

#include <stdio.h>

/* Regex -> AFN de Thompson -> AFD por subconjuntos -> AFD minimo.
 * Emite um reconhecedor ANSI C do maior prefixo; rejeita regex anulavel.
 * Retorna zero em caso de sucesso. */
int regex_emitir(FILE *f, const char *nome, const char *regex);

#endif
