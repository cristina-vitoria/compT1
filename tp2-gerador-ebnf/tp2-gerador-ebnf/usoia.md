# Registro de uso de IA
Ferramenta e modelo: Perplexity 

Trecho ou arquivo afetado: `microc.flex` e `test.mc`.

Finalidade do uso: auxiliar na organização da implementação do scanner Micro C, revisar a cobertura de tokens, comentários, strings, caracteres, escapes e erros léxicos e auxilio para estruturar casos de teste.

O que fiz com a resposta: usei as sugestões como orientação.

Ferramenta e modelo: Claude Sonnet 5.5 (Anthropic), pelo chat do claude.ai

Trecho ou arquivo afetado: nenhum arquivo do projeto foi escrito ou alterado pela IA. A IA leu o projeto inteiro (src/, microc/, testes/, Makefile, README.md e o enunciado trabalho2.pdf), mas apenas para conferência.

Finalidade do uso: verificar o trabalho antes do envio. A IA compilou o gerador e rodou make test, executou manualmente os analisadores gerados (expressões, Micro C e a metagramática EBNF), conferiu os casos de rejeição e as gramáticas inválidas, rodou o Valgrind em busca de vazamentos e comparou o resultado com os requisitos do enunciado (ANSI C sem avisos, cinco gramáticas, três programas aceitos e três rejeitados por gramática válida, extras). Também listou os comandos para testar tudo.

O que fiz com a resposta: usei o relatório como orientação para revisar o trabalho.
