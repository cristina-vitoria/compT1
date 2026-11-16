# Trabalho de Compiladores Final 

# Trabalho Prático 

Repositório do Trabalho Prático 2 da disciplina **Compiladores I (FACOM)**:

## 👤 Autor(es)

| Nome | Matrícula |
|------|-----------|
| _Vitória Cristina Pinto_ | _202519040618_ |

## 🎓 Disciplina

- **Curso:** Compiladores I
- **Instituição:** FACOM
- **Trabalho:** TP2 

## Implementação do TP2

O gerador está em [`tp2-gerador-ebnf/tp2-gerador-ebnf`](tp2-gerador-ebnf/tp2-gerador-ebnf).
O gerador lê e valida EBNF, gera analisadores em C e integra o scanner de
Micro C. A suíte inclui cinco gramáticas e os programas de aceitação/rejeição.
O extra da metagramática EBNF está implementado; recuperação de erros, AST e
correção automática de conflitos continuam pendentes.

```sh
cd tp2-gerador-ebnf/tp2-gerador-ebnf
make
make test
./gerador testes/gramaticas/expr.ebnf --debug --sets
```

O estado detalhado e os testes estão no README do gerador.
