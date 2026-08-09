# Próximos passos — rSystemOS

Estado no fim desta sessão. Sessões anteriores resumidas no topo; **esta
sessão refatorou o código pra deixá-lo mais enxuto** (código morto +
duplicação removidos, sem mudar comportamento), depois de sessões
anteriores que deram ao rSystemOS uma taskbar com menu Iniciar e fizeram
o `cc` salvar o programa compilado num arquivo reutilizável.

## Estado: build limpo confirmado (WSL, sem warnings novos). Boot
## confirmado ao vivo no QEMU com screendump: janela do Program Manager,
## moldura, caixas de sistema e barra de tarefas renderizam exatamente
## como antes. Interação por clique (abrir apps via duplo-clique, menu
## Iniciar) **não foi possível confirmar nesta sessão** -- ver nota de
## ambiente abaixo, não é um problema introduzido pelo refactor. Não há
## tasks pendentes de código; só falta reconfirmar interação por clique
## numa sessão futura (ou com display gráfico de verdade).

### Esta sessão, parte 3 — refactor (enxugar código)

Pedido do usuário: tentar deixar o sistema mais leve sem quebrar nada.
Duas investigações (kernel-core e GUI/WM) confirmaram código morto real
(zero call sites, checado por grep no projeto inteiro) e duplicação
mecânica — não uma reestruturação, ~250-300 linhas removidas/consolidadas
num projeto de vários milhares.

**Código morto removido**: `src/lib/stdio.c` inteiro (`scan()`, sem
chamador — entrada de texto já passa por `lineedit.c`/`editbuf.c`);
`stdlib.c`'s `sleep()`/`shutdown()`; `string.c`'s `strchr()`; `pic.c`'s
`pic_set_mask()`; `isr.c`'s `irq_uninstall_handler()`; `event.c`'s
`event_count()`; `fs.c`'s `fs_set_attr()`; `wm.c`'s `wm_window_at_point()`
(o comentário de 8 linhas justificando drag&drop entre janelas nunca
teve um chamador de verdade); o campo `on_resize` de `app_st` (nunca
chamado por `wm.c` — as 6 apps só inicializavam `.on_resize = 0,` à toa).

**Duplicação consolidada**:
- `fm_name_cmp()` (`file_manager.c`) e `terminal_name_cmp()` (`terminal.c`)
  eram idênticas byte-a-byte — as duas viraram chamadas a `strncmp()`
  (que já existia em `string.c` mas não tinha chamador nenhum; ajustada
  pra `const char *` nos parâmetros).
- `fs.c`: guarda `id < 0 || id >= FS_MAX_ENTRIES || !g_table[id].used`
  repetida 9x virou `static int fs_valid(int id)`; 4 loops manuais de
  zero/cópia de byte viraram `memset`/`memcpy` (já existiam em `string.c`,
  sem uso).
- Maior consolidação: o bisel 3D "chiseled" (`draw_bevel`, já existia em
  `widget.c` mas era `static`) estava reimplementado à mão (4 `fill_rect`
  idênticos, só variando `pressed`) em **9 lugares** —
  `window.c` (moldura + caixas de título), `confirm.c`, `icon.c`,
  `menubar.c`, `taskbar.c` (botão + dropdown do menu Iniciar),
  `scrollbar.c` (thumb, cujo comentário já admitia a duplicação),
  `lineedit.c` e `calculator.c` (display). `draw_bevel()` foi exposto em
  `widget.h` e todos os 9 sites viraram uma chamada só.

**Verificação feita**: build limpo via WSL (o warning de `strchr`
builtin-mismatch sumiu junto com a função, como esperado; só restam os
warnings pré-existentes de `strcpy`). Boot ao vivo no QEMU confirmou
visualmente que a moldura da janela, as caixas de título/sistema e a
barra de tarefas (todos usando o `draw_bevel()` consolidado) renderizam
pixel-a-pixel como antes.

**Não confirmado nesta sessão**: interação por clique (abrir Calculadora/
Terminal/Arquivos via duplo-clique no ícone, abrir o menu Iniciar via
clique). `mouse_move` funcionou normalmente (cursor se moveu e ficou
visivelmente posicionado em cima dos ícones-alvo, confirmado por
screendump, inclusive logo após um boot limpo), mas `mouse_button`
(`printf 'mouse_button 1\nmouse_button 0\n' | nc -U ...`) não produziu
nenhum efeito visível em nenhuma tentativa (single click no botão
"Iniciar", double-click no ícone da Calculadora, com timings variados,
em duas instâncias diferentes do QEMU incluindo uma recém-bootada). O
código de despacho de clique (`wm_on_mouse_down`, `mouse_irq_handler`,
`pm_on_mouse_down`) não foi alterado nesta sessão e foi revisado
manualmente linha a linha — não há motivo funcional pra ele ter parado de
funcionar. Tudo indica um problema pontual do monitor QEMU nesta sessão
(injeção de clique via `mouse_button`, não de movimento), não uma
regressão do refactor. Recomenda-se reconfirmar num teste futuro (ou com
display gráfico de verdade em vez de monitor headless) antes de
considerar 100% fechado.

