# Relatório Técnico: Tangram Infinito
## Trabalho Prático G1 - Jogo Pedagógico

---

## 1. Introdução

O **Tangram Infinito** é um jogo pedagógico interativo desenvolvido em C++ com OpenGL (FreeGLUT). O objetivo é desafiar o usuário a posicionar, rotacionar e redimensionar peças geométricas para encaixá-las em um padrão alvo, promovendo compreensão de transformações geométricas.

---

## 2. Elementos Obrigatórios Implementados

### 2.1 Cinco Objetos Geométricos

O jogo contém exatamente 5 peças do Tangram:

| # | Nome | Forma | Vértices (SRO) | Cor |
|---|------|-------|----------------|-----|
| 1 | Triângulo Grande A | Triângulo retângulo isósceles | (0,0), (1.9,0), (0,1.9) | Vermelho |
| 2 | Triângulo Grande B | Triângulo retângulo isósceles | (0,0), (-1.9,0), (0,1.9) | Laranja |
| 3 | Triângulo Médio | Triângulo retângulo isósceles | (0,0), (1.35,0), (0,1.35) | Verde |
| 4 | Quadrado | Quadrilátero regular | (-0.62,-0.62) a (0.62,0.62) | Ciano |
| 5 | Paralelogramo | Quadrilátero não-retângulo | (-0.82,-0.54) a (0.90,0.54) | Rosa |

### 2.2 Transformações Geométricas

Todas as peças suportam as três transformações fundamentais, controladas pelo usuário:

1. **Translação**: Movimento livre no plano via mouse
2. **Rotação**: Rotações de 15° incrementais via tecla R
3. **Escala**: Mudança de tamanho via teclas +/-

### 2.3 Interação Homem-Máquina

- **Mouse**: Arrastar peças para reposicionar
- **Teclado**:
  - `R`: Rotacionar peça selecionada (+15°)
  - `+` ou `=`: Aumentar escala
  - `-`: Diminuir escala
  - `N`: Reiniciar na fase 1
  - `+15s`: Botão para adicionar tempo (clicável)

---

## 3. Sistemas de Referência

### 3.1 SRU (Sistema de Referência do Universo)

O espaço do jogo é definido em coordenadas ortogonais 2D:

```
Limites SRU:
X: [-10, 10]  (largura: 20 unidades)
Y: [-5.625, 5.625]  (altura: 11.25 unidades)
Proporção: 16:9 (1280×720 pixels)

Projeção: glOrtho(-10.0, 10.0, -5.625, 5.625, -1.0, 1.0)
```

**Divisão do espaço**:
- **Esquerda [-10, 0]**: Área de montagem (peças iniciam aqui)
- **Direita [0, 10]**: Área alvo com silhueta a montar

### 3.2 SRO (Sistema de Referência do Objeto)

Cada peça é definida localmente por seus vértices, com origem no seu centroide aproximado.

**Transformação SRO → SRU**:

```
Pseudo-código:
Para cada vértice V no SRO:
  1. Aplicar escala: V' = V × scale
  2. Aplicar rotação: V'' = Rot(angle) × V'
  3. Aplicar translação: V_final = V'' + position
  4. Resultado em SRU
```

**Fórmula de rotação** (ângulo em graus):

```
r = ângulo em radianos = ângulo × π / 180
cos_r = cos(r), sin_r = sin(r)

V_rotado.x = V.x × cos_r - V.y × sin_r
V_rotado.y = V.x × sin_r + V.y × cos_r

V_final = V_rotado + position
```

**Exemplo - Triângulo Grande A**:
- SRO vértices: (0,0), (1.9,0), (0,1.9)
- Se posição=(3.2, -1.55), rotação=180°, escala=0.58:
  - Vértice (1.9, 0) → (-1.102, 0) após rotação → (2.098, -1.55) final

---

## 4. Modelagem e Comportamento

### 4.1 Propriedades de Cada Peça (Piece)

```cpp
struct Piece {
    // SRO: geometria em coordenadas locais
    std::vector<Vec2> baseShape;
    
    // Transformações no SRU
    Vec2 position;          // Translação
    float rotation;         // Rotação em graus [0°, 360°)
    float scale;            // Escala uniforme [0.5, 1.7]
    float color[3];         // RGB [0, 1]
    
    // Estado alvo (para alinhamento automático)
    Vec2 targetPosition;
    float targetRotation;
    float targetScale;
    bool snapped;           // Peça está encaixada?
}
```

### 4.2 Fases do Jogo

- **Fase N**: 3 padrões diferentes que se repetem
- **Tempo inicial**: 62 - (N-1)×1.6 segundos (diminui com dificuldade)
- **Alinhamento automático**: Ao aproximar de ±(10°, 0.6u, 0.12 escala), peça encaixa automaticamente
- **Progressão**: Ao encaixar 5 peças, avança para próxima fase

