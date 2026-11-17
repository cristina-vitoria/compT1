/* Scanner do TP1 adaptado para uso sob demanda no TP2. */
%option prefix="microc_yy" noyywrap noinput nounput never-interactive nodefault
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scanner_interno.h"

/* Posicao inicial do token, preservada dentro de strings/comentarios. */
int linha_token = 1;
#define YY_USER_ACTION if (YY_START == INITIAL) linha_token = linha_atual;
MicrocValor microc_yylval;

static char *copiar(const char *texto, size_t n)
{
    char *copia = (char *) malloc(n + 1);
    if (copia == NULL) {
        fprintf(stderr, "memoria insuficiente\n");
        exit(EXIT_FAILURE);
    }
    memcpy(copia, texto, n);
    copia[n] = '\0';
    return copia;
}

/* Linha atual do arquivo-fonte sendo processado. Deve ser incrementada
 * toda vez que uma quebra de linha for consumida pelo scanner (seja em
 * codigo "normal", dentro de comentarios ou dentro de strings). */
int linha_atual = 1;

/* Funcao auxiliar para preencher microc_yylval.symbol com uma copia do
 * texto reconhecido (yytext). */
static void guarda_lexema(void) {
    microc_yylval.tamanho = (size_t) yyleng;
    microc_yylval.symbol = copiar(yytext, microc_yylval.tamanho);
}

/* ---------------------------------------------------------------------
 * Rastreamento do ultimo token retornado.
 *
 * E usado apenas para decidir, ao encontrar um '-' seguido imediatamente
 * de digitos (sem espaco), se ele e o operador de subtracao (quando o
 * token anterior pode "terminar" uma expressao, como um identificador,
 * uma constante ou um ')'/']') ou se e o sinal de um literal inteiro
 * negativo (em qualquer outro caso, por exemplo apos '=', '(', ',', um
 * operador, uma palavra reservada, ou no inicio do arquivo).
 * ------------------------------------------------------------------- */
static TokenType ultimo_token = UNDEF;

static int emitir(TokenType tipo) {
    ultimo_token = tipo;
    return (int) tipo;
}

/* Indica se o token 't' pode ser o ultimo token de uma expressao, ou
 * seja, se um '-' logo em seguida deve ser interpretado como operador
 * de subtracao em vez de sinal de um numero negativo. */
static int eh_fim_de_expressao(TokenType t) {
    switch (t) {
        case ID:
        case INTEGERCONST:
        case CHARCONST:
        case STRINGCONST:
        case RPAREN:
        case RBRACKET:
            return 1;
        default:
            return 0;
    }
}

/* ---------------------------------------------------------------------
 * Sequencias de escape suportadas em constantes de caractere e string:
 * \n \t \\ \" \' \0
 * Qualquer outra sequencia "\X" nao reconhecida e tratada de forma
 * permissiva, produzindo apenas o caractere X (sem a barra).
 * ------------------------------------------------------------------- */
static char decodifica_escape(char c) {
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '\\': return '\\';
        case '"': return '"';
        case '\'': return '\'';
        case '0': return '\0';
        default: return c;
    }
}

/* ---------------------------------------------------------------------
 * Buffer dinamico usado para acumular o conteudo de uma STRINGCONST
 * enquanto o scanner esta no estado exclusivo STRING (o tamanho final
 * da string nao e conhecido de antemao, entao usamos um buffer que
 * cresce conforme necessario).
 * ------------------------------------------------------------------- */
static char *str_buf = NULL;
static size_t str_buf_len = 0;
static size_t str_buf_cap = 0;

static void str_buf_iniciar(void) {
    str_buf_cap = 32;
    str_buf = (char *) malloc(str_buf_cap);
    if (str_buf == NULL) {
        fprintf(stderr, "memoria insuficiente\n");
        exit(EXIT_FAILURE);
    }
    str_buf_len = 0;
}

