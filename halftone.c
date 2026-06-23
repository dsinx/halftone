#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int largura;
  int altura;
  size_t totalSize;
  unsigned char *pixels;
} Imagem;

void pularComentarios(FILE *fpointer) {
  int character;

  while ((character = (fgetc(fpointer))) != EOF) {
    if (character == ' ' || character == '\n' || character == '\t' ||
        character == '\r') {
      continue;
    } else if (character == '#') {
      while ((character = (fgetc(fpointer))) != '\n' && character != EOF)
        ;
    } else {
      ungetc(character, fpointer);
      break;
    }
  }
}

static const int bayer4[4][4] = {
    {0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

void halftoneBayer(const Imagem *src, Imagem *dst) {
  for (int i = 0; i < src->altura; i++) {
    for (int j = 0; j < src->largura; j++) {
      int pixel = src->pixels[i * src->largura + j];
      int threshold = (bayer4[i % 4][j % 4] * 255) / 15;
      dst->pixels[i * src->largura + j] = (pixel >= threshold) ? 255 : 0;
    }
  }
}

void halftoneLimiar(const Imagem *src, Imagem *dst, int limiar) {
  int totalPixelSize = src->largura * src->altura;

  for (int i = 0; i < totalPixelSize; i++) {
    if (src->pixels[i] >= limiar) {
      dst->pixels[i] = 255;
    } else {
      dst->pixels[i] = 0;
    }
  }
}

int readHeader(FILE *fpointer, int *largura, int *altura) {
  char magic[3];
  if (fscanf(fpointer, "%2s", magic) != 1 || strcmp(magic, "P5") != 0)
    return 0;

  pularComentarios(fpointer);
  if (fscanf(fpointer, "%d %d", largura, altura) != 2)
    return 0;

  if (*largura <= 0 || *altura <= 0)
    return 0;

  pularComentarios(fpointer);
  int valorMax;
  if (fscanf(fpointer, "%d", &valorMax) != 1 || valorMax != 255)
    return 0;

  fgetc(fpointer);

  return 1;
}

double mse(const Imagem *original, const Imagem *resultado) {
  double somatorio = 0.0;
  int total = original->largura * original->altura;
  for (int i = 0; i < total; i++) {
    double diff = (double)original->pixels[i] - (double)resultado->pixels[i];
    somatorio += diff * diff;
  }
  return somatorio / total;
}

Imagem *pgmCriar(int largura, int altura) {
  Imagem *img = calloc(1, sizeof(Imagem));
  if (img == NULL)
    return NULL;

  img->altura = altura;
  img->largura = largura;
  img->totalSize = (size_t)altura * largura;
  img->pixels = calloc(img->totalSize, sizeof(unsigned char));

  if (img->pixels == NULL) {
    free(img);
    return NULL;
  }

  return img;
}

Imagem *pgmLer(const char *path) {
  FILE *fpointer = fopen(path, "rb");
  int altura = 0, largura = 0;

  if (fpointer == NULL) {
    fprintf(stderr, "ERRO: nao foi possivel abrir o arquivo no path %s\n",
            path);
    return NULL;
  }

  if ((readHeader(fpointer, &largura, &altura)) == 0) {
    fprintf(stderr, "ERRO: cabecalho invalido ou corrompido, esperado PGM.\n");
    fclose(fpointer);
    return NULL;
  }

  Imagem *img = pgmCriar(largura, altura);

  if (fread(img->pixels, sizeof(unsigned char), img->totalSize, fpointer) !=
      img->totalSize) {
    fprintf(stderr, "ERRO: falha ao ler pixels da imagem.");
    free(img->pixels);
    free(img);
    fclose(fpointer);
    return NULL;
  }

  fclose(fpointer);
  return img;
}

int pgmEscrever(const Imagem *img, const char *path) {

  if (img == NULL || img->pixels == NULL) {
    fprintf(stderr, "ERRO: a imagem nao existe na memoria.");
    return 0;
  }

  FILE *fpointer = fopen(path, "wb");
  if (fpointer == NULL) {
    fprintf(stderr, "ERRO: nao foi possivel criar o arquivo no caminho %s.\n",
            path);
    return 0;
  }

  fprintf(fpointer, "P5\n");
  fprintf(fpointer, "%d %d\n", img->largura, img->altura);
  fprintf(fpointer, "255\n");

  size_t written =
      fwrite(img->pixels, sizeof(unsigned char), img->totalSize, fpointer);
  fclose(fpointer);

  if (written != img->totalSize) {
    fprintf(stderr, "ERRO: falha no processo de escrita.\n");
    return 0;
  }

  return 1;
}

void pgmLiberar(Imagem *img) {
  if (img == NULL)
    return;

  if (img->pixels != NULL)
    free(img->pixels);

  free(img);
}

int main(int argc, char *argv[]) {
  if (argc < 5) {
    fprintf(stderr,
            "USO INVALIDO!\nUso: %s <imagem.pgm> <limiar> <saida_limiar.pgm> "
            "<saida_bayer.pgm>\n",
            argv[0]);
    return 1;
  }

  const char *inputPgm = argv[1];
  int limiar = atoi(argv[2]);

  const char *outputLimiar = argv[3];
  const char *outputBayer = argv[4];

  Imagem *img = pgmLer(inputPgm);
  if (img == NULL)
    return 1;

  Imagem *inputLimiar = pgmCriar(img->largura, img->altura);
  Imagem *inputBayer = pgmCriar(img->largura, img->altura);

  if (inputBayer == NULL || inputLimiar == NULL) {
    fprintf(stderr, "ERRO: falha ao alocar memoria para o output.\n");
    pgmLiberar(img);
    pgmLiberar(inputLimiar);
    pgmLiberar(inputBayer);
    return 1;
  }

  halftoneLimiar(img, inputLimiar, limiar);
  halftoneBayer(img, inputBayer);

  pgmEscrever(inputLimiar, outputLimiar);
  pgmEscrever(inputBayer, outputBayer);

  printf("Imagem: %s (%dx%d, %zu pixels)\n", inputPgm, img->largura,
         img->altura, img->totalSize);
  printf("Limiar fixo (t=%d): MSE = %.3f\n", limiar, mse(img, inputLimiar));
  printf("Bayer 4x4: MSE = %.3f\n", mse(img, inputBayer));

  pgmLiberar(img);
  pgmLiberar(inputLimiar);
  pgmLiberar(inputBayer);

  return 0;
}
