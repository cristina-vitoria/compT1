# Scanner Micro C integrado

`microc.flex` é a adaptação do scanner do TP1. O scanner original permanece
em `../../microc.flex` para consulta.

- `tokens.h`: enumeração compartilhada e estrutura `Token`, com comprimento.
- `scanner.h`: interface usada pelo analisador gerado.
- `scanner_interno.h`: interface entre o Flex e o adaptador.
- `scanner.c`: obtém o próximo token sem imprimir mensagens e preserva
  lexemas, posição inicial e mensagens de erro.

O scanner usa `%option prefix="microc_yy" noyywrap`. Ele retorna `TokenType`,
inclusive para operadores de um caractere; EOF é `END_OF_FILE`. O analisador
imprime os diagnósticos e encerra ao receber `UNDEF`.

O adaptador e a rotina de finalização liberam lexemas e buffers. É possível
reinicializar o scanner para outro arquivo. O comprimento explícito preserva
`\0` dentro de valores de strings e caracteres. O limite da interface é
99 bytes por lexema; entradas maiores produzem `UNDEF`.

```sh
make microc
make test
```

O código gerado pelo Flex é compilado separadamente e é a única exceção à
exigência de ANSI C sem avisos. As demais fontes seguem as flags do projeto.