### Esta sessão, parte 2 — `cc` salva arquivo + comando `run`

`cc <arquivo.c>` antes compilava E executava na hora, sem nunca gravar o
resultado em disco. Agora `cc <arquivo.c> <saida>` só compila e salva; um
comando novo, `run <arquivo>`, executa um programa já compilado sem
recompilar.

**Formato do arquivo** (`CC_PROGRAM_HEADER_SIZE=8` em `cc.h`): 4 bytes de
assinatura `'R''K''X''C'` + 4 bytes de `code_len` little-endian + os
bytes de código de máquina crus. Nenhuma relocação é necessária: os
saltos de `if`/`while` já são deslocamentos relativos dentro do próprio
buffer (seguros sob cópia inteira), e a única referência de endereço
absoluto no código gerado é a chamada a `print_fn`, que aponta pra uma
função fixa do kernel (não pro buffer) — kernel sem ASLR, mesmo endereço
sempre.

**Novo em `cc.c`/`cc.h`**: `cc_save_program()` (empacota código+cabeçalho
num buffer do chamador) e `cc_load_program()` (valida assinatura/tamanho,
copia pro mesmo buffer estático `g_cc_code` que `cc_compile()` usa,
chama `cc_serialize()` de novo — mesmo hazard de self-modifying-code já
documentado, agora também no caminho de carregar do disco).

**Limite de tamanho aceito, não corrigido**: `CC_CODE_SIZE` (32KB) é bem
maior que `FS_MAX_FILE_SIZE` (4096 bytes, compartilhado com o buffer do
Editor de Texto) — decisão explícita de não mexer no filesystem;
`cc_save_program()` recusa (com mensagem clara em `cmd_cc`) programas cujo
código compilado não caiba num arquivo de até 4088 bytes.

