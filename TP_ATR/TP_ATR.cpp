#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <time.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <windows.h>
#include <conio.h>
#include <process.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <fstream>

#define CAP_BUFF 200         // capacidade do buffer circular, 200 mensagens

// Vari�vel global para NSEQ do hotbox e da remota
static LONG nseq_counter_hotbox = 0;
static LONG nseq_counter_remote = 0;

typedef struct Node {
	std::string       msg;       // Conteudo da mensagem
	struct Node*     next = NULL;      // Ponteiro para o proximo n� da lista circular
} Node;

CRITICAL_SECTION cs_list;  // protege lista e free_list
CRITICAL_SECTION file_access; // Protege acesso ao arquivo de log (txt)

Node  pool[CAP_BUFF];      // bloco de n�s pr�-alocado
Node* free_list = NULL;    // n�s livres
Node* head = NULL;         // �ltimo elemento da lista circular (head->next � o 1.� e tb o 1� que deve ser consumido)

HANDLE sem_tipo[2];        // um sem�foro por tipo de mensagem para indicar que h� mensagem do respectivo tipo
HANDLE sem_space;          // conta n�s livres no buffer (0�200)
HANDLE sem_txtspace;      // conta espa�o livre no arquivo em disco (txt) (0-200)

HANDLE hPauseEvent; // Handle para controle de pausa/continua��o
HANDLE hRemoteEvent; // Handle para sinalizar que h� mensagens no arquivo de disco para a tarefa 4


// Inicializa��o da lista
static void initialize_circular_list(void)
{
	// Todos os 200 n�s come�am na free_list 
	for (int i = 0; i < CAP_BUFF; ++i) {
		pool[i].next = free_list;
		free_list = &pool[i];
	}
}

// Fun��es utilitarias 

static Node* alloc_node(void)
{
	// Remove n� da free_list  
	Node* n;
	EnterCriticalSection(&cs_list);
	n = free_list;
	free_list = free_list->next;
	LeaveCriticalSection(&cs_list);
	return n;                   
}

static void recycle_node(Node* n)
{
	// Devolve n� � free_list para reutiliza��o futura 
	EnterCriticalSection(&cs_list);
	n->next = free_list;
	free_list = n;
	LeaveCriticalSection(&cs_list);
}

static void msgToVector(Node* n, std::vector<std::string>& msgVector) {  // Transforma a mensagem em um vetor de strings
	std::stringstream mensagem(n->msg);
	std::string campo;
	while (std::getline(mensagem, campo, ';')) {
		msgVector.push_back(campo);
	}
}


/*
static void print_circular_list(CircularList *circular_list) 
{
	
	// Exibe o conte�do da lista circular.
	
	WaitForSingleObject(circular_list->hMutex, INFINITE);

	printf("\nTamanho da lista circular: %d\n", circular_list->tam);

	Node* node = circular_list->begin;
	if (node == NULL) {
		printf("Lista circular vazia.\n");
	}

	else {
		int i = 0;
		do {
			printf("[%02d]: Mensagem: %s\n", i++, node->data.content);
			node = node->next;

		} while (node != circular_list->begin);
	}

	ReleaseMutex(circular_list->hMutex);
}
*/
static void deposit_messages(std::string Mensagem, int tipo) {
	
	// Aloca a mensagem em um n� livre
	Node* n = alloc_node();             // obt�m slot livre
	n->msg = Mensagem;

	// Insere o n� com a mensagem no fim da lista circular                   
	//  head aponta SEMPRE para o primeiro n� (o n� mais velho)   
	EnterCriticalSection(&cs_list);
	if (!head) {                         // Se lista vazia   
		head = n;
		n->next = n;                     // N� �nico aponta para si
	}
	else {
		n->next = head->next;          // insere ap�s head
		head->next = n;
		head = n;                  // n vira a nova head
	}
	LeaveCriticalSection(&cs_list);

	// Sinaliza disponibilidade � thread que tem interesse nessa mensagem 
	ReleaseSemaphore(sem_tipo[tipo], 1, NULL);
	
}