static void str_buf_adiciona(char c) {
    if (str_buf_len + 1 >= str_buf_cap) {
        char *novo;
        str_buf_cap *= 2;
        novo = (char *) realloc(str_buf, str_buf_cap);
        if (novo == NULL) {
            fprintf(stderr, "memoria insuficiente\n");
            exit(EXIT_FAILURE);
        }
        str_buf = novo;
    }
    str_buf[str_buf_len++] = c;
}

static void str_buf_liberar(void) {
    free(str_buf);
    str_buf = NULL;
    str_buf_len = 0;
    str_buf_cap = 0;
}

/* ---------------------------------------------------------------------
 * Estado auxiliar para constantes de caractere (estado exclusivo
 * CHARLIT). Como uma CHARCONST valida tem no maximo um caractere de
 * conteudo, basta contar quantos "caracteres logicos" foram vistos
 * (charlit_len) e guardar o ultimo valor decodificado (charlit_valor)
 * para detectar constantes vazias ('') ou mal formadas ('ab').
 * ------------------------------------------------------------------- */
static int charlit_len = 0;
static char charlit_valor = '\0';

%}

/* -----------------------------------------------------------------------
 * 2. SECAO DE DEFINICOES
 * ------------------------------------------------------------------- */

DIGIT       [0-9]
LETRA       [a-zA-Z_]
ALFANUM     [a-zA-Z0-9_]

%x COMMENT
%x STRING
%x CHARLIT

%%

 /* -----------------------------------------------------------------------
  * 3. SECAO DE REGRAS
  * --------------------------------------------------------------------- */

 /* --- Fim de arquivo -----------------------------------------------------
  * Tratada explicitamente (em vez de depender do retorno automatico 0
  * do flex), pois o token UNDEF tambem vale 0 no enum TokenType -- se
  * dependessemos do comportamento padrao, um erro lexico seria
  * confundido com o fim do arquivo pelo main() de teste abaixo. */
<INITIAL><<EOF>>    { linha_token = linha_atual; return emitir(END_OF_FILE); }

<INITIAL,CHARLIT>\x00 {
                        microc_yylval.error_msg = "Caractere nulo na entrada";
                        BEGIN(INITIAL);
                        return emitir(UNDEF);
                    }


 /* --- Espacos em branco e quebras de linha ---------------------------- */
\n                  { linha_atual++; }
[ \t\r]+            { /* ignora espacos em branco */ }

 /* --- Comentarios ------------------------------------------------------
  * Estes ja estao implementados como exemplo de uso de estados (%x) e
  * de tratamento de erro via EOF dentro de um estado especial. */
"//".*              { /* comentario de linha: ignora ate o fim da linha */ }

"/*"                { BEGIN(COMMENT); }
<COMMENT>"*/"       { BEGIN(INITIAL); }
<COMMENT>\n         { linha_atual++; }
<COMMENT><<EOF>>    {
                        microc_yylval.error_msg = "EOF em comentario";
                        BEGIN(INITIAL);
                        return emitir(UNDEF);
                    }
<COMMENT>.          { /* consome qualquer outro caractere dentro do comentario */ }

 /* Fechamento de comentario sem abertura correspondente. */
