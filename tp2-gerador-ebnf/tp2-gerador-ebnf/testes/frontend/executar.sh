#!/bin/sh
# Regressao das etapas 1 a 4. Nao depende do gerador de codigo.
set -eu
GERADOR=${GERADOR:-./gerador}
SAIDA=build/testes/frontend
mkdir -p "$SAIDA"
total=0
falhas=0

caso() {
    nome=$1
    esperado=$2
    mensagem=$3
    fonte=$4
    entrada=$SAIDA/$nome.ebnf
    prefixo=$SAIDA/$nome-parser
    printf '%s\n' "$fonte" > "$entrada"
    rm -f "$prefixo.c" "$prefixo.h"
    status=0
    modo=""
    case "$nome" in externo*) modo="--scanner microc" ;; esac
    if [ "$esperado" -eq 0 ]; then
        $GERADOR "$entrada" --sets --debug $modo > "$SAIDA/$nome.saida" 2>&1 || status=$?
    else
        $GERADOR "$entrada" -o "$prefixo" $modo > "$SAIDA/$nome.saida" 2>&1 || status=$?
    fi
    total=$((total + 1))
    if [ "$status" -eq "$esperado" ] \
       && grep -F -- "$mensagem" "$SAIDA/$nome.saida" > /dev/null \
       && [ ! -f "$prefixo.c" ] && [ ! -f "$prefixo.h" ]; then
        printf 'ok     frontend/%s\n' "$nome"
    else
        printf 'FALHA  frontend/%s (status %s, esperado %s)\n' "$nome" "$status" "$esperado"
        cat "$SAIDA/$nome.saida"
        falhas=$((falhas + 1))
    fi
}

caso vazio 0 'sim' 'RULES inicio = ;'
caso epsilon_alternativa 0 'FOLLOW = {$}' 'RULES inicio = "a" | ;'
caso construcao_completa 0 'FIRST  = {' 'TOKENS NUM = /[0-9]+/; RULES inicio = NUM { ("+" | "-") NUM } [";"];'
caso ponto_fixo 0 "FIRST  = {'b', 'a'}" 'RULES inicio = a b; b = "b"; a = | "a";'

caso indefinido 1 'nao terminal ausente usado mas nao definido' 'RULES inicio = ausente;'
caso nao_declarado 1 'terminal NUM nao declarado em TOKENS' 'RULES inicio = NUM;'
caso regra_duplicada 1 'definido mais de uma vez' 'RULES inicio = "a"; inicio = "b";'
caso token_duplicado 1 'declarado mais de uma vez' 'TOKENS NUM = /a/; NUM = /b/; RULES inicio = NUM;'
caso inalcancavel 0 'perdido inalcancavel' 'RULES inicio = "a"; perdido = "b";'
caso improdutivo 1 'improdutivo' 'RULES inicio = ciclo; ciclo = inicio;'
caso recursao_direta 1 'recursao a esquerda direta' 'RULES inicio = inicio "a" | "b";'
caso recursao_indireta 1 'recursao a esquerda indireta' 'RULES inicio = outro; outro = inicio "a" | "b";'
caso prefixo_anulavel 1 'recursao a esquerda indireta' 'RULES inicio = a; a = [","] b | "x"; b = a "y";'
caso prefixo_anulavel_nt 1 'recursao a esquerda direta' 'RULES inicio = prefixo inicio "a" | "b"; prefixo = ;'
caso recursao_direita 0 'FIRST' 'RULES inicio = "a" [inicio];'
caso conflito_first 1 'alternativas 1 e 2 comecam com' 'RULES inicio = "a" "b" | "a" "c";'
caso duas_anulaveis 1 'alternativas 1 e 2 sao anulaveis' 'RULES inicio = a | b; a = ; b = ;'
caso conflito_follow 1 'FIRST e FOLLOW de alternativa anulavel' 'RULES inicio = a "x"; a = "x" | ;'
caso conflito_follow_ordem 1 'FIRST e FOLLOW de alternativa anulavel' 'RULES inicio = a "x"; a = | "x";'
caso opcional_anulavel 1 'corpo de [ ] e anulavel' 'RULES inicio = [a]; a = ;'
caso repeticao_anulavel 1 'corpo de { } e anulavel' 'RULES inicio = {a}; a = ;'
caso opcional_guloso 0 'resolvido de forma gulosa' 'RULES inicio = ["a"] "a";'
caso repeticao_gulosa 0 'resolvido de forma gulosa' 'RULES inicio = {"a"} "a";'
caso grupo_incompleto 1 "esperado ')'" 'RULES inicio = ("a";'
caso delimitador_trocado 1 "esperado ']'" 'RULES inicio = ["a"};'
caso ponto_virgula_ausente 1 "esperado ';'" 'RULES inicio = "a"'
caso caractere_invalido 1 'caractere invalido' 'RULES inicio = @;'
caso literal_incompleto 1 'literal nao terminado' 'RULES inicio = "a'
caso regex_incompleta 1 'expressao regular nao terminada' 'TOKENS NUM = /[0-9]'
caso comentario_incompleto 1 'comentario nao terminado' 'RULES inicio = "a"; (*'
caso sem_regras 1 'ao menos uma regra' 'RULES'
caso literal_vazio 1 'literal vazio' 'RULES inicio = "";'
caso escape_invalido 1 'escape invalido' 'RULES inicio = "\x";'
caso comentario_escapes 0 'FIRST' '(* nota *) RULES inicio = "\n" ("\t" | "\\");'

# O leitor de TokenType deve ignorar comentarios, outras enumeracoes e
# identificadores usados nos valores explicitos de um enumerador.
cat > "$SAIDA/tokens.h" <<'CABECALHO'
typedef enum { NAO_E_TOKEN } Outra;
/* TokenType { FALSO } */
typedef enum ComTag {
    UNDEF = 0, ID = (2 + 1), END_OF_FILE = 9,
    INTEGERCONST = sizeof("VIRGULA, FALSO"), INT,
} TokenType;
CABECALHO
caso externo 0 'FIRST  = {ID}' 'TOKENS EXTERN "tokens.h"; RULES inicio = ID;'
caso externo_inexistente 1 'nao existe na enumeracao' 'TOKENS EXTERN "tokens.h"; RULES inicio = FALSO;'
caso externo_literal_inexistente 1 'nao existe na enumeracao' 'TOKENS EXTERN "tokens.h"; RULES inicio = "+";'
caso externo_sem_arquivo 1 'nao foi possivel abrir cabecalho' 'TOKENS EXTERN "ausente.h"; RULES inicio = ID;'
cat > "$SAIDA/incorreto.h" <<'CABECALHO'
typedef enum { ID, END_OF_FILE } Outro;
CABECALHO
caso externo_sem_enum 1 'TokenType nao encontrada' 'TOKENS EXTERN "incorreto.h"; RULES inicio = ID;'
cat > "$SAIDA/incompleto.h" <<'CABECALHO'
typedef enum { ID } TokenType;
CABECALHO
caso externo_sem_fim 1 'TokenType deve declarar UNDEF e END_OF_FILE' 'TOKENS EXTERN "incompleto.h"; RULES inicio = ID;'

printf '\n%s de %s testes do frontend passaram\n' "$((total - falhas))" "$total"
[ "$falhas" -eq 0 ]