**`terminal.c`**: `cmd_cc` perdeu a execução (agora só compila + salva via
`fs_create_file`/`fs_write_file`); `cmd_run` novo faz `fs_read_file` +
`cc_load_program()` + a mesma cauda de execução que `cmd_cc` costumava
ter (`g_cc_print_target`, chamar como `int(*)(void)`, "programa
retornou: N"). Registrado em `g_commands[]`.

**Testado ao vivo**: `.c` digitado no Editor de Texto (`int main() {
print(42); return 7; }`), salvo como `t.c`; `cc t.c t.bin` compila e
salva sem executar (`compilado: 37 bytes -> t.bin`); `run t.bin` executa
e imprime `42` / `programa retornou: 7`, repetido duas vezes seguidas com
resultado idêntico; `run t.c` (arquivo que não é um programa compilado)
rejeitado com erro de validação, nada executado, kernel não trava.

### Esta sessão, parte 1 — taskbar + menu Iniciar

### Sessões anteriores (resumo)
- Bootloader com driver IDE PIO, WM completo, apps Info/Calculator/
  Program Manager/Editor de Texto, sistema de arquivos próprio v2
  (`fs_dirent_t` 128 bytes, bump allocator sem reciclagem), Gerenciador
  de Arquivos estilo Windows 3.11.
- Terminal virou um interpretador de comandos estilo COMMAND.COM
  (`src/gui/apps/terminal.c`), com `fs_resolve_path` em `fs.c`/`fs.h` e
  comandos `dir`/`cd`/`md`/`rd`/`del`/`ren`/`copy`/`type`/`cls`/`date`/
  `time`/`ver`/`vol`.
- Ícones de verdade pros 5 apps do Program Manager (`src/include/
  icon_data.h`, paleta EGA/VGA de 16 cores).
- Compilador C básico (`cc`, novo comando do Terminal): lexa/parseia um
  subconjunto de C, gera bytes x86-32 reais num buffer `.bss` e executa
  chamando-o como ponteiro de função (kernel roda flat 4GB ring 0 sem
  paginação, então isso funciona sem processo/loader). Achado real de
  self-modifying-code hazard corrigido com `cpuid` como instrução
  serializadora (`cc_serialize()`) — ver histórico do arquivo (git) pra
  detalhe completo dessas sessões.

### Esta sessão — taskbar + menu Iniciar

**Problema**: Program Manager é criado uma única vez no boot
(`kernel.c`) e é a única forma de abrir Terminal/Calculadora/Info/
Arquivos/Editor. Fechá-lo (duplo-clique na caixinha de sistema) rodava o
mesmo `wm_close_window` genérico de qualquer janela — sem nenhum caso
especial — e não existia nenhuma UI persistente (sem taskbar, sem menu,
sem atalho) pra reabri-lo. Resultado: desktop vazio e travado.

**Solução escolhida com o usuário**: barra de tarefas fixa no rodapé com
botão "Iniciar" (menu com os 6 apps: Program Manager + os 5 que ele
lança) **e** unificada com a antiga bandeja de ícones minimizados — cada
janela aberta (minimizada ou não) agora é um botão na barra, clicar
foca/restaura.

**Novo widget `src/gui/taskbar.c`/`src/include/taskbar.h`** — só desenha
e faz hit-test (`taskbar_draw_bar`, `taskbar_draw_start_menu`,
`taskbar_hit_button`, `taskbar_hit_start_item`), no mesmo espírito
"burro" de `window.c`/`menubar.c`; não sabe nada de `app_st`/
`wm_window_st`. `TASKBAR_HEIGHT=24` substitui o antigo
`ICON_TRAY_HEIGHT=56` (a faixa reservada no rodapé, usada
simbolicamente por `wm_maximized_rect`/`wm_cascade`/`wm_tile`/clamp de
drag-resize — janelas ganharam ~32px a mais de área útil).

**Reuso em vez de duplicar**: a tabela de 5 launchers do Program Manager
(`g_launchers[]`, antes `static` em `program_manager.c`) e a função
`pm_launch()` foram exportadas via `program_manager.h` — o menu Iniciar
em `wm.c` usa exatamente a mesma tabela, sem risco de uma cópia
desatualizar em relação à outra. Também ganhou `program_manager_initial_rect()`
(o literal `{120,90,400,200}`), usado tanto pelo boot (`kernel.c`) quanto
pelo caso especial "reabrir Program Manager" do menu Iniciar (ele não
está em `g_launchers[]` porque não lista a si mesmo no próprio grid de
ícones).

**Removida por completo a bandeja de ícones de janelas minimizadas**
(`wm_icon_slot_rect`, `g_icon_slot_used[]`, `g_last_icon_win`/
`g_last_icon_tick`, campos `icon_slot`/`icon_rect` de `wm_window_st`) —
virou código morto assim que os botões da taskbar cobrem o mesmo papel
(e melhor, incluindo janelas não minimizadas).

**`wm_on_mouse_down` ganhou 2 novos passos** antes do loop de janelas:
botão Iniciar (toggle) e, se o menu estiver aberto, seleciona um item OU
fecha o menu sem consumir o clique (o mesmo clique continua e ainda
chega na janela por baixo, igual Windows de verdade) — depois os botões
de janela da taskbar (foca/restaura, sempre clique único).

## Testado ao vivo no QEMU (monitor + screendump, sem display gráfico)
Build via WSL limpo. Boot headless (`-display none -monitor unix:...`),
`mouse_move`/`mouse_button`/`screendump` pelo monitor, PPM convertido pra
PNG com `ffmpeg` e inspecionado visualmente:
1. Boot: barra aparece com "Iniciar" + botão "Program Manager" (janela
   default já aberta, foco ativo/azul).
2. Duplo-clique na caixinha de sistema do Program Manager fecha a
   janela — barra continua, botão dele some.
3. Clique em "Iniciar" abre o menu pra cima com as 6 opções.
4. Clique em "Program Manager" no menu reabre a janela na geometria
   padrão, focada, e o botão volta na barra.
5. Repetir "Iniciar" → "Program Manager" com ela já aberta: **não
   duplica**, só foca a existente (confirmado pela barra continuar com
   um único botão "Program Manager").

**Nota de calibração pra quem for repetir esse tipo de teste**: o
`mouse_move dx dy` do monitor QEMU parece ter ganho não-linear/
inconsistente em alguns casos (um delta pequeno logo após o boot rendeu
bem menos que 1:1 em pixels; deltas maiores em seguida bateram bem perto
de 1:1) — melhor prática é sempre tirar um `screendump` depois de cada
`mouse_move` e corrigir o delta restante por medição, em vez de confiar
num fator fixo calculado de antemão.

## Riscos/limitações aceitas (documentadas no código, não é bug escondido)
- Botão de janela na taskbar trunca o título sem reticências se não
  couber — simplicidade aceita.
- Clicar num botão de janela na taskbar sempre foca/restaura; não
  alterna pra minimizar se já estiver em foco (diferente do Windows de
  verdade).
- O menu Iniciar não tem destaque de hover ao mover o mouse (só reage ao
  clique) — `taskbar_draw_start_menu` recebe `hover_index` mas `wm.c`
  sempre passa `-1`; não estava no pedido original, fácil de adicionar
  depois se quiser (bastaria `wm_on_mouse_move` chamar algo como
  `taskbar_track_hover` e guardar o índice, igual `menubar_track_hover`
  já faz pra dropdowns de app).
- Continuam valendo as limitações de escopo já aceitas em sessões
  anteriores (sem isolamento de memória, sem funções/arrays/ponteiros/
  structs no `cc`, etc.).

## Nota sobre toolchain de build/teste (reconfirmado, sem mudança)
- **Este ambiente Windows não tem gcc/nasm/qemu/ffmpeg/python3 no PATH.**
  Build e testes rodam via `wsl.exe -d Ubuntu`
  (`cd /mnt/d/dev/rkdxOs/kernel_basic-main && make all`).
- Pra testar interações de mouse sem display gráfico: suba o QEMU
  destacado com `setsid ... </dev/null >log 2>&1 &` (nohup sozinho não
  basta — sem `setsid`/stdin redirecionado o processo recebe SIGHUP
  quando a chamada do `wsl.exe` que o originou termina) e um monitor
  `unix:/tmp/qemu-mon.sock,server,nowait`; comandos via
  `printf 'cmd\n' | nc -U -q1 /tmp/qemu-mon.sock`; `screendump` +
  `ffmpeg -i x.ppm x.png` pra poder olhar o resultado.

## Arquivos-chave pra retomar contexto rápido
- `src/gui/widget.h`/`widget.c` — `draw_bevel()` agora pública, usada por
  `window.c`, `confirm.c`, `icon.c`, `menubar.c`, `taskbar.c`,
  `scrollbar.c`, `lineedit.c` e `calculator.c` (todo bisel 3D "chiseled"
  raised/sunken do sistema).
- `src/kernel/fs.c` — `fs_valid(id)` privado, usado nos 8 pontos que
  antes repetiam a guarda de validade à mão; loops de zero/cópia agora
  são `memset`/`memcpy`.
- `src/lib/string.c`/`string.h` — `strncmp()` (agora com `const char *`)
  é usado de verdade por `file_manager.c`/`terminal.c` pra ordenar nomes;
  `strchr()`, `stdio.c` inteiro e `stdlib.c`'s `sleep`/`shutdown` foram
  removidos por não terem chamador.
- `src/include/app.h` — campo `on_resize` removido (nunca era chamado).
- `src/gui/taskbar.c`/`src/include/taskbar.h` — a barra inteira (desenho
  + hit-test), sem saber nada de `app_st`/janelas.
- `src/gui/wm.c` — `g_start_menu_open`, `g_start_menu_labels[]`,
  `wm_start_menu_select()`, os novos passos 1-3 de `wm_on_mouse_down`, e
  a montagem de `taskbar_button_t[]` no fim de `wm_composite`.
- `src/include/apps/program_manager.h`/`src/gui/apps/program_manager.c`
  — `pm_launcher_entry_t`, `g_launchers[]` e `pm_launch()` agora
  exportados (não mais `static`); `program_manager_initial_rect()` nova.
- `Makefile` — `dist/taskbar.o` adicionado (compile + link).
- `src/kernel/cc.c`/`src/include/cc.h` — `cc_save_program()`/
  `cc_load_program()` novos, `CC_PROGRAM_HEADER_SIZE`; reaproveitam
  `g_cc_code`/`cc_serialize()` internos, nada exposto além disso.
- `src/gui/apps/terminal.c` — `cmd_cc` só compila+salva agora (exige 2
  argumentos); `cmd_run` novo faz o load+execute que `cmd_cc` fazia antes;
  `g_cc_save_buf`/`g_cc_run_buf` novos (buffers estáticos, mesmo padrão de
  `g_cc_source_buf`).
- `src/kernel/fs.c`/`fs.h`, `src/include/icon_data.h` — ver sessões
  anteriores, inalterados nesta rodada. `FS_MAX_FILE_SIZE` (4096)
  continua sendo o teto de tamanho de qualquer arquivo, incluindo
  programas compilados salvos.