std::string generate_random_id() {
	/*
	* Fun��o para gerar um ID aleat�rio no formato XXX-XXXX, onde XXX s�o letras mai�sculas e XXXX s�o d�gitos.
	*/

	// Gerar 3 letras mai�sculas aleat�rias
	std::ostringstream id;
	for (int i = 0; i < 3; i++) {
		id << static_cast<char>('A' + (rand() % 26));
	}
	id << "-";

	// Gerar e adicionar 4 d�gitos aleat�rios
	id << std::setw(4) << std::setfill('0') << (rand() % 10000);

	return id.str();

}

DWORD WINAPI keyboard_control_thread(LPVOID) {
	/*
	* Controle do teclado para pausar e continuar a execu��o das threads.
	*/
	int nTecla = 0;
	int paused = 0;
	printf("Pressione 'c' para pausar/continuar a simulacao ou 'ESC' para encerrar...\n\n");

	while(1) {
		nTecla = _getch();
		if (nTecla == 'c' || nTecla == 'C') {
			paused = ~paused; // Alterna entre 0 e 1

			//DWORD dwWaitResult = WaitForSingleObject(hPauseEvent, 0); // Verifica o estado do evento
			if (paused) {
				ResetEvent(hPauseEvent);
				printf("Simulacao PAUSADA\n\n");
			}
			else {
				SetEvent(hPauseEvent); // Sinaliza o evento (executando)
				printf("Simulacao CONTINUANDO\n");
			}
		}

		else if (nTecla == 27) {
			printf("Esc pressionado. Encerrando...\n");
			return 0;
		}

	}
	return 0;
}

DWORD WINAPI generate_hotbox_message(LPVOID) {
	/*
	* Mensagens provenientes dos detectores de rodas quentes (hotbox).
	*/
	HANDLE hEvent;
	DWORD status;
	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	std::string msg;
	std::ostringstream mensagem;

	while (true) {
		                                                                 //ERRO: mesclar os waitforsingle object num waitformultopleobjects
		                                                                 // com temporizador em vez de 500 de espera
		status = WaitForSingleObject(hEvent, 500); // Aguarda o timeout de 500ms para gerar a pr�xima mensagem
		// Espera espa�o livre no buffer � bloqueia se sem_space == 0.                 
		WaitForSingleObject(sem_space, INFINITE);

		WaitForSingleObject(hPauseEvent, INFINITE); // Espera at� que o evento seja sinalizado (executando)
		if (status == WAIT_TIMEOUT) {
			// NSEQ
			LONG nseq = InterlockedIncrement(&nseq_counter_hotbox); // Long indica que o tipo de dados deve ter pelo menos 32 bits. Incrementa atomicamente a vari�vel.

			// Tipo
			// int tipo = 99; // Tipo de mensagem (99)
			int msg_type = 99;

			// ID
			std::string id = generate_random_id();
			

			// Estado
			int estado = rand() % 2; // Estado (0 = Normal, 1 = Roda quente)

			// Hora
			SYSTEMTIME st;
			GetLocalTime(&st); // Obt�m a hora local do sistema

			//printf("DEBUG: ID gerado: [%s]\n", id.c_str());

			mensagem << std::setw(7) << std::setfill('0') << nseq << ";"
				<< std::setw(2) << std::setfill('0') << msg_type << ";"
				<< id << ";"
				<< estado << ";"
				<< std::setw(2) << std::setfill('0') << st.wHour << ":"
				<< std::setw(2) << std::setfill('0') << st.wMinute << ":"
				<< std::setw(2) << std::setfill('0') << st.wSecond << ":"
				<< std::setw(3) << std::setfill('0') << st.wMilliseconds;

			msg = mensagem.str();

			mensagem.str("");  // limpa o conte�do
			mensagem.clear();  // reseta flags

			printf("Hotbox message: %s\n", msg.c_str());
			deposit_messages(msg, 1); // Deposita a mensagem na lista circular
		}

	}
	CloseHandle(hEvent);
	return 0;
}

