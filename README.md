# rSystemOS

Projeto para estudo sobre como desenvolver um Kernel e um bootload basicos.

## Introdução

Por efeito de curiosidade, sempre quis saber como funcionava a fundo um sistema operacional, com isso iniciei as pesquisas para obter o conhecimento de como era construído.

No meio dessa pesquisa, acabei encontrando diversos materiais na internet explicando como funcionava mas não mostrava o código.
Depois de um certo tempo descobri que estava fazendo a pesquisa errada.

Estava pesquisando "**como criar/desenvolver um sistema operacional do zero**", mas na verdade, a pesquisa que mais surgiu resultado foi "**como criar um bootloader/como criar um kernel**".

Com isso esbarrei em alguns materiais ricos em detalhes, códigos e explicações bem detalhadas.

O que useio como base foi o [BrokenThorn](http://www.brokenthorn.com/Resources/OSDevIndex.html) por criar, explicar com detalhes incríveis e deixar o código fonte para download.

O sistema apenas carrega em memória, porém o documento BrokenThorn, (se não me engano) mostra como fazer a leitura e gravação em disco, tanto quanto utilizar o video com uma interface gráfica.

### Prerequisitos

O que você precisa para baixar, rodar e disponibilizar.
O projeto foi desenvolvido em Linux, com isso foi apenas testado a compilação no ambiente Linux

* Uma IDE de sua preferência que consiga visualizar os arquivos em *.asm, *.c e *.h
* O compilador **gcc** instalado
* O compilador **nasm** instalado
* Um emulador de sua preferência - estou usando o QEmu por sem mais simples

### Geração da imagem para teste

Para facilitar a compilação da projeto e geração da imagem foi criado o **makefile**

O comando **make** compila na seguinte ordem

* bootloader - Gera o bin do bootlaoder através do arquivo bootloader.asm
* kernel - Compila o kernel e as bibliotecas dependentes (libs e drivers)
* load_kernel - Compila o assembler (load_kernel) que faz a execução da função main do kernel.c
* ld - Link dos buildaveis e gera o arquivo de kernel com compatibilidade de x86 (melf_i386)
* cat - Gera a imagem *.img através dos dois arquivos compilados (bootloader.bin kernel.bin)
* rm - Remove os arquivos gerados que já não são mais necessários após a geração da imagem

o comando **make exec** executa no qemu a img

## Diretórios

1. `src` - Contém os fontes do projeto
2. `src/boot` - Arquivo de bootloader 
3. `src/drivers` - Arquivos dos drivers de video e teclado
4. `src/include` - Arquivos de interface (cabeçalho da linguagem C) das bibliotecas
5. `src/kernel` - Arquivos do kernel, o assemble que faz a chamada da função main do kernel.c
6. `src/lib` - Arquivos das bibliotecas a serem carregadas em memória onde é possivel colocar seus comandos.
7. `dist` - Contém o arquivo da imgem gerada pelo comando **make** (também são gerados nessa pasta os arquivos de saída dos builds dos outros arquivos porém são removidos no final do processo)

## Execução dos testes

Não foi gerado

## Publicação

Não foi gerado

## Autores

* **Robson Pedroso** - *Projeto inicial* - [RobsonPedroso](https://github.com/robsonpedroso)

## Referências

[BrokenThorn](http://www.brokenthorn.com/Resources/OSDevIndex.html)

## License

[MIT](https://github.com/robsonpedroso/kernel_basic/blob/main/LICENSE)