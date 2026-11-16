

# Compiladores I — Trabalho Prático 2: Gerador de Analisadores Sintáticos Descendentes Recursivos

Código-base do gerador. Ele lê uma gramática em EBNF e gera, em ANSI C, um analisador sintático descendente recursivo para a linguagem descrita. Veja o enunciado para os requisitos completos.

## Compilar e executar

```
make                      # compila o gerador (e o analisador de Micro C, se o TP1 estiver em microc/)
make test                 # executa os testes de testes/
make microc               # gera e compila build/microc/parser_microc
make clean
```

```
./gerador gramatica.ebnf -o saida/parser [--scanner microc] [--sets] [--tree] [--debug]
gcc -ansi -pedantic -Wall -Wextra -o parser saida/parser.c
./parser [--tree] programa.txt        # ou: ./parser --tokens tokens.txt
```

Todo o código deve compilar sem avisos com `gcc -ansi -pedantic -Wall -Wextra`. A única exceção é o `lex.yy.c` gerado pelo flex.

## Estado da implementação

As etapas de leitura e análise da gramática estão implementadas. O executável
compila em ANSI C sem avisos; a geração dos analisadores ainda contém os stubs
do código-base e não está concluída.

| Etapa | Estado |
| --- | --- |
| Leitura EBNF | Implementada: sequência, alternativa, grupo, opcional, repetição e sequência vazia; árvores com posição de origem. |
| Verificação semântica | Implementada: indefinidos, duplicados, terminais não declarados, inalcançáveis, improdutivos e recursão à esquerda direta/indireta por prefixos anuláveis. |
| Conjuntos | Implementada: anulável, FIRST e FOLLOW por ponto fixo, para regras e subexpressões. |
| LL(1) | Implementada: conflitos fatais e avisos com decisão gulosa para interseções FIRST/FOLLOW em opcionais e repetições. |
| Tokens externos | Implementada: leitura dos nomes de `TokenType`, incluindo enum com tag, comentários e valores explícitos. Falha ao ler o cabeçalho é erro. |
| Geração de código | Pendente: tradução das construções, regex em autômatos e leitura de arquivos de tokens. |
| Scanner Micro C | `microc/tokens.h` extraído do TP1; cópia/adaptação do scanner ainda pendente. |
| Extras | Pendentes: recuperação de erros no analisador gerado, AST, correção automática de conflitos e metagramática EBNF. |

Para conferir a etapa atual:

```sh
make test-frontend
./gerador testes/gramaticas/expr.ebnf --debug --sets
./gerador testes/gramaticas/microc.ebnf --scanner microc --sets
```

`make test-frontend` executa 40 casos de leitura/verificação e um teste em C dos
atributos das subexpressões. Ele também verifica que gramáticas inválidas não
produzem arquivos de analisador. `make test` executa essa etapa e os testes
originais: os quatro testes de gramáticas passam; os sete programas de expressões
ainda falham, e os programas Micro C aguardam a integração do scanner.

Validação desta etapa: compilação com `-ansi -pedantic-errors -Wall -Wextra
-Werror`, 40 casos de regressão e o teste dos atributos internos aprovados.
Os mesmos casos e a análise da gramática Micro C foram executados com Valgrind,
sem erros de memória ou vazamentos detectados.

A recuperação de erros já fornecida no leitor de gramáticas não corresponde ao
extra de recuperação no analisador gerado. Da mesma forma, a árvore `Expr` da
gramática e a árvore de derivação de `--tree` não correspondem ao extra de AST.
O bônus total previsto no enunciado é limitado a dois pontos.

Módulos de apoio fornecidos: `util.c`, `bitset.c`, `gramatica.c` e `main.c`.

## Integração com o scanner do TP1 (seção 3.1 do enunciado)

1. Copie `microc.flex`, `tokens.h` e os demais `.c`/`.h` do seu TP1, como a tabela de strings, para `microc/`.
2. No `microc.flex`, use `%option prefix="microc_yy" noyywrap` e devolva `END_OF_FILE` na regra `<<EOF>>`.
3. Ajuste `microc/scanner.c` ao seu TP1: lexema vindo da tabela de strings e mensagem de erro de `error_msg` no token UNDEF.
4. Rode `make microc` e `make test`.

A gramática `testes/gramaticas/microc.ebnf` é a do Apêndice B do livro-texto, fatorada para LL(1). O `else` pendente gera um aviso de conflito em `[ "else" comando ]`. O conflito é resolvido de forma gulosa: o `else` fica com o `if` mais próximo.

## Testes

`testes/executar.sh` compara a saída do gerador (`testes/esperado/*.gerador`) e a dos analisadores gerados (`testes/programas/<gramatica>/*.esperado`) com as saídas esperadas. Cada caso termina com a linha `status <código de saída>`. Acrescente seus próprios casos: o enunciado pede pelo menos 5 gramáticas e 3 programas aceitos e 3 rejeitados por gramática válida.

Os programas de Micro C só rodam depois de `make microc`. O caso `rejeitado_lexico.mc` espera a mensagem "String nao terminada" do seu TP1.
