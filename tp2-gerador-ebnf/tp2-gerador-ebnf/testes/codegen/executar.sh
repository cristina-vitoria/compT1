#!/bin/sh
# Regressao de ponta a ponta alem dos exemplos do enunciado.
set -eu
SAIDA=build/testes/codegen
GERADOR=${GERADOR:-./gerador}
EXECUTOR=${EXECUTOR:-}
CC=${CC:-gcc}
CFLAGS=${CFLAGS:-"-ansi -pedantic-errors -Wall -Wextra -Werror"}
mkdir -p "$SAIDA"
total=0

# Cada gramatica e compilada com avisos tratados como erros.
gerar() {
    nome=$1
    printf '%s\n' "$2" > "$SAIDA/$nome.ebnf"
    $GERADOR "$SAIDA/$nome.ebnf" -o "$SAIDA/$nome" --tree > "$SAIDA/$nome.gerador" 2>&1
    $CC $CFLAGS -o "$SAIDA/$nome.bin" "$SAIDA/$nome.c"
    bin=$SAIDA/$nome.bin
}

executar() {
    nome=$1
    esperado=$2
    mensagem=$3
    shift 3
    status=0
    $EXECUTOR "$bin" "$@" > "$SAIDA/$nome.saida" 2>&1 || status=$?
    if [ "$status" -ne "$esperado" ] || ! grep -F -- "$mensagem" "$SAIDA/$nome.saida" >/dev/null; then
        printf 'FALHA  codegen/%s (status %s, esperado %s)\n' "$nome" "$status" "$esperado"
        cat "$SAIDA/$nome.saida"
        exit 1
    fi
    total=$((total + 1))
    printf 'ok     codegen/%s\n' "$nome"
}

programa() {
    printf '%s' "$4" > "$SAIDA/$1.txt"
    executar "$1" "$2" "$3" "$SAIDA/$1.txt"
}

gerar prioridade 'TOKENS ID = /[a-z]+/; NUM = /[0-9]+/; SKIP = /[ \t\r\n]+/; RULES inicio = "if" ID ("<=" | "<") NUM ";";'
programa maior_prefixo 0 'entrada aceita' 'if ifx<=123;'
programa literal_empate 1 'esperado ID' 'if if<123;'
programa literal_menor 0 'entrada aceita' 'if x<2;'
programa consumo_total 1 'esperado fim de arquivo' 'if x<2; if'
programa lexico_invalido 1 'erro lexico' 'if x@2;'
programa linha_coluna 1 'linha 2, coluna 3:' 'if x<2;
  @'
printf 'if x<2;\000if' > "$SAIDA/nulo.txt"
executar byte_nulo 1 'byte nulo' "$SAIDA/nulo.txt"

gerar skips 'TOKENS SKIP = /a/; SKIP = /ab/; RULES inicio = "x";'
programa maior_skip 0 'entrada aceita' 'abx'

gerar empate 'TOKENS PRIMEIRO = /[a-z]+/; SEGUNDO = /[a-z]+/; RULES inicio = PRIMEIRO;'
programa ordem_declaracao 0 'entrada aceita' 'abc'

gerar regex_alternativa 'TOKENS T = /a|ab/; RULES inicio = T "x";'
programa alternativa_regex 0 'entrada aceita' 'abx'

gerar anulavel 'RULES inicio = opc "z"; opc = ["a"] | "b";'
programa alternativa_vazia 0 'entrada aceita' 'z'
programa alternativa_opcional 0 'entrada aceita' 'az'
programa alternativa_nao_anulavel 0 'entrada aceita' 'bz'
programa alternativa_erro 1 "esperado 'z'" 'abz'

gerar vazio 'RULES inicio = ;'
programa cadeia_vazia 0 'entrada aceita' ''
programa cadeia_vazia_erro 1 'erro lexico' 'a'

gerar comentario 'RULES inicio = "*/";'
programa comentario_c_seguro 0 'entrada aceita' '*/'

gerar colisao 'TOKENS FIM_2 = /b/; FIM = /a/; RULES inicio = FIM_2 FIM;'
programa nomes_sem_colisao 0 'entrada aceita' 'ba'

