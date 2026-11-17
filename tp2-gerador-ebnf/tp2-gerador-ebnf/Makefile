# Makefile do gerador de analisadores sintaticos descendentes recursivos
#
#   make            compila o gerador (e o analisador de Micro C, se os
#                   arquivos do TP1 estiverem em microc/)
#   make microc     gera e compila o analisador de Micro C com o scanner do TP1
#   make test       executa os testes automatizados
#   make clean      remove os arquivos gerados

CC      = gcc
CFLAGS  = -ansi -pedantic -Wall -Wextra -g
# O lex.yy.c gerado pelo flex e a unica excecao a regra de "sem avisos".
CFLAGS_FLEX = -g -w
# Fontes C do TP1 alem do scanner.c (tabela de strings etc.) e bibliotecas.
TP1_FONTES = $(filter-out microc/scanner.c, $(wildcard microc/*.c))
LDLIBS_MICROC =

FONTES  = $(wildcard src/*.c)
OBJETOS = $(patsubst src/%.c, build/obj/%.o, $(FONTES))

ifneq ($(wildcard microc/microc.flex),)
all: gerador microc
else
all: gerador
endif

gerador: $(OBJETOS)
	$(CC) $(CFLAGS) -o $@ $(OBJETOS)

build/obj/%.o: src/%.c $(wildcard src/*.h) | build/obj
	$(CC) $(CFLAGS) -c -o $@ $<

build/obj build/microc:
	mkdir -p $@

# ---- Micro C (secao 3.1 do enunciado) ----

MICROC_DIR = build/microc

microc: $(MICROC_DIR)/parser_microc

$(MICROC_DIR)/parser.c $(MICROC_DIR)/parser.h &: gerador testes/gramaticas/microc.ebnf microc/tokens.h | build/microc
	./gerador testes/gramaticas/microc.ebnf -o $(MICROC_DIR)/parser --scanner microc --tree

$(MICROC_DIR)/lex.yy.c: microc/microc.flex | build/microc
	flex -o $@ $<

$(MICROC_DIR)/lex.yy.o: $(MICROC_DIR)/lex.yy.c microc/tokens.h microc/scanner_interno.h
	$(CC) $(CFLAGS_FLEX) -Imicroc -c -o $(MICROC_DIR)/lex.yy.o $(MICROC_DIR)/lex.yy.c

$(MICROC_DIR)/parser_microc: $(MICROC_DIR)/parser.c $(MICROC_DIR)/parser.h $(MICROC_DIR)/lex.yy.o microc/scanner.c microc/scanner.h microc/scanner_interno.h microc/tokens.h $(TP1_FONTES)
	$(CC) $(CFLAGS) -Imicroc -I$(MICROC_DIR) -o $@ $(MICROC_DIR)/parser.c microc/scanner.c \
		$(TP1_FONTES) $(MICROC_DIR)/lex.yy.o $(LDLIBS_MICROC)

microc/tokens.h microc/microc.flex:
	@echo "Copie o $@ do seu TP1 para a pasta microc/ (veja microc/LEIAME.md)." && false

# ---- testes ----

test: test-frontend test-codegen microc
	sh testes/executar.sh

# Etapas de leitura e analise, independentes da geracao de codigo.
build/testes/frontend/atributos: testes/frontend/conjuntos.c $(filter-out build/obj/main.o, $(OBJETOS))
	mkdir -p build/testes/frontend
	$(CC) $(CFLAGS) -Isrc -o $@ testes/frontend/conjuntos.c $(filter-out build/obj/main.o, $(OBJETOS))

test-frontend: gerador build/testes/frontend/atributos
	build/testes/frontend/atributos
	sh testes/frontend/executar.sh

build/testes/codegen:
	mkdir -p $@

build/testes/codegen/emitir-automatos: testes/codegen/automatos.c build/obj/automato.o build/obj/bitset.o build/obj/util.o | build/testes/codegen
	$(CC) $(CFLAGS) -Isrc -o $@ $^

build/testes/codegen/scanner: testes/codegen/scanner.c microc/scanner.c microc/scanner.h microc/scanner_interno.h microc/tokens.h $(MICROC_DIR)/lex.yy.o | build/testes/codegen
	$(CC) $(CFLAGS) -Imicroc -o $@ testes/codegen/scanner.c microc/scanner.c $(MICROC_DIR)/lex.yy.o

test-codegen: gerador microc build/testes/codegen/emitir-automatos build/testes/codegen/scanner
	build/testes/codegen/emitir-automatos build/testes/codegen/automatos.c 2> build/testes/codegen/regex-invalidas.log
	$(CC) $(CFLAGS) -o build/testes/codegen/automatos build/testes/codegen/automatos.c
	build/testes/codegen/automatos
	build/testes/codegen/scanner
	sh testes/codegen/executar.sh

clean:
	rm -rf build gerador

.PHONY: all microc test test-frontend test-codegen clean