"*/"                {
                        microc_yylval.error_msg = "Comentario nao iniciado";
                        return emitir(UNDEF);
                    }

 /* --- Palavras reservadas e identificadores ----------------------------
  * O lexema reconhecido e comparado com cada palavra reservada da
  * linguagem; se houver correspondencia, o token especifico e
  * devolvido. Caso contrario, trata-se de um ID comum, e o lexema e
  * armazenado via guarda_lexema(). */
{LETRA}{ALFANUM}*   {
                        if (strcmp(yytext, "main") == 0)   return emitir(MAIN);
                        if (strcmp(yytext, "if") == 0)     return emitir(IF);
                        if (strcmp(yytext, "else") == 0)   return emitir(ELSE);
                        if (strcmp(yytext, "for") == 0)    return emitir(FOR);
                        if (strcmp(yytext, "return") == 0) return emitir(RETURN);
                        if (strcmp(yytext, "int") == 0)    return emitir(INT);
                        if (strcmp(yytext, "char") == 0)   return emitir(CHAR);
                        if (strcmp(yytext, "print") == 0)  return emitir(PRINT);

                        guarda_lexema();
                        return emitir(ID);
                    }

 /* --- Constantes inteiras (incluindo negativas) -------------------------
  * Um '-' seguido imediatamente de um ou mais digitos (sem espaco) e
  * ambiguo: pode ser o sinal de um literal negativo ou o operador de
  * subtracao seguido de um literal positivo (ex.: "x-1"). Resolvemos
  * essa ambiguidade olhando o ultimo token emitido: se ele pode encerrar
  * uma expressao (ID, constante, ')' ou ']'), o '-' e subtracao; usamos
  * entao yyless(1) para "devolver" os digitos ao buffer de entrada,
  * emitindo apenas o MINUS agora. Caso contrario, o '-' faz parte do
  * literal negativo. */
"-"{DIGIT}+         {
                        if (eh_fim_de_expressao(ultimo_token)) {
                            yyless(1); /* mantem so o '-' em yytext, devolve os digitos */
                            return emitir(MINUS);
                        }
                        guarda_lexema();
                        return emitir(INTEGERCONST);
                    }

{DIGIT}+            {
                        guarda_lexema();
                        return emitir(INTEGERCONST);
                    }

 /* --- Constantes de caractere --------------------------------------------
  * Usamos um estado exclusivo (CHARLIT), no mesmo espirito do estado
  * COMMENT ja fornecido, para tratar corretamente EOF e quebras de
  * linha no meio de uma constante de caractere. charlit_len conta
  * quantos caracteres "logicos" (apos decodificar escapes) foram
  * vistos entre as aspas simples, o que permite detectar constantes
  * vazias ('') e mal formadas ('ab') sem precisar de um buffer. */
"'"                 { BEGIN(CHARLIT); charlit_len = 0; charlit_valor = '\0'; }

<CHARLIT>\\[ntr\\'"0]  {
                        charlit_valor = decodifica_escape(yytext[1]);
                        charlit_len++;
                    }
<CHARLIT>\\.        {
                        /* sequencia de escape desconhecida: usada de forma
                         * permissiva, mantendo apenas o caractere apos a
                         * barra invertida. */
                        charlit_valor = yytext[1];
                        charlit_len++;
                    }
