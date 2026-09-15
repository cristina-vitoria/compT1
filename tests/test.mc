// test.mc - Programa de teste para o scanner de Micro C
//
// Este arquivo exercita boa parte dos tokens da linguagem: palavras
// reservadas, identificadores, constantes inteiras, de caractere e de
// string, operadores aritmeticos, relacionais e logicos, simbolos de
// pontuacao, alem de comentarios de linha e de bloco.
//
// LEMBRETE: este teste NAO cobre todos os casos (em especial, poucos
// casos de erro lexico). Parte do trabalho e ampliar este arquivo com
// testes proprios que exercitem seu scanner de forma mais completa,
// incluindo entradas invalidas (veja a Secao 4.1 do enunciado).

/* Calcula o fatorial de um numero inteiro utilizando um laco for. */
int fatorial(int n) {
    int resultado;
    int i;

    resultado = 1;
    for (i = 1; i <= n; i = i + 1) {
        resultado = resultado * i;
    }
    return resultado;
}

int main() {
    int x;
    int y;
    char c;
    char letras[10];

    x = 5;
    y = fatorial(x);

    if (y > 100) {
        print("Resultado grande");
    } else {
        print("Resultado pequeno");
    }

    c = 'A';
    letras[0] = 'H';
    letras[1] = 'i';

    // Testando operadores relacionais e logicos
    if (x >= 0 && y != 0) {
        print("x e nao-negativo e y e diferente de zero");
    }

    if (x == 5 || y == 0) {
        print("condicao ou satisfeita");
    }

    if (!(x < 0)) {
        print("x nao e negativo");
    }

    int contador, limite, resultado;
    char inicial, quebra, tabulacao;

    contador = 0;
    limite = 10;
    resultado = -42;

    inicial = 'A';
    quebra = '\n';
    tabulacao = '\t';

    /* ---------- Operadores aritméticos ---------- */
    resultado = contador + limite;
    resultado = resultado - 1;
    resultado = resultado * 2;
    resultado = resultado / 3;
    resultado = resultado % 2;

    /* ---------- Operadores relacionais e lógicos ---------- */
    if (contador == 0) {
        print("contador igual a zero");
    }

    if (contador != limite) {
        print("contador diferente do limite");
    }

    if (contador < limite) {
        print("contador menor que limite");
    }

    if (limite > contador) {
        print("limite maior que contador");
    }

    if (contador <= limite) {
        print("contador menor ou igual ao limite");
    }

    if (limite >= contador) {
        print("limite maior ou igual ao contador");
    }

    if (contador == 0 && limite > 0) {
        print("operador AND");
    }

    if (contador != 0 || limite == 10) {
        print("operador OR");
    }

    if (!(contador > limite)) {
        print("operador NOT");
    }

    /* ---------- For, arrays, vírgula e parênteses ---------- */
    int valores[5];
    valores[0] = 1;
    valores[1] = 2;
    valores[2] = 3;
    valores[3] = 4;
    valores[4] = 5;

    for (contador = 0; contador < 5; contador = contador + 1) {
        print("percorrendo vetor");
    }

    /* ---------- Strings e escapes ---------- */
    print("texto simples");
    print("linha 1\nlinha 2");
    print("coluna\tseguinte");
    print("barra \\ invertida");
    print("aspas \"duplas\"");
    print("caractere nulo: \0");

    /* ---------- Comentário de linha ---------- */
    // este comentário deve ser completamente ignorado pelo scanner

    /* ---------- Comentário de bloco
       com mais de uma linha ---------- */

    else {
        return 0;
    }
}

/*
 * ---------- Casos inválidos para testes separados ----------
 *
 * 1. Caractere inválido:
 *    @
 *
 * 2. Comentário não iniciado:
 *    */
 *
 * 3. Caractere não terminado:
 *    '
 *
 * 4. String não terminada:
 *    "string sem fechamento
 *
 * 5. Escape inválido:
 *    "escape inválido: \x"
 *
 * 6. EOF em comentário de bloco:
 *    /* comentário sem fechamento
 *
 * 7. EOF em string:
 *    "string sem fechamento no fim do arquivo
 */