DWORD WINAPI generate_remote_message(LPVOID) {
	/*
	* Mensagens de sinaliza��o provenientes das remotas de E/S.
	*/
	HANDLE hEvent;
	DWORD status;
	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	std::string msg;
	std::ostringstream mensagem;

	while (true) {
																	//ERRO: mesclar os waitforsingle object num waitformultopleobjects
																	// com temporizador em vez de 500 de espera
		int time_ms = (rand() % 1901) + 100; // Gera um n�mero aleat�rio entre 100 e 2000
		status = WaitForSingleObject(hEvent, time_ms); // Aguarda o timeout para gerar a pr�xima mensagem (entre 100 e 2000 ms)
		// Espera espa�o livre no buffer � bloqueia se sem_space == 0.                 
		WaitForSingleObject(sem_space, INFINITE);

		WaitForSingleObject(hPauseEvent, INFINITE); // Espera at� que o evento seja sinalizado (executando)
		if (status == WAIT_TIMEOUT) {
			// NSEQ
			long nseq = InterlockedIncrement(&nseq_counter_remote); // Long indica que o tipo de dados deve ter pelo menos 32 bits. Incrementa atomicamente a vari�vel.

			// Tipo
			int msg_type = 00;

			// Diag
			int diag = rand() % 2; // (0 = Normal, 1 = Falha de hardware da remota)

			// Remota
			long remota = rand() % 1000; // N�mero da remota (0 a 999)

			// ID 
			// ID
			std::string id = generate_random_id();

			// Estado
			int estado = (rand() % 2) + 1; // Estado do sensor (1 = False, 2 = True)

			if (diag == 1) {
				id = "XXXXXXXX";
				estado = 0;
			}

			// Hora
			SYSTEMTIME st;
			GetLocalTime(&st); // Obt�m a hora local do sistema


			mensagem << std::setw(7) << std::setfill('0') << nseq << ";"
				<< std::setw(2) << std::setfill('0') << msg_type << ";"
				<< diag << ";"
				<< std::setw(3) << std::setfill('0') << remota << ";"
				<< id << ";"
				<< estado << ";"
				<< std::setw(2) << std::setfill('0') << st.wHour << ":"
				<< std::setw(2) << std::setfill('0') << st.wMinute << ":"
				<< std::setw(2) << std::setfill('0') << st.wSecond << ":"
				<< std::setw(3) << std::setfill('0') << st.wMilliseconds;

			msg = mensagem.str();
			mensagem.str("");  // limpa o conte�do
			mensagem.clear();  // reseta flags

			printf("Remote message: %s\n", msg.c_str());
			deposit_messages(msg, 0); // Deposita a mensagem na lista circular

		}
	}
	CloseHandle(hEvent);
	return 0;

}