<CHARLIT>[^'\\\n]   { charlit_valor = yytext[0]; charlit_len++; }

<CHARLIT>"'"        {
                        BEGIN(INITIAL);
                        if (charlit_len == 0) {
                            microc_yylval.error_msg = "Constante de caractere vazia";
                            return emitir(UNDEF);
                        }
                        if (charlit_len > 1) {
                            microc_yylval.error_msg = "Constante de caractere mal formada";
                            return emitir(UNDEF);
                        }
                        microc_yylval.tamanho = 1;
                        microc_yylval.symbol = copiar(&charlit_valor, 1);
                        return emitir(CHARCONST);
                    }
<CHARLIT>\n         {
                        microc_yylval.error_msg = "Constante de caractere nao terminada";
                        BEGIN(INITIAL);
                        linha_atual++;
                        return emitir(UNDEF);
                    }
<CHARLIT><<EOF>>    {
                        microc_yylval.error_msg = "EOF em constante de caractere";
                        BEGIN(INITIAL);
                        return emitir(UNDEF);
                    }

 /* --- Constantes de string -------------------------------------------
  * Tambem implementadas com um estado exclusivo (STRING), pois o
  * conteudo tem tamanho variavel: usamos o buffer dinamico str_buf
  * para acumular os caracteres (ja com os escapes decodificados) ate
  * encontrar as aspas de fechamento. */
"\""                { BEGIN(STRING); str_buf_iniciar(); }

<STRING>\\[ntr\\'"0]  { str_buf_adiciona(decodifica_escape(yytext[1])); }
<STRING>\\.         {
                        /* escape desconhecido: mantem apenas o caractere
                         * apos a barra, de forma permissiva. */
                        str_buf_adiciona(yytext[1]);
                    }
<STRING>"\""        {
                        BEGIN(INITIAL);
                        microc_yylval.tamanho = str_buf_len;
                        microc_yylval.symbol = copiar(str_buf, str_buf_len);
                        str_buf_liberar();
                        return emitir(STRINGCONST);
                    }
<STRING>\n          {
                        microc_yylval.error_msg = "String nao terminada";
                        BEGIN(INITIAL);
                        linha_atual++;
                        str_buf_liberar();
                        return emitir(UNDEF);
                    }
<STRING><<EOF>>     {
                        microc_yylval.error_msg = "EOF em string";
                        BEGIN(INITIAL);
                        str_buf_liberar();
                        return emitir(UNDEF);
                    }
<STRING>.           {
                        if (yytext[0] == '\0') {
                            microc_yylval.error_msg = "String contem caractere nulo";
                            BEGIN(INITIAL);
                            str_buf_liberar();
                            return emitir(UNDEF);
                        }
                        str_buf_adiciona(yytext[0]);
                    }

<CHARLIT>"\\"      {
                        microc_yylval.error_msg = "Escape incompleto em caractere";
                        BEGIN(INITIAL);
                        return emitir(UNDEF);
                    }

 /* --- Operadores relacionais e logicos ---------------------------------
  * O caso de '=' esta implementado como EXEMPLO do uso de lookahead
  * (o flex escolhe automaticamente o casamento mais longo entre regras
  * que compartilham prefixo). Os demais operadores seguem o mesmo
  * padrao. */
"=="                { return emitir(EQ); }
"="                 { return emitir(ASSIGN); }

"!="                { return emitir(NEQ); }
"!"                 { return emitir(NOT); }
"<="                { return emitir(LEQ); }
"<"                 { return emitir(LT); }
">="                { return emitir(GEQ); }
">"                 { return emitir(GT); }
"&&"                { return emitir(AND); }
"||"                { return emitir(OR); }

 /* --- Operadores aritmeticos e simbolos de pontuacao (ja prontos) ------ */
"+"                 { return emitir(PLUS); }
"-"                 { return emitir(MINUS); }
"*"                 { return emitir(MUL); }
"/"                 { return emitir(DIV); }
"%"                 { return emitir(MOD); }
";"                 { return emitir(SEMICOLON); }
","                 { return emitir(COMMA); }
"("                 { return emitir(LPAREN); }
")"                 { return emitir(RPAREN); }
"{"                 { return emitir(LBRACE); }
"}"                 { return emitir(RBRACE); }
"["                 { return emitir(LBRACKET); }
"]"                 { return emitir(RBRACKET); }

 /* --- Caractere invalido -------------------------------------------------
  * Casa com qualquer caractere que nao tenha correspondido a nenhuma
  * regra anterior. Deve ser SEMPRE a ultima regra do arquivo. */
.                   {
                        static char invalido[2];
                        invalido[0] = yytext[0];
                        invalido[1] = '\0';
                        microc_yylval.error_msg = invalido;
                        return emitir(UNDEF);
                    }

%%

void microc_liberar_recursos(void)
{
    free(microc_yylval.symbol);
    memset(&microc_yylval, 0, sizeof microc_yylval);
    str_buf_liberar();
}

void microc_preparar(FILE *entrada)
{
    microc_liberar_recursos();
    BEGIN(INITIAL);
    ultimo_token = UNDEF;
    linha_atual = linha_token = 1;
    yyrestart(entrada);
}
