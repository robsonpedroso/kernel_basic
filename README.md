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

o comando **make exec** executa no qemu a img (via `-hda`, tratando a imagem como um disco IDE bruto — o setor de boot não é mais um BPB de disquete, ver seção de orçamento de boot abaixo)

## Diretórios

1. `src` - Contém os fontes do projeto
2. `src/boot` - Bootloader (real mode, liga VGA Mode 12h, entra em modo protegido 32-bit e carrega o kernel via um driver IDE PIO próprio — não usa mais BIOS INT 13h, ver seção de orçamento de boot abaixo)
3. `src/drivers` - Vídeo (VGA Mode 12h, 640x480x16, planar), teclado (IRQ1), IDE (PIO, leitura+escrita em runtime) e RTC (leitura única do CMOS no boot)
4. `src/include` - Arquivos de interface (cabeçalho da linguagem C) das bibliotecas
5. `src/kernel` - Kernel: IDT/PIC/ISR (`idt.c`, `isr.c`, `isr.asm`, `pic.c`), GDT própria (`gdt.c`), timer PIT (`timer.c`), mouse PS/2 (`mouse.c`), fila de eventos (`event.c`), saída serial de debug (`serial.c`), sistema de arquivos (`fs.c`) e `kernel.c` (loop principal orientado a eventos)
6. `src/lib` - Bibliotecas carregadas em memória: `stdio`, `stdlib`, `string` e o heap (`heap.c`, alocador first-fit simples)
7. `src/gui` - Framework de janelas estilo Windows 3.11: `rect.c`/`widget.c`/`window.c`/`cursor.c` (chrome básico), `wm.c` (gerenciador de janelas), `textbox.c`/`editbuf.c`/`lineedit.c` (texto), `listbox.c`/`treeview.c`/`scrollbar.c`/`splitter.c`/`menubar.c`/`confirm.c` (primitivas do Gerenciador de Arquivos), e os apps em `src/gui/apps` (Program Manager, Terminal, Calculator, Info, Editor de Texto, Gerenciador de Arquivos)
8. `dist` - Contém o arquivo da imgem gerada pelo comando **make** (também são gerados nessa pasta os arquivos de saída dos builds dos outros arquivos porém são removidos no final do processo)

## Arquitetura (a partir da v0.3)

O kernel ganhou infraestrutura básica de sistema operacional, além do que já existia (bootloader próprio, modo protegido 32-bit, driver de vídeo):

* **Interrupções**: IDT com 256 entradas, PIC 8259 remapeado para os vetores 0x20-0x2F, stubs de exceção/IRQ em `isr.asm`.
* **Timer**: PIT programado a 100Hz (IRQ0), contador de ticks acessível via `timer_get_ticks()`.
* **Teclado**: passou de polling para IRQ1, com rastreio de Shift/Ctrl/Alt.
* **Mouse PS/2**: driver de IRQ12 (porta 0x60/0x64), posição de cursor com clamping à tela.
* **Fila de eventos**: buffer circular consumido via `event_wait()` (`hlt`-based, sem busy-wait), unificando teclado/mouse/timer.
* **Heap**: alocador `kmalloc`/`kfree` (first-fit) sobre uma região fixa de 64KB.
* **Vídeo**: migrado de Mode 13h (320x200x256) para Mode 12h (640x480, 16 cores, planar via registradores da Graphics Controller/Sequencer).

## Interface gráfica (a partir da v0.4)

A tela de launcher virou uma janela "Program Manager" real, usando o framework em `src/gui`:

* **Janela**: barra de título azul + borda com bevel 3D (`window.c`).
* **Botões**: bevel raised/sunken estilo Win3.11, foco por teclado indicado por um retângulo interno (`widget.c`).
* **Mouse**: cursor com sprite de seta, salvando/restaurando o fundo sob ele (não há framebuffer sombra ainda, então cada redraw de tela esconde o cursor antes e mostra depois); hover sobre um botão move o foco, clique esquerdo ativa o botão (mesmo efeito do Enter).

**Orçamento de boot (atualizado)**: o bootloader não usa mais INT 13h Extensions da BIOS — ele entra em modo protegido e carrega o kernel através de um driver IDE PIO próprio (`ide_load_kernel` em `src/boot/bootloader.asm`), falando direto com a controladora (`0x1F0-0x1F7`, LBA28), sem qualquer envolvimento da BIOS. Isso eliminou o teto cumulativo de ~24KB que o SeaBIOS impunha às leituras estendidas. O orçamento atual é `IDE_SECTORS = 800` (400KB) — o kernel hoje usa ~66 setores (~33KB), sobrando ~367KB de folga. O segundo teto que existe é o `ASSERT(. <= 0x60000)` em `link.ld` (o início do heap, ver `heap.c`), que cobre `.text+.rodata+.data+.bss` combinados e falha em tempo de link, não em runtime, caso a imagem cresça demais — esse assert é o alarme antecipado operante para qualquer crescimento futuro do kernel, bem antes do teto de 400KB do carregador IDE ser sequer aproximado.

Se um dia o orçamento de 400KB realmente apertar, o fallback é simples e não exige um novo estágio de boot: aumentar `IDE_SECTORS` em `src/boot/bootloader.asm` e deslocar `FS_SUPER_LBA`/`FS_TABLE_LBA`/`FS_DATA_LBA`/`FS_END_LBA` em `src/include/fs.h` pelo mesmo delta (a região do sistema de arquivos começa logo depois do range do kernel), seguido de `make distclean && make all` — realocar a região do FS não preserva os dados já gravados na imagem, efeito equivalente a um `distclean`.

## Gerenciador de Arquivos estilo Windows 3.11 (a partir da v0.5)

O app "Arquivos" (`src/gui/apps/file_manager.c`) deixou de ser uma lista
single-pane e ganhou a estrutura clássica do `winfile.exe`: barra de menus
(Arquivo/Árvore/Exibir/Opções/Janela/Ajuda) com dropdown, uma barra de
drive ("C:", único disco real por trás), árvore de pastas e lista de
arquivos lado a lado com um splitter arrastável e scrollbars, e uma barra
de status mostrando o caminho atual e o espaço livre em disco.

Suporta multi-seleção (clique/Ctrl-clique/Shift-clique), renomear, mover e
copiar (por menu, escolhendo o destino na árvore, ou por drag&drop direto
da lista pra árvore — segurar Ctrl no drop copia em vez de mover),
ordenação por nome/tipo/tamanho/data, e confirmação opcional antes de
excluir. "Abrir" um arquivo usa uma tabela de associação por extensão
(hoje só `.txt` → Editor de Texto) — não é um "Abrir Com" completo e não
executa programas: este kernel não tem processo/exec, só janelas com
`app_st` compilados estaticamente no binário.

O sistema de arquivos por trás (`src/kernel/fs.c`/`fs.h`) também ganhou
`fs_rename`/`fs_move`/`fs_copy`/`fs_path`/`fs_set_attr`, atributos
(RO/hidden/system/archive, só exibição por enquanto) e timestamps reais
(lidos do CMOS via `src/drivers/rtc.c` no boot). O formato on-disk foi de
64 para 128 bytes por entrada (v1 → v2) — uma imagem de disco antiga é
reformatada automaticamente na próxima boot.

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