/* ----------- THREAD CAPTURA DE DADOS DE SINALIZA��O FERROVI�RIA ------------------ */
DWORD WINAPI captura_sinalizacao(LPVOID)  // L� mensagens de sinaliza��o ferroviaria de 40 char
{
	Node* cursor = NULL;                 // posi��o atual de varredura
	std::vector<std::string> msgVector;   // Vetor para armazenar a mensagem
	int lineCount = 0;
	DWORD dwWaitResult;

	while (TRUE) {                      //Substituir True por evento de bloqueio dessa thread                   ERRO

		// Aguarda existir mensagem do meu tipo 
		WaitForSingleObject(sem_tipo[0], INFINITE);

		// Espera permiss�o para acessar a lista circular
		EnterCriticalSection(&cs_list);

		if (!cursor) {                     // Primeira vez: come�a no in�cio
			if (head) {					   // Como ponteiro, head retorna falso apenas se for igual a NULL
				cursor = head->next;
			}
			else {
				cursor = NULL;
			}
		}

		// Percorre a lista at� encontrar uma mensagem de 40 char
		while (cursor && cursor->msg.length() != 40)  
			cursor = cursor->next;

		if (!cursor) {                   // Se lista vazia 
			LeaveCriticalSection(&cs_list);
			continue;
		}

		Node* alvo = cursor;             // N� correspondente � mensagem a ser processada
		cursor = cursor->next;           // Avan�a para ser usado na pr�xima busca


		// Remo��o do n� da lista circular 
		Node* prev = head;
		while (prev->next != alvo) prev = prev->next;

		if (alvo == head) {             // Ajusta head se ela estiver sendo removida 
			if (alvo->next == alvo) {
				head = NULL;
				cursor = NULL;
			}
			else {
				head = prev;
			}
		}
		prev->next = alvo->next;        // desvincula alvo
		LeaveCriticalSection(&cs_list);


		// Transforma a mensagem em um vetor de strings
		msgToVector(alvo, msgVector);

		// Verifica o valor de DIAG e d� destino � mensagem
		if (std::stoi(msgVector[2]) == 1) {
			// Enviar mensagem para tarefa 5 por pipes/mailslots
			printf("DIAG = %s, ", msgVector[2].c_str());
			printf("Mensagem de Sinalizacao enviada por pipes para visualizacao de rodas quentes: %s\n", alvo->msg.c_str());
		}
		else {
			// Se n�o tiver espa�o no disco: avisar tarefa 4 e bloquear-se, marcar evento de bloqueio
			
			//sem_txtspace = OpenSemaphore(SEMAPHORE_MODIFY_STATE, FALSE, TEXT("SemaforoEspacoDisco"));
			dwWaitResult = WaitForSingleObject(sem_txtspace, INFINITE); // Espera espa�o no disco (pode at� 200 mensagens no m�ximo) (Decrementa valor do sem�foro)
			if (dwWaitResult == WAIT_OBJECT_0) {
				EnterCriticalSection(&file_access);

				// Depositar mensagem no disco
				std::ofstream outfile("sinalizacao.txt", std::ios::app);
				if (outfile.is_open()) {
					outfile << alvo->msg.c_str() << std::endl;
					outfile.close();
					printf("Mensagem depositada no disco: %s\n", alvo->msg.c_str());
					SetEvent(hRemoteEvent); // Sinaliza que h� mensagem no arquivo de disco
				}
				else
					printf("Erro ao abrir o arquivo de disco para escrita.\n");
			}
			else
				printf("Erro ao esperar por espa�o no disco: %d\n", GetLastError());

			LeaveCriticalSection(&file_access);

		}

		// Esvazia o vetor
		msgVector.clear();


		// Devolve o n� � lista de n�s livres e indica espa�o livre no buffer
		recycle_node(alvo);
		ReleaseSemaphore(sem_space, 1, NULL);

	}
	return 0;
}

/* ----------- THREAD CAPTURA DE DADOS DOS DETECTORES DE RODA QUENTE ------------------ */
DWORD WINAPI captura_rodas_quentes(LPVOID)  // L� mensagens dos detectores de rodas quentes de 34 char
{
	Node* cursor = NULL;                 // posi��o inicial de varredura

	while (TRUE) {                      //Substituir True por evento de bloqueio dessa thread                   ERRO

		// Aguarda existir mensagem do meu tipo 
		WaitForSingleObject(sem_tipo[1], INFINITE);

		// Busca pr�ximo n� do meu tipo na lista circular
		EnterCriticalSection(&cs_list);

		if (!cursor) {                     // Primeira vez: come�a no in�cio
			if (head) {          // Como ponteiro, head retorna falso apenas se for igual a NULL
				cursor = head->next;
			}
			else { 
				cursor = NULL;
			}  
		}

		while (cursor && cursor->msg.length() != 34)  // Percorre a lista at� encontrar uma mensagem de 34 char
			cursor = cursor->next;

		if (!cursor) {                   // lista vazia 
			LeaveCriticalSection(&cs_list);
			continue;
		}

		Node* alvo = cursor;             // N� correspondente � mensagem a ser processada
		cursor = cursor->next;           // Avan�a para ser usado na pr�xima busca


		// Remo��o do n� da lista circular 
		Node* prev = head;
		while (prev->next != alvo) prev = prev->next;

		if (alvo == head) {             // Ajusta head se a cauda est� sendo removida 
			if (alvo->next == alvo) {
				head = NULL;
				cursor = NULL;
			}
			else {
				head = prev;
			}
		}
		prev->next = alvo->next;        // desvincula alvo
		LeaveCriticalSection(&cs_list);


		// Enviar mensagem para tarefa 5 por pipes/mailslots
		printf("Mensagem de rodas quentes enviada por pipes: %s\n", alvo->msg.c_str());


		// Devolve o n� � lista de n�s livres e indica espa�o livre no buffer
		recycle_node(alvo);
		ReleaseSemaphore(sem_space, 1, NULL);

	}
	return 0;
}