gerar arvore 'RULES inicio = "a";'
printf 'a' > "$SAIDA/arvore.txt"
executar arvore 0 'entrada aceita' --tree "$SAIDA/arvore.txt"
printf "inicio\n  'a'\nentrada aceita\n" > "$SAIDA/arvore.esperado"
diff -u "$SAIDA/arvore.esperado" "$SAIDA/arvore.saida"

gerar tokens 'TOKENS NUM = /[0-9]+/; ID = /[a-z]+/; RULES inicio = NUM "+" ID;'
cat > "$SAIDA/nomes.tok" <<'TOKENS'

Token: tipo = NUM, lexema = '2', linha = 3
Token: tipo = T_MAIS, lexema = '+', linha = 3
Token: tipo = T_ID, lexema = 'x', linha = 3
TOKENS
executar tokens_nomes 0 'entrada aceita' --tokens "$SAIDA/nomes.tok"
cat > "$SAIDA/numeros.tok" <<'TOKENS'
Token: tipo = 1, lexema = '2', linha = 1
Token: tipo = 3, lexema = '+', linha = 1
Token: tipo = 2, lexema = 'x', linha = 1
Token: tipo = 0, lexema = '', linha = 1
TOKENS
executar tokens_numericos 0 'entrada aceita' --tokens "$SAIDA/numeros.tok"
printf '\nincorreto\n' > "$SAIDA/incorreto.tok"
executar tokens_malformados 1 'arquivo de tokens, linha 2:' --tokens "$SAIDA/incorreto.tok"
printf "Token: tipo = 9999999999999999999999999999, lexema = '', linha = 1\n" > "$SAIDA/overflow.tok"
executar tokens_overflow 1 'formato invalido' --tokens "$SAIDA/overflow.tok"
printf "Token: tipo = NUM, lexema = '2', linha = 8, coluna = 4\n" > "$SAIDA/coluna.tok"
executar tokens_coluna 1 'linha 8, coluna 1:' --tokens "$SAIDA/coluna.tok"
# A coluna do EOF sintetico e 1; a linha conserva a ultima posicao recebida.
cat "$SAIDA/numeros.tok" > "$SAIDA/apos_fim.tok"
printf "Token: tipo = 1, lexema = '7', linha = 2\n" >> "$SAIDA/apos_fim.tok"
executar tokens_apos_fim 1 'arquivo de tokens, linha 5:' --tokens "$SAIDA/apos_fim.tok"

gerar lexema_longo 'TOKENS ID = /[a-z]+/; RULES inicio = ID;'
printf '%0100d' 0 | tr '0' 'a' > "$SAIDA/longo.txt"
executar lexema_longo 1 'excede limite de 99 bytes' "$SAIDA/longo.txt"

# Regex invalida nao cria arquivos nem sobrescreve uma saida anterior.
for regex in 'a*' '[z-a]' '(abc'; do
    printf 'TOKENS T = /%s/; RULES inicio = T;\n' "$regex" > "$SAIDA/invalida.ebnf"
    rm -f "$SAIDA/invalida.c" "$SAIDA/invalida.h"
    status=0
    $GERADOR "$SAIDA/invalida.ebnf" -o "$SAIDA/invalida" > "$SAIDA/invalida.saida" 2>&1 || status=$?
    [ "$status" -eq 1 ] && [ ! -f "$SAIDA/invalida.c" ] && [ ! -f "$SAIDA/invalida.h" ]
    total=$((total + 1))
done
printf 'preservar\n' > "$SAIDA/invalida.c"
printf 'preservar\n' > "$SAIDA/invalida.h"
status=0
$GERADOR "$SAIDA/invalida.ebnf" -o "$SAIDA/invalida" > "$SAIDA/invalida.saida" 2>&1 || status=$?
[ "$status" -eq 1 ]
[ "$(cat "$SAIDA/invalida.c")" = preservar ] && [ "$(cat "$SAIDA/invalida.h")" = preservar ]
total=$((total + 1))
printf 'ok     regex invalidas sem saidas parciais\n'
printf '\n%s testes adicionais de geracao passaram\n' "$total"
