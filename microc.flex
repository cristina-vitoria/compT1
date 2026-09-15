/*
 * microc.flex
 *
 * Disciplina: Compiladores I - FACOM
 *
 * Compilacao:
 *      flex microc.flex
 *      gcc lex.yy.c -o lexer
 *
 * Uso:
 *      ./lexer test.mc
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------
 * 1. VOCABULARIO DE TOKENS (equivalente a tokens.h)
 * ------------------------------------------------------------------- */

typedef enum {
    /* Tokens fundamentais */
    UNDEF,          /* token indefinido (usado para reportar erros) */
    ID,             /* identificador                                */
    END_OF_FILE,    /* fim de arquivo                                */

    /* Constantes literais */
    INTEGERCONST,
    CHARCONST,
    STRINGCONST,

    /* Operadores aritmeticos */
    PLUS, MINUS, MUL, DIV, MOD,

    /* Operadores relacionais e logicos */
    EQ, NEQ, LT, GT, LEQ, GEQ, AND, OR, NOT,

    /* Simbolos de atribuicao e pontuacao */
    ASSIGN, SEMICOLON, COMMA, LPAREN, RPAREN,
    LBRACE, RBRACE, LBRACKET, RBRACKET,

    /* Palavras reservadas */
    MAIN, IF, ELSE, FOR, RETURN, INT, CHAR, PRINT
} TokenType;

/* Nomes dos tokens, usados apenas pelo main() de teste abaixo para
 * imprimir o tipo de cada token de forma legivel. Mantenha esta lista
 * na MESMA ORDEM do enum TokenType. */
static const char *nome_token[] = {
    "UNDEF", "ID", "END_OF_FILE",
    "INTEGERCONST", "CHARCONST", "STRINGCONST",
    "PLUS", "MINUS", "MUL", "DIV", "MOD",
    "EQ", "NEQ", "LT", "GT", "LEQ", "GEQ", "AND", "OR", "NOT",
    "ASSIGN", "SEMICOLON", "COMMA", "LPAREN", "RPAREN",
    "LBRACE", "RBRACE", "LBRACKET", "RBRACKET",
    "MAIN", "IF", "ELSE", "FOR", "RETURN", "INT", "CHAR", "PRINT"
};

/* Valor semantico do token corrente. */
typedef struct {
    char *symbol;      /* lexema para ID, INTEGERCONST, CHARCONST, STRINGCONST */
    char *error_msg;   /* mensagem de erro, usada apenas quando tipo == UNDEF   */
} YYSTYPE;

YYSTYPE microc_yylval;

/* Linha atual do arquivo-fonte sendo processado. Deve ser incrementada
 * toda vez que uma quebra de linha for consumida pelo scanner (seja em
 * codigo "normal", dentro de comentarios ou dentro de strings). */
int linha_atual = 1;

/* Funcao auxiliar para preencher microc_yylval.symbol com uma copia do
 * texto reconhecido (yytext). */
static void guarda_lexema(void) {
    microc_yylval.symbol = strdup(yytext);
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
    str_buf = malloc(str_buf_cap);
    str_buf_len = 0;
}

static void str_buf_adiciona(char c) {
    if (str_buf_len + 1 >= str_buf_cap) {
        str_buf_cap *= 2;
        str_buf = realloc(str_buf, str_buf_cap);
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
<INITIAL><<EOF>>    { return emitir(END_OF_FILE); }

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
                        char buf[2] = { charlit_valor, '\0' };
                        microc_yylval.symbol = strdup(buf);
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
                        str_buf_adiciona('\0');
                        microc_yylval.symbol = strdup(str_buf);
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
                        microc_yylval.error_msg = strdup(yytext);
                        return emitir(UNDEF);
                    }

%%

/* -----------------------------------------------------------------------
 * 4. SUB-ROTINAS DO USUARIO
 * ------------------------------------------------------------------- */

/* yywrap: informa ao flex que, ao atingir o EOF, a leitura deve
 * simplesmente parar (nao ha um proximo arquivo a processar). */
int yywrap(void) {
    return 1;
}

/* main() de teste: le o arquivo passado como argumento e imprime, para
 * cada token reconhecido, seu tipo, lexema e linha -- no mesmo espirito
 * do utilitario "lexer" mencionado no enunciado (Secao 6). Este main()
 * e apenas uma ferramenta de depuracao para voce testar seu scanner de
 * forma isolada; ele NAO faz parte da interface formal entre o scanner
 * e o parser (isso sera tratado nos trabalhos seguintes). */
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <arquivo.mc>\n", argv[0]);
        return 1;
    }

    FILE *arquivo_fonte = fopen(argv[1], "r");
    if (!arquivo_fonte) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'\n", argv[1]);
        return 1;
    }
    yyin = arquivo_fonte;

    int tipo;
    while ((tipo = yylex()) != END_OF_FILE) {
        if (tipo == UNDEF) {
            fprintf(stderr, "ERRO LEXICO (linha %d): %s\n",
                    linha_atual, microc_yylval.error_msg);
            continue;
        }
        /* Para ID, INTEGERCONST, CHARCONST e STRINGCONST o lexema "oficial"
         * fica em microc_yylval.symbol (Secao 5 do enunciado). Isso e
         * necessario em especial para CHARCONST/STRINGCONST, cujo
         * reconhecimento usa estados exclusivos que casam o conteudo aos
         * poucos: nesses casos, yytext no momento do return contem apenas
         * o ultimo fragmento casado (a aspas de fechamento), nao o lexema
         * inteiro. Para os demais tokens (operadores, pontuacao, palavras
         * reservadas), yytext ja e o lexema completo. */
        if (tipo == ID || tipo == INTEGERCONST ||
            tipo == CHARCONST || tipo == STRINGCONST) {
            printf("Token: tipo = %-13s lexema = '%s'  linha = %d\n",
                   nome_token[tipo], microc_yylval.symbol, linha_atual);
        } else {
            printf("Token: tipo = %-13s lexema = '%s'  linha = %d\n",
                   nome_token[tipo], yytext, linha_atual);
        }
    }

    fclose(arquivo_fonte);
    return 0;
}
