# Automação em Tempo Real
Este repositório é referente ao Trabalho Prático da Disciplina de Automação em Tempo Real da UFMG.

## 🚂 Sistema de Monitoramento em Automação Industrial

### 📌 Tarefas:
- **Leitura dos CLPs**:
  - Simula a leitura de dados dos CLPs gerando dois tipos de mensagens:
    - Mensagens provenientes dos detectores de rodas quentes (hotbox) (500ms)
    - Mensagens provenientes das remotas de E/S (100-2000ms)
  - Todas as mensagens geradas são armazenadas em uma lista circular na memória.

- **Captura de dados de sinalização ferroviária**:
  - Captura as mensagens provenientes das remotas de E/S presentes em uma lista circular na memória.
    O campo 'DIAG' é analisado: caso '1', a mensagem é encaminhada para a tarefa de visualização de rodas quentes. CC, a mensagem é escrita em um arquivo em disco (txt).
  
- **Captura de dados de detectores de rodas quentes**:
  - Captura as mensagens provenientes de rodas quentes (hotbox) presentas em uma lista circular na memória.
    Tais mensagens são repassadas para a tarefa de visualização de rodas quentes.
  
- **Exibição de dados de sinalização ferroviária**:
  
- **Visualização de rodas quentes**:
  
- **Leitura do teclado**:

### 🛠️ Tecnologias Utilizadas

- C++ com Win32 API
- Visual Studio 2022
  
