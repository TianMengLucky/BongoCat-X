<div align="center">

2.2.2: páginas e traduções gerenciadas pelos plugins, menor uso de memória na renderização e no carregamento de imagens e links de compra na loja.
  <a href="https://bongocat.pet" target="_blank">
    <img src="../resources/assets/bongocat.png" alt="BongoCat" width="249">
  </a>
  <h1><a href="https://bongocat.pet" target="_blank">BongoCat</a></h1>
</div>

<p align="center">💘 C/C++ × Rust × SDL3, misture tudo e bata à vontade! Bong~ Bongo Cat!!!</p>
<p align="center">
  <a href="README.en-US.md">English</a> • <a href="../README.md">简体中文</a> • <a href="README.zh-Hant.md">繁體中文</a> • <a href="README.fr-FR.md">Français</a> • <a href="README.de-DE.md">Deutsch</a> • <a href="README.ko-KR.md">한국어</a> • <strong>Português</strong> • <a href="README.ru-RU.md">Русский</a> • <a href="README.es-ES.md">Español</a> • <a href="README.id-ID.md">Bahasa Indonesia</a>
</p>
> [!NOTE]
> Este repositório (**BongoCat-X**) é um fork de [vladelaina/BongoCat](https://github.com/vladelaina/BongoCat). Não tem afiliação com a Live2D Inc. nem inclui Cubism SDK ou Core. As licenças da Live2D devem ser respeitadas.

<p align="center">
  <a href="../LICENSE"><img src="https://img.shields.io/badge/AGPL--3.0-1fa669?style=flat&logo=gnu&logoColor=white"></a>
  <a href="https://github.com/TianMengLucky/BongoCat-X"><img src="https://img.shields.io/badge/C-A8B9CC?style=flat&logo=c&logoColor=white"></a>
</p>

<div align="center"><video src="https://github.com/user-attachments/assets/75719230-9e49-4124-ae5a-8e35592c5d49" autoplay loop style="border-radius: 8px; max-width: 800px;"></video></div>

> [!TIP]
> O modelo usado na demonstração é de [宇痕冫](https://space.bilibili.com/348616056).
>
> 🎁 Procurando modelos **gratuitos**? Trabalhamos com criadores de modelos talentosos para trazer uma grande variedade de modelos gratuitos, enquanto exploramos continuamente experiências de desktop ainda mais divertidas! Visite nosso site oficial: [bongocat.pet](https://bongocat.pet/models)

<p align="center">
  <a href="https://bongocat.pet/models">
    <img height="1080" src="https://github.com/user-attachments/assets/dedd83ca-742a-4f8c-a64a-69e659fca564" />
  </a>
</p>

## 💬 Comunidade QQ

Escaneie o código QR com o QQ ou procure o grupo **211957388** para participar da comunidade **BongoCat-X**.

<p align="center"><img src="../resources/assets/qq-group.jpg" alt="BongoCat-X QQ · 211957388" width="320"></p>

## SDK Live2D e cores

A importação e o download do SDK ficam em Configurações → Plugins; a página Modelos gerencia modelos. No Windows, ative o plugin Live2D, escolha Baixar SDK Live2D e leia e aceite os dois contratos oficiais para instalar o Core compatível. O progresso mostra o volume recebido, o total e a porcentagem; falhas podem ser tentadas novamente no diálogo. Nas outras plataformas, importe um Core compatível manualmente. As cores da aparência e do fundo sólido compartilham uma paleta aberta ao clicar na amostra de cor.

## 📥 Download

- GitHub Releases ([v2.2.2](https://github.com/TianMengLucky/BongoCat-X/releases/tag/v2.2.2))

  Baixe a versão mais recente nas [GitHub Releases](https://github.com/TianMengLucky/BongoCat-X/releases/latest).

  > [!IMPORTANT]
  > As versões oficiais são **builds runtime-Core**: o suporte à renderização Live2D está embutido, mas a biblioteca de tempo de execução Live2D Cubism Core **não é incluída** (licença proprietária, nunca distribuída com os pacotes). **Não** são builds de diagnóstico sem Live2D. Quando nenhum Core é encontrado, o aplicativo volta para o backend de diagnóstico e mostra um aviso na janela de configurações.

  **Ativar a renderização Live2D (escolha uma opção):**

  1. **Importar pelo aplicativo (recomendado)**: abra *Configurações → Plugins*, clique em *Importar Live2D Core* e selecione um arquivo `Live2DCubismCore.dll` ou o zip oficial do SDK do Cubism. Tem efeito imediato, sem reiniciar.
  2. **Soltar na pasta live2d**: baixe o **Cubism SDK for Native** na [página oficial de download](https://www.live2d.com/en/sdk/download/native/) (é preciso aceitar a licença da Live2D) e coloque o zip ou o `Live2DCubismCore.dll` extraído na pasta `live2d` ao lado do aplicativo ou no diretório de dados; após reiniciar, ele é reconhecido automaticamente.

  Para os passos completos de importação do SDK ao compilar do código-fonte, consulte a seção «Live2D / Cubism SDK» abaixo.

## 🛠️ Compilar a partir do código-fonte

BongoCat requer CMake 3.24 ou mais recente, compiladores C11 e C++17, arquivos de desenvolvimento OpenGL e a toolchain Rust (cargo, disponível via rustup). Corrosion compila os módulos Rust. A configuração baixa SDL3, stb, miniaudio, Nuklear e Corrosion por padrão; a primeira compilação precisa de acesso à rede.

Execute os comandos abaixo na raiz do projeto (o diretório que contém `CMakeLists.txt`).

### 📋 Pré-requisitos por plataforma

- **Windows:** Visual Studio 2022 (com a carga de trabalho ‘Desenvolvimento para desktop com C++’) e CMake. Use o gerador MSVC; o MinGW pode compilar o backend de diagnóstico, mas não suporta o SDK do Cubism.
- **macOS:** Xcode Command Line Tools, CMake e Ninja. Se a arquitetura de destino for diferente da padrão do host, especifique-a por meio de `CMAKE_OSX_ARCHITECTURES`.
- **Linux (Debian/Ubuntu):** GCC ou Clang, Ninja e os cabeçalhos de OpenGL/X11:

  ```bash
  sudo apt-get update
  sudo apt-get install -y build-essential cmake ninja-build \
    libgl1-mesa-dev libx11-dev libxi-dev libxfixes-dev libxcb-screensaver0-dev libfontconfig1-dev fonts-noto-cjk
  ```

### 🔧 Configuração e compilação

No Linux e no macOS, use um gerador de configuração única como o Ninja:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_FETCH_DEPS=ON
cmake --build build --parallel
```

No Windows, execute os comandos no Prompt de Comando do Desenvolvedor do Visual Studio 2022 (ou em qualquer shell em que o MSVC esteja disponível):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DBONGO_CAT_FETCH_DEPS=ON
cmake --build build --config Release --parallel
```

O executável fica em `build/BongoCat` no Linux, em `build/BongoCat.app/Contents/MacOS/BongoCat` no macOS e em `build/Release/BongoCat.exe` para compilações do Visual Studio no Windows.

### 🧪 Testes

Os alvos do CTest estão habilitados por padrão. Execute o seguinte após compilar:

```bash
ctest --test-dir build --output-on-failure
```

Para um gerador de múltiplas configurações, como o Visual Studio, especifique explicitamente a configuração de compilação:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

### 🎭 Live2D / SDK do Cubism (Opcional — compila mesmo sem ele)

O SDK proprietário do Cubism continua opcional. Sem ele, o aplicativo e o plugin Inox2D são compilados. Um plugin Live2D compatível e o Cubism Core fornecido pelo usuário ativam o Live2D sem recompilar o aplicativo. O SDK gera o plugin C++ separado.

1. Abra a [página de download do SDK do Cubism](https://www.live2d.com/en/sdk/download/native/), aceite o Live2D Proprietary Software License Agreement e baixe o **Cubism SDK for Native** (os releases são compilados e testados com o SDK `5-r.5`).
2. Extraia o arquivo. Se a pasta extraída se chamar `CubismSdkForNative-5-r.5`, renomeie-a para `CubismSdkForNative` e coloque-a em `vendor/` de modo que a árvore contenha `Core/` e `Framework/`.
3. Arquivos mais recentes do SDK não incluem mais o GLEW. Baixe o [GLEW 2.2.0](https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.zip) e extraia-o em `vendor/CubismSdkForNative/Samples/OpenGL/thirdParty/glew` (o diretório que contém diretamente `include/GL/glew.h` e `src/glew.c`).

Você também pode manter o SDK em qualquer lugar e informar o caminho explicitamente:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_CUBISM_SDK=/path/to/CubismSdkForNative
```

O SDK deve incluir a biblioteca Core, o código-fonte do Framework e o diretório de terceiros do OpenGL GLEW na estrutura esperada por `cmake/Cubism.cmake`. As compilações de Cubism no Windows exigem o Visual Studio 2022. Com o SDK no lugar, configure como de costume para obter uma compilação com renderização do Live2D; `BONGO_CAT_REQUIRE_CUBISM=ON` agora apenas faz a configuração falhar rapidamente com instruções de importação quando o SDK está ausente (a CI de releases usa essa opção). Deixe o padrão `OFF` quando não precisar disso.

### ⚙️ Opções do CMake

| Opção | Valor padrão | Descrição |
| --- | --- | --- |
| `BONGO_CAT_FETCH_DEPS` | `ON` | Baixa dependências de terceiros em versões fixadas via `FetchContent` do CMake (incluindo Corrosion e as dependências do crate Rust). Defina como `OFF` apenas se SDL3, stb, miniaudio, Nuklear e Corrosion já estiverem disponíveis para o CMake. |
| `BONGO_CAT_CUBISM_SDK` | `vendor/CubismSdkForNative` | Caminho para o Cubism SDK for Native. |
| `BONGO_CAT_REQUIRE_CUBISM` | `OFF` | Define se a ausência do SDK faz a configuração falhar. Padrão `OFF`: sem o SDK, compila o backend de diagnóstico sem renderização do Live2D; defina `ON` para exigir o SDK (a CI de releases usa essa opção). |
| `BONGO_CAT_WARNINGS_AS_ERRORS` | `OFF` | Trata avisos do compilador nativo como erros. |

Para uma compilação offline, defina `BONGO_CAT_FETCH_DEPS=OFF` e forneça as configurações de pacote do CMake para SDL3 (incluindo `SDL3-static`) ; se stb, Nuklear e miniaudio não puderem ser detectados automaticamente, informe também seus diretórios de inclusão:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBONGO_CAT_FETCH_DEPS=OFF \
  -DBONGO_CAT_STB_INCLUDE_DIR=/path/to/stb \
  -DBONGO_CAT_NUKLEAR_INCLUDE_DIR=/path/to/nuklear \
  -DBONGO_CAT_MINIAUDIO_INCLUDE_DIR=/path/to/miniaudio
```

## 📌 Status do projeto

![Commit activity](https://img.shields.io/github/commit-activity/m/TianMengLucky/BongoCat-X?style=flat)
![Contributors](https://img.shields.io/github/contributors/TianMengLucky/BongoCat-X?style=flat)
![Stars](https://img.shields.io/github/stars/TianMengLucky/BongoCat-X?style=flat)
![Issues](https://img.shields.io/github/issues/TianMengLucky/BongoCat-X?style=flat)
![Release](https://img.shields.io/github/v/release/TianMengLucky/BongoCat-X?style=flat)

![Alt](https://repobeats.axiom.co/api/embed/74334755a589dea40c5d31f8d2bcdc6c2bd39d87.svg "Repobeats analytics image")

## 📜 Licença

O código-fonte e o runtime nativo do BongoCat estão licenciados sob [AGPL-3.0-only](../LICENSE).

O modo de modelo integrado padrão (`standard`) permanece sob a licença MIT. Os recursos de modelo incluídos em `resources/assets/models/standard`, `keyboard` e `gamepad` são cobertos pela [declaração de licença MIT](../LICENSE-MIT) separada. Essa licença MIT se aplica apenas aos recursos dos modelos e às artes que os acompanham; ela não altera a licença do código-fonte nem do runtime nativo do BongoCat.

## 🧭 Arquitetura técnica

O programa C11 usa SDL3 para eventos, atualização dos modelos e apresentação na thread principal, e Nuklear para configurações. `src/rust/bongo-safe` gerencia a análise de dados não confiáveis, filas e métricas de entrada e o estado compartilhado das threads de plugins. A ponte Live2D, o backend de recursos Vulkan e os adaptadores Windows COM/WIC usam C++17.

Os receptores de plataforma enviam eventos de teclas e botões a uma fila Rust limitada, agrupam os movimentos do ponteiro separadamente e despertam a thread principal para atualizar parâmetros e interface. Uma ABI C versionada carrega plugins Live2D/Inox2D com OpenGL, Vulkan ou Metal conforme a plataforma.

Veja [Plugins de renderização](model-plugins.md) para compatibilidade e compilação e [Renderização nativa](live2d-vulkan-metal.md) para limites e escopo de validação.

## ❓ Perguntas frequentes

### 🔒 O BongoCat registra minhas entradas de teclado ou mouse?

BongoCat processa entradas localmente para animações e atalhos e não envia dados de entrada ou uso. Quando ativado, o plugin de estatísticas pode salvar totais, tendências diárias/horárias e tempo dos aplicativos em primeiro plano localmente. Não salva texto digitado, ordem de teclas, títulos de janelas, URLs ou coordenadas. Na página Estatísticas é possível desativar a coleta, redefinir ou exportar o histórico. Veja [Estatísticas](statistics-plugin.md). A verificação de atualizações solicita metadados públicos de versões.

### 🖼️ Qual é o progresso do suporte a Vulkan / Metal?

Configurações → Aplicativo → Backend de renderização permite trocar imediatamente: OpenGL ↔ Vulkan no Windows / Linux x64 e OpenGL ↔ Metal no macOS. Sistema usa OpenGL por padrão. O envio de texturas nativas, desenho Cubism, leitura da GPU e recarga estão conectados; o modelo e movimentos/expressões selecionados são preservados. Falhas de inicialização ou recarga restauram OpenGL. Vulkan/Metal são experimentais: sobreposições de teclas/efeitos/ponteiro agora suportam todos os motores. Live2D suporta resolução dinâmica em OpenGL, Vulkan e Metal, com decodificação/redimensionamento em segundo plano e envio nativo à GPU síncrono. Desativar recarrega as texturas conforme a qualidade escolhida. Os backends nativos armazenam o fundo de mesa em cache, o incluem no quadro ajustado e arredondam os quatro cantos na GPU sem outra textura de tela inteira. As camadas usam renderização por software do SDL, ficam em cache até a entrada mudar e são compostas no quadro nativo concluído antes do envio à GPU; o desempenho ainda não foi medido.

A otimização de renderização aparece em Vulkan / Metal e fica desativada por padrão. Vulkan oferece três opções; Metal apenas grandes alocações. Inox2D Metal agrupa os vértices de cada quadro; Live2D Metal mostra a opção como indisponível. As três subopções são salvas separadamente; alterações recarregam o modelo. Inox2D Vulkan oferece gravação paralela e blocos grandes. Live2D Vulkan oferece VMA e gravação paralela com mistura comum, sem nós fora da tela ou máscaras de alta precisão. Live2D exige 128 Drawables para buffers secundários e 128 snapshots desenháveis para usar até quatro threads; outros modelos mantêm a rota serial do SDK. Live2D Vulkan Bindless usa uma tabela de até 64 texturas de atlas com mistura comum e sem nós fora da tela; retorna à rota anterior se os recursos ou a capacidade do dispositivo forem insuficientes. Inox2D Bindless não está implementado. Pools maiores podem usar mais memória; os ganhos de desempenho ainda não foram medidos.

Compilações com SDK usam `BONGO_CAT_CUBISM_VULKAN=ON` no Windows / Linux x64 (`glslangValidator` ou `glslang`, pacote Linux `glslang-tools`; Vulkan 1.3 na execução) e `BONGO_CAT_CUBISM_METAL=ON` no macOS (ferramentas Metal do Xcode). Use `OFF` para desativar a extensão. Fontes Metal e `.metallib` entram apenas no macOS; Vulkan e `.spv` apenas no Windows / Linux x64, incluindo variantes de mistura. Compilações de diagnóstico sem SDK omitem esses recursos Live2D. GitHub Actions prepara as ferramentas e verifica recursos por plataforma. Verificações estáticas foram executadas; compilação e GPU ainda não foram validadas. Veja [integração e validação](live2d-vulkan-metal.md).

## 🙏 Agradecimentos especiais
> [!TIP]
> Cada passo do BongoCat é movido pelo espírito do código aberto. Agradecemos sinceramente a todos os colaboradores da comunidade por suas contribuições altruístas (listadas abaixo em ordem cronológica de contribuição). É o apoio de vocês que torna a companhia no desktop mais livre e genuína.❤️‍🔥


<a href="https://github.com/TianMengLucky/BongoCat-X/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=TianMengLucky/BongoCat-X" />
</a>


---

<div align="center">
Copyright © 2026 - **BongoCat**<br>
By vladelaina<br>
Made with ❤️ & ⌨️
</div>

### Layout das opções

As preferências usam cartões de grupos recolhíveis, com Janela, Modelo, Aplicativo e Aparência expandidos por padrão. Ative Layout compacto em Aparência para reduzir os espaços mantendo as descrições. O ajuste é salvo automaticamente.

### Plugin de estatísticas de entrada

O plugin Rust é compilado com o aplicativo por padrão. Ative, desative ou importe na página de plugins. A página de estatísticas mostra totais de teclado e mouse, gráficos diários e por hora, armazenamento local e exportação CSV. Veja [Estatísticas](statistics-plugin.md). O tempo dos aplicativos ativos inclui ranking, totais por hora e controle separado. Coleta, armazenamento e exportação usam uma thread independente. O plugin adiciona a entrada de estatísticas ao ser ativado e a remove ao ser desativado ou desinstalado.

Personalize os nomes das unidades e os fatores de movimento e rolagem em Estatísticas, com opções para salvar e restaurar. A aparência usa uma paleta compartilhada para as cores principal, secundária, do texto e do fundo da página.

### Plugins de renderização dinâmicos

Preferências → Plugins permite instalar, ativar e remover renderizadores locais. Mude para outro renderizador antes de alterar um plugin ativo. Live2D usa um plugin C++; Inochi2D usa o plugin Rust Inox2D com OpenGL, Vulkan (Windows/Linux) e Metal (macOS), sem wgpu. A importação identifica primeiro o conteúdo do modelo (`.model3.json`, `.inp`, `.inx`). Toda a leitura e escrita de JSON é feita em Rust.

As versões portáteis para Windows são arquivos ZIP. Mantenha a pasta extraída `plugins` ao lado do executável. Veja [Plugins de renderização](model-plugins.md) para empacotamento, dependências offline, ABI, mapeamento de parâmetros e limites de compatibilidade.

Cada push para `dev` que altera código, recursos ou configuração de compilação publica uma versão de pré-lançamento no GitHub. Alterações apenas na documentação são ignoradas; não há execução agendada. Todas as plataformas usam o commit enviado. A versão estável mais recente não é substituída.

As prévias noturnas reutilizam a [página nightly](https://github.com/TianMengLucky/BongoCat-X/releases/tag/nightly), atualizam a tag e substituem arquivos com nomes fixos. Plataformas com falha mantêm seus arquivos anteriores.

### Linux Wayland / evdev

A entrada evdev experimental no Linux Wayland é ativada com `BONGOCAT_ENABLE_EVDEV=1`. Primeiro, os dispositivos são abertos somente para leitura como usuário normal. Se o acesso for negado, uma instalação protegida solicita autenticação por `/usr/bin/sudo`, abre os dispositivos e reinicia após abandonar imediatamente os privilégios, antes de inicializar SDL, configurações e modelos. O programa, sudo e diretórios superiores devem pertencer a root e não permitir escrita pelo grupo ou outros usuários. Instalações de desenvolvimento e portáteis precisam de permissões administradas separadamente. O caminho confiável do sudo é configurável por `BONGO_CAT_SUDO_EXECUTABLE`. Falhas interrompem a inicialização; permissões permanentes não são alteradas. Apenas dispositivos presentes ao iniciar são monitorados; novas conexões exigem reiniciar. Não execute diretamente como root nem entre no grupo `input`. A entrada pode incluir senhas e não pausa ao bloquear a tela, trocar de sessão ou ocultar o pet.

[SECURITY.md](../SECURITY.md#linux-input)

Preferências → Preferências → Janela permite escolher uma imagem de fundo personalizada: PNG, JPEG, WebP, BMP ou GIF (primeiro quadro). A imagem preenche a janela centralizada, mantendo a proporção, e não pode ser usada com o fundo de cor sólida. A cópia fica no diretório de dados do aplicativo, mesmo que o original seja movido. Limites: 64 MB e 16 megapixels. Compatível com OpenGL, Vulkan e Metal.

Live2D reutiliza automaticamente buffers de geometria e estados de renderização repetidos. Uploads OpenGL, lotes e cache de pipelines Vulkan, buffers compartilhados Metal e máscaras nativas adaptativas não exigem outro interruptor. O desempenho e o comportamento por plataforma ainda precisam ser medidos.

Vulkan gerencia a memória automaticamente: Live2D usa blocos VMA de 1 MiB por padrão ou de 16 MiB com alocações grandes ativadas. A apresentação em camadas do Windows mantém um destino de cor. Janelas ocultas ou minimizadas liberam caches de leitura após a conclusão da GPU e os recriam quando necessário. Qualidade e resolução dinâmica continuam sendo configurações independentes.

Configurações → Pet → Faixa do mouse: horizontal/vertical 25–400%, desativada por padrão; 100% mantém o original. As patas Mver desenhadas vão mais longe; Live2D mantém seus limites. Olhar e janela não mudam

### Várias teclas e caracteres legíveis no modo espelho

Mver (OpenGL/Vulkan/Metal) recorta separadamente a mão, a borda e os caracteres, unindo as teclas pressionadas com + em um único balão. A borda e a ponta espelham com o modelo; os caracteres continuam legíveis, também no modo de uma tecla. Configure as três áreas, o espaçamento ao redor de + e o limite por lado em `hand-layout.json` junto às imagens das mãos e reinicie após editar. Veja o [guia e o exemplo](hand-layout.md).

A janela de ações verifica a referência `Sound` e se o arquivo existe, mostrando um controle individual junto aos movimentos com áudio. Ajuste de 0 a 100% arrastando ou digitando; um clique duplo restaura 100% e 0% silencia. Os valores são salvos por modelo e movimento e valem para a janela, atalhos, movimentos aleatórios e menu de contexto, inclusive durante a reprodução. Outros movimentos que usam o mesmo arquivo mantêm seu próprio volume.

### Plugin de itens

O plugin independente de itens vem incluído por padrão. Ative-o em Preferências → Plugins para mostrar Itens. Suporta importação de várias imagens e arrastar e soltar, posição, tamanho, rotação, opacidade, camadas à frente/atrás, bloqueio, duplicação e cenas com OpenGL, Vulkan e Metal. Seguir a área do modelo não prende o item à malha. GIF usa o primeiro quadro; até 16 itens no pet principal. Veja [Plugin de itens](items-plugin.md). `BONGO_CAT_BUILD_ITEMS=OFF` omite a biblioteca.

Preferência de memória de renderização: Priorizar memória de vídeo / Priorizar memória do sistema. Somente Live2D Vulkan. Recarrega a renderização. A memória de vídeo favorece a velocidade; a do sistema reduz a pressão sobre a VRAM, mas pode ser mais lenta. Recursos incompatíveis usam uma alternativa automática. Outros mecanismos alocam automaticamente; GPUs integradas compartilham memória.

### Sons do teclado

Ative Efeitos sonoros em Plugins. Clique em uma tecla para escolher áudio, passe o mouse para ver o nome e pressione a tecla física para reproduzir. As atribuições são salvas automaticamente. Sons e itens têm threads separadas. WAV/MP3/Ogg Vorbis/FLAC no pet principal. Veja [Efeitos sonoros](sounds-plugin.md); `BONGO_CAT_BUILD_SOUNDS=OFF` omite a biblioteca.

A loja Rust exibe prévias, links de autores e botões de download/desinstalação. O SHA-256 é verificado antes da instalação; um estado de espera aparece enquanto o catálogo não é publicado. [Plugin da loja](store-plugin.md).

A verificação automática de atualizações fica ativada por padrão em Preferências → Aplicativo e ocorre uma vez em segundo plano a cada início. O aviso de uma nova versão permanece até abrir as configurações, com o botão «Atualizar agora» no canto superior direito. Todos os avisos das configurações podem ser fechados manualmente.

A página de modelos reúne os modos do mesmo pacote em um cartão com seletor, incluindo os modos padrão, teclado e controle dos modelos padrão. Arraste vários ZIPs para importar em segundo plano. Fechar as configurações não interrompe a importação. Ao sair, as próximas importações são interrompidas e a fonte atual termina antes de liberar as threads e os recursos temporários.

Os atalhos ficam em Preferências, com Configurações / Atalhos no topo. A loja fica em Modelo, com Modelos locais / Loja. Suas duas entradas independentes saem da barra lateral; cada subpágina mantém sua posição de rolagem.

Ao ativar Live2D sem Cubism Core, os dois contratos oficiais são exibidos. No Windows, Aceitar e baixar obtém o SDK Native R5 via HTTPS oficial, extrai apenas o Core dinâmico compatível e ativa a renderização local. Cancelar e continuar mantém o plugin ativo sem renderização. É possível tentar novamente ou importar manualmente. O aplicativo e os pacotes não incluem SDK nem Core.

Windows com GPU AMD oferece apresentação DirectComposition experimental, desativada por padrão. A composição alfa DXGI aplica-se imediatamente, limita eventos expose a 50 ms e desativa VSync; falhas restauram a apresentação anterior. A leitura de pixels permanece; Independent Flip em jogos não é garantido.

A ABI dos plugins de renderização agora é v2: atualize ou recompile juntos o aplicativo e os plugins Live2D/Inox2D; plugins antigos são incompatíveis. Live2D Vulkan oferece recursos fora da tela sob demanda, redimensionamento PNG em fluxo com Rust e cache BC7 com alternativa RGBA8; consulte os [detalhes de renderização](live2d-vulkan-metal.md) para os limites de plataforma.
