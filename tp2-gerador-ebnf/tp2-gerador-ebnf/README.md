aluno: 202519040618
uso_ia: sim

# Compiladores I — Trabalho Prático 2: Gerador de Analisadores Sintáticos Descendentes Recursivos

Implementação a partir do código-base do gerador. Ele lê uma gramática em EBNF e gera, em ANSI C, um analisador sintático descendente recursivo para a linguagem descrita. Veja o enunciado para os requisitos completos.


## Compilar e executar

Execute os comandos nesta pasta, onde está o `Makefile`:

```sh
make clean
make                      # compila o gerador e o analisador de Micro C
make test                 # executa os testes de testes/
```

Para gerar, compilar e executar um analisador de expressões, copie este exemplo
com os arquivos que já existem no projeto:

```sh
mkdir -p build/exemplo
./gerador testes/gramaticas/expr.ebnf -o build/exemplo/parser --tree
gcc -ansi -pedantic -Wall -Wextra -o build/exemplo/analisador build/exemplo/parser.c
./build/exemplo/analisador testes/programas/expr/aceito_1.txt
./build/exemplo/analisador --tree testes/programas/expr/aceito_1.txt
```

O analisador imprime `entrada aceita`; a última linha também imprime a árvore
de derivação. O diretório de saída deve existir antes de executar o gerador.

Para apenas consultar a gramática lida e os conjuntos, sem gerar arquivos:

```sh
./gerador testes/gramaticas/expr.ebnf --sets --debug
```

Para executar Micro C, use o alvo que também vincula o scanner do TP1:

```sh
make microc
./build/microc/parser_microc testes/programas/microc/soma.mc
./build/microc/parser_microc --tree testes/programas/microc/else_pendente.mc
```

As opções são facultativas. `--tree` no
gerador habilita a impressão da árvore; `--tree` no analisador solicita essa
impressão. `--scanner microc` é usado apenas com gramáticas `TOKENS EXTERN`,
como `testes/gramaticas/microc.ebnf`; o comando `make microc` já o inclui.
Os nomes `gramatica.ebnf` e `programa.txt` eram exemplos genéricos: use os
caminhos dos seus arquivos ou os exemplos acima.

Requisitos de construção: GCC, Flex e GNU Make 4.3 ou posterior.

Todo o código deve compilar sem avisos com `gcc -ansi -pedantic -Wall -Wextra`. A única exceção é o `lex.yy.c` gerado pelo flex.

## Estado da implementação

O gerador lê e valida EBNF e produz analisadores funcionais em ANSI C.
Há dois modos: scanner gerado a partir de regex e scanner de Micro C do TP1.

| Etapa | Estado |
| --- | --- |
| Leitura EBNF | Sequência, alternativa, grupo, opcional, repetição e sequência vazia, com posição de origem. |
| Verificação semântica | Indefinidos, duplicados, terminais não declarados, inalcançáveis, improdutivos e recursão à esquerda direta/indireta por prefixos anuláveis. |
| Conjuntos | Anulável, FIRST e FOLLOW por ponto fixo, para regras e subexpressões. |
| LL(1) | Conflitos fatais e avisos com decisão gulosa em opcionais e repetições. |
| Tokens externos | Leitura de `TokenType`, incluindo enum com tag, comentários e valores explícitos. |
| Geração de código | Procedimentos recursivos, conjuntos constantes, diagnósticos, comentários com as regras e árvore de derivação (`--tree`). |
| Scanner gerado | Regex → AFN de Thompson → AFD por subconjuntos → AFD mínimo por refinamento de partições → C com `switch`. |
| Scanner Micro C | Integrado, sob demanda, com mensagens léxicas, posição inicial, escapes e liberação de recursos. |
| Arquivos de tokens | `--tokens` aceita tipos numéricos e nomes de tokens, com diagnóstico de linhas malformadas. |

O código gerado libera arquivos e buffers com `atexit`, inclusive quando encontra
erros. Gramáticas ou regex inválidas não criam saídas parciais nem sobrescrevem
analisadores anteriores.

## Expressões regulares

Suporte: concatenação, `|`, `*`, `+`, `?`, agrupamento, classes e intervalos,
classes negadas, ponto (exceto quebra de linha), escapes `\n`, `\t`, `\r`,
`\f`, `\v` e pontuação escapada. O alfabeto corresponde aos bytes de 1 a 255.
Padrões que aceitam a cadeia vazia são rejeitados para tokens e `SKIP`.
O limite de determinização é 4096 estados; o limite de grupos aninhados é 256.