---

## 5. Algoritmos Principais

### 5.1 Detecção de Clique em Peça

**Algoritmo de raycasting (point-in-polygon)**:

```
Para ponto P e polígono com vértices V[i]:
  inside = false
  Para cada aresta V[i]→V[i+1]:
    Se P.y está entre V[i].y e V[i+1].y:
      Calcular intersecção do raio horizontal com aresta
      Se P.x está à esquerda da intersecção:
        inside = !inside
  Retornar inside
```

### 5.2 Conversão Tela → Mundo

```cpp
x_mundo = (x_pixels / width) × 20 - 10
y_mundo = ((height - y_pixels) / height) × 11.25 - 5.625
```

### 5.3 Alinhamento Automático (Snap)

Peça encaixa quando satisfaz simultaneamente:

```
distância ≤ 0.6 unidades
|rotação_atual - rotação_alvo| ≤ 10°
|escala_atual - escala_alvo| ≤ 0.12
```

---

## 6. Controles do Programa

### 6.1 Controles Principais

| Ação | Entrada | Efeito |
|------|---------|--------|
| Selecionar peça | Clique esquerdo | Peça segue o mouse |
| Rotacionar | Tecla `R` | +15° (cumulativo) |
| Aumentar tamanho | Tecla `+` | +8% (até 1.7×) |
| Diminuir tamanho | Tecla `-` | -8% (até 0.5×) |
| Bônus de tempo | Clique no botão +15s | +15 segundos (1× por fase) |
| Reiniciar | Tecla `N` | Volta para fase 1 |

### 6.2 Estados do Jogo

- **Em andamento**: Manipular peças
- **Peça encaixada**: Fica mais escura, não pode ser movida
- **Fase completa**: Mensagem "Fase completa! Próxima fase..." por 0.9s
- **Game Over**: "Tempo esgotado! Você morreu." → Pressione N

### 6.3 Interface Visual

- **Painel esquerdo**: Área de montagem com peças iniciais
- **Painel direito**: Silhueta alvo em escala de cinza
- **HUD inferior**: Fase, tempo restante, dicas de controle
- **Botão +15s**: Azul (disponível) → Cinza (usado)

---

## 7. Implementação Técnica

### 7.1 Estrutura do Código

```
jogo.c++ (606 linhas)
├── Cabeçalho: identificação e descrição
├── Estruturas: Vec2, Piece, TargetTemplate, PiecePose
├── Classe PuzzleGame: gerenciador principal
│   ├── initialize(): setup inicial
│   ├── render(): desenho por frame
│   ├── onKeyboard/onMouse/onMotion: input
│   ├── onTimer(): lógica do jogo
│   ├── transformações geométricas
│   ├── renderização (OpenGL)
│   └── funções de callback estáticas
└── main(): inicialização GLUT
```

### 7.2 Fluxo de Renderização

```
render()
├── Limpar buffers (cor de fundo cinza-claro)
├── Configurar projeção ortho
├── drawBoard(): divisor central + rótulos
├── drawTargets(): silhuetas das peças alvo
├── drawPieces(): peças interativas
├── drawHud(): interface (fase, tempo, controles)
└── drawOverlay(): mensagens (vitória/derrota)
```

### 7.3 Performance

- **Taxa de atualização**: 60 FPS (16ms por frame via timer)
- **Cálculos por frame**:
  - 5 peças × 2 transformações (mundo + alvo) = 10 transformações
  - Detecção de colisão só ao clicar
  - Renderização: ~50 vértices visíveis

---

## 8. Características Pedagógicas

1. **Aprendizado de transformações**: Visualizar rotação, escala, translação interativamente
2. **Reconhecimento de padrões**: Observar silhueta e replicá-la
3. **Resolução de problemas**: Lógica espacial progressiva
4. **Feedback visual**: Encaixe automático ao atingir posição correta
5. **Dificuldade progressiva**: Tempo diminui, padrões variam

---

## 9. Conclusão

O **Tangram Infinito** implementa todos os requisitos obrigatórios:
- ✓ 5 objetos geométricos com SRO bem definido
- ✓ Transformações SRO→SRU (translação, rotação, escala)
- ✓ Interação completa (mouse + teclado)
- ✓ SRU ortho 2D (16:9)
- ✓ Algoritmo robusto de detecção de ponto
- ✓ Código otimizado e comentado

O jogo oferece uma experiência educativa envolvente com progressão clara e feedback visual satisfatório.

---

**Compilação**: `g++ -o jogo jogo.c++ -lfreeglut -lopengl32 -std=c++17`

**Execução**: `./jogo`