int main() {

	DWORD dwThreadIdKeyboard;
	DWORD dwThreadIdHotbox;
	DWORD dwThreadIdRemote;
	DWORD dwThreadSinalizacao;
	DWORD dwThreadRodasQuentes;
	DWORD dwExitCode;

	SetConsoleOutputCP(CP_ACP);
	InitializeCriticalSection(&cs_list);
	InitializeCriticalSection(&file_access);
	initialize_circular_list();

	//sem_space inicia em 200 => 200 vagas dispon�veis           
	//sem_tipo[k] inicia em 0  => nenhuma mensagem disponivel para leitura     
	sem_space = CreateSemaphore(NULL, CAP_BUFF, CAP_BUFF, NULL);
	sem_tipo[0] = CreateSemaphore(NULL, 0, CAP_BUFF, NULL);
	sem_tipo[1] = CreateSemaphore(NULL, 0, CAP_BUFF, NULL);
	sem_txtspace = CreateSemaphore(NULL, 10, 10, TEXT("SemaforoEspacoDisco"));

	hPauseEvent = CreateEvent(
		NULL,   // Atributos de seguran�a padr�o
		TRUE,   // Manual-reset (n�s controlamos o reset)
		TRUE,   // Estado inicial (sinalizado = executando)
		NULL    // Sem nome
	);
	hRemoteEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("RemoteEvent"));

	if (hPauseEvent == NULL) {
		printf("Erro ao criar evento: %d\n", GetLastError());
		return 1;
	}

	HANDLE keyboardThread = CreateThread(NULL, 0, keyboard_control_thread, NULL, 0, &dwThreadIdKeyboard);
	HANDLE hotboxThread = CreateThread(NULL, 0, generate_hotbox_message, NULL, 0, &dwThreadIdHotbox);
	HANDLE remoteThread = CreateThread(NULL, 0, generate_remote_message, NULL, 0, &dwThreadIdRemote);
	HANDLE sinalizacaoThread = CreateThread(NULL, 0, captura_sinalizacao, NULL, 0, &dwThreadSinalizacao);
	HANDLE rodasQuentesThread = CreateThread(NULL, 0, captura_rodas_quentes, NULL, 0, &dwThreadRodasQuentes);

	if (hotboxThread) {
		printf("Thread hotbox criada com ID = %0x \n", dwThreadIdHotbox);
	}

	if (remoteThread) {
		printf("Thread remota criada com ID = %0x \n", dwThreadIdRemote);
	}

	if (rodasQuentesThread) {
		printf("Thread rodas quentes criada com ID = %0x \n", dwThreadRodasQuentes);
	}

	if (sinalizacaoThread) {
		printf("Thread sinalizacao criada com ID = %0x \n", dwThreadSinalizacao);
	}

	if (keyboardThread) {
		printf("Thread de controle do teclado criada com ID = %0x \n\n", dwThreadIdKeyboard);
		WaitForSingleObject(keyboardThread, INFINITE);
	}

	//print_circular_list(&cl); // Exibe o conte�do da lista circular

	GetExitCodeThread(hotboxThread, &dwExitCode);
	CloseHandle(hotboxThread);

	GetExitCodeThread(remoteThread, &dwExitCode);
	CloseHandle(remoteThread);

	GetExitCodeThread(sinalizacaoThread, &dwExitCode);
	CloseHandle(sinalizacaoThread);

	GetExitCodeThread(rodasQuentesThread, &dwExitCode);
	CloseHandle(rodasQuentesThread);

	GetExitCodeThread(keyboardThread, &dwExitCode);
	CloseHandle(keyboardThread);

	CloseHandle(sem_space); 
	CloseHandle(sem_tipo[0]); 
	CloseHandle(sem_tipo[1]);
	DeleteCriticalSection(&cs_list);

	return EXIT_SUCCESS;
}