O scanner escolhe o maior prefixo. Em empate, literais vencem tokens nomeados;
entre tokens nomeados, vence a ordem de declaração. As diretivas `SKIP` são
aplicadas antes de reconhecer o próximo token, escolhendo o maior prefixo
entre os padrões de descarte.

A interface `Token` mantém o limite de 99 bytes de conteúdo do código-base;
lexemas maiores são erros explícitos. No scanner Micro C, `Token.tamanho`
preserva o comprimento de caracteres e strings com `\0` interno. Byte nulo
literal no arquivo de entrada não faz parte da notação textual suportada.

## Integração com o TP1

`microc/microc.flex` é uma cópia adaptada do scanner original em `../../microc.flex`.
Ela usa o prefixo `microc_yy`, não contém `main`, não imprime mensagens e retorna
os valores de `TokenType`. O adaptador `microc/scanner.c` fornece a interface
`scanner_iniciar`, `proximo_token` e `scanner_finalizar`.

A gramática Micro C segue a versão fatorada do código-base. O `else` pendente
produz aviso e se associa ao `if` mais próximo. Essa gramática não descreve todas
as construções de C: por exemplo, exige declarações no início dos blocos e não
inclui definições ou chamadas de funções arbitrárias.

```sh
make microc
build/microc/parser_microc testes/programas/microc/soma.mc
build/microc/parser_microc --tree testes/programas/microc/else_pendente.mc
```

## Arquivos de tokens

Os formatos abaixo são aceitos, com vírgulas opcionais entre os campos:

```text
Token: tipo = 1, lexema = '2', linha = 1
Token: tipo = NUM  lexema = '2'  linha = 1
Token: tipo = T_NUM, lexema = '2', linha = 1, coluna = 4
```

Os números correspondem à enumeração do cabeçalho usado pelo analisador.
No modo Micro C, use os nomes do TP1, como `INTEGERCONST`, `PLUS` e `ID`.
No modo próprio também são aceitos os nomes `T_...` do cabeçalho gerado.
Linhas vazias são ignoradas. O fim físico do arquivo fornece EOF; um token EOF
explícito também é aceito, desde que não haja outros tokens depois dele.
Cada registro ocupa uma linha e o lexema tem no máximo 99 bytes. Não se deve
usar uma saída de depuração que contenha quebras de linha dentro do lexema.
A coluna é opcional no modo próprio; o formato Micro C conserva apenas a linha.

## Testes

```sh
make test                 # inclui a compilação e os testes de Micro C
make test-frontend        # leitura e análise de gramáticas
make test-codegen         # autômatos, scanner e geração
```

- Frontend: 40 casos e um teste dos atributos internos das subexpressões.
- Autômatos: 33 casos de maior prefixo e 15 regex inválidas/anuláveis.
- Scanner Micro C: escapes, conteúdo com `\0`, posição de erros e reinicialização.
- Geração: 30 casos adicionais, incluindo prioridades, `--tokens`, `--tree`,
  nomes de tokens em conflito e preservação das saídas quando há erro.
- Suíte principal: 24 testes (5 gramáticas e 19 programas), incluindo três
  programas aceitos e três rejeitados por gramática válida.

As gramáticas válidas são expressões aritméticas, Micro C e a notação EBNF.
As duas gramáticas inválidas exercitam conflito LL(1) e recursão à esquerda.
Os exemplos existentes do TP1 em `../../tests/` continuam separados desta suíte.

A compilação foi verificada com `-ansi -pedantic-errors -Wall -Wextra -Werror`.
O C emitido pelo Flex é a exceção prevista pelo código-base. Os testes de
memória podem ser repetidos com Valgrind usando `GERADOR` e `EXECUTOR` em
`testes/codegen/executar.sh`.

## Extras

Implementado: descrição da própria notação EBNF em
`testes/gramaticas/ebnf.ebnf` (+0,5). O analisador gerado reconhece a própria
gramática; há três programas aceitos e três rejeitados para essa notação.
A metagramática descreve a sintaxe; as verificações semânticas continuam sendo
feitas pelo gerador depois da leitura.

