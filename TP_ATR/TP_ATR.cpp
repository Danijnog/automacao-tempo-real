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

// Variável global para NSEQ do hotbox e da remota
static LONG nseq_counter_hotbox = 0;
static LONG nseq_counter_remote = 0;
static LONG sem_space_counter = CAP_BUFF;  // Contador para indicar o valor do semáforo sem_space (número de espaços livres na lista)

typedef struct Node {
	std::string       msg;       // Conteudo da mensagem
	struct Node*     next = NULL;      // Ponteiro para o proximo nó da lista circular
} Node;

CRITICAL_SECTION cs_list;  // protege lista e free_list

Node  pool[CAP_BUFF];      // bloco de nós pré-alocado
Node* free_list = NULL;    // nós livres
Node* head = NULL;         // Último elemento da lista circular (head->next é o 1.º e tb o 1° que deve ser consumido)

HANDLE sem_tipo[2];        // Um semáforo por tipo de mensagem para indicar que há mensagem do respectivo tipo
HANDLE sem_space;          // Conta nós livres no buffer (0–200)


HANDLE hRemoteEvent; // Handle para sinalizar que há mensagens no arquivo de disco para a tarefa 4
HANDLE hFileFullEvent; // Evento para sinalizar que o arquivo está cheio e não pode ser escrito
HANDLE hFinishAllEvent; // Evento para encerrar todas as threads do programa
HANDLE hPauseEventC; // Handle para controle de pausa/continuação
HANDLE hPauseEventD; // Handle para controle de pausa/continuação
HANDLE hPauseEventH; // Handle para controle de pausa/continuação
HANDLE hPauseEventS; // Handle para controle de pausa/continuação
HANDLE hPauseEventQ; // Handle para controle de pausa/continuação
HANDLE hMutexFile; // Handle para exclusão mútua ao acesso do arquivo de log (txt)
HANDLE hOpenMailslotVRQEvent;  //Handle para o evento que indica que o mailslot da tarefa Visualizacao de Rodas Quentes foi criado
HANDLE hMailslotVRodasQ = NULL;  // Handle para o mailslot da tarefa de Visualizacao de Rodas Quentes


// Inicialização da lista
static void initialize_circular_list(void)
{
	// Todos os 200 nós começam na free_list 
	for (int i = 0; i < CAP_BUFF; ++i) {
		pool[i].next = free_list;
		free_list = &pool[i];
	}
}

// Funções utilitarias 

static Node* alloc_node(void)
{
	// Remove nó da free_list  
	Node* n;
	EnterCriticalSection(&cs_list);
	n = free_list;
	free_list = free_list->next;
	LeaveCriticalSection(&cs_list);
	return n;                   
}

static void recycle_node(Node* n)
{
	// Devolve nó à free_list para reutilização futura 
	EnterCriticalSection(&cs_list);
	n->msg = "";
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

void imprime_lista_circular() {  
	
	// Imprime todo o conteudo da lista circular no console principal e esvazia a lista

	std::cout << "Impressao da lista:" << std::endl;

	Node* cursor = NULL;

	while (head != NULL) {
		EnterCriticalSection(&cs_list);  // Espera permissão para acessar a lista circular
		cursor = head->next;
		std::cout << cursor->msg << std::endl;

		// Remoção do nó da lista circular 
		Node* prev = head;
		while (prev->next != cursor) prev = prev->next;

		if (cursor == head) {             // Ajusta head se ela estiver sendo removida 
			if (cursor->next == cursor) {
				head = NULL;
			}
			else {
				head = prev;
				prev->next = cursor->next;
			}
		}
		else {
			prev->next = cursor->next;        // desvincula alvo
		}
		
		LeaveCriticalSection(&cs_list);
		recycle_node(cursor);
	}
} 

void createProcess(const char* path) {
	
	//Função para criar processos a partir do executável passado como parâmetro para a função.
	
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si)); // Zera a estrutura
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	ZeroMemory(&pi, sizeof(pi));

	char exec[50];
	strcpy_s(exec, path);

	if (!CreateProcessA(NULL, exec, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi)) {
		std::cerr << "Erro ao criar processo: " << path << " Código: " << GetLastError() << std::endl;
		return;
	}
	else {
		
		std::cout << "Processo criado com PID: " << pi.dwProcessId << " a partir do executavel: " << exec << std::endl;
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
}

static void deposit_messages(std::string Mensagem, int tipo) {
	
	// Aloca a mensagem em um nó livre
	Node* n = alloc_node();             // obtém slot livre
	n->msg = Mensagem;

	// Insere o nó com a mensagem no fim da lista circular                   
	//  head aponta SEMPRE para o primeiro nó (o nó mais velho)   
	EnterCriticalSection(&cs_list);
	if (!head) {                         // Se lista vazia   
		head = n;
		n->next = n;                     // Nó único aponta para si
	}
	else {
		n->next = head->next;          // insere após head
		head->next = n;
		head = n;                  // n vira a nova head
	}
	LeaveCriticalSection(&cs_list);

	// Sinaliza disponibilidade à thread que tem interesse nessa mensagem 
	ReleaseSemaphore(sem_tipo[tipo], 1, NULL);
	
}


std::string generate_random_id() {
	/*
	* Função para gerar um ID aleatório no formato XXX-XXXX, onde XXX são letras maiúsculas e XXXX são dígitos.
	*/

	// Gerar 3 letras maiúsculas aleatórias
	std::ostringstream id;
	for (int i = 0; i < 3; i++) {
		id << static_cast<char>('A' + (rand() % 26));
	}
	id << "-";

	// Gerar e adicionar 4 dígitos aleatórios
	id << std::setw(4) << std::setfill('0') << (rand() % 10000);

	return id.str();

}

void initialize_circular_file(std::string file_path) {
	/*
	* Inicializar o arquivo circular txt para armazenamento de mensagens.
	*/
	std::ofstream file(file_path, std::ios::binary | std::ios::trunc);
	int head = 0, tail = 0;

	file.write(reinterpret_cast<char*>(&head), sizeof(int));
	file.write(reinterpret_cast<char*>(&tail), sizeof(int));

	std::string empty(40, '\0');
	for (int i = 0; i < CAP_BUFF; i++)
		file.write(empty.c_str(), 40);

	file.close();
}

int write_messages(std::string file_path, const std::string& msg) {
	/*
	* Escreve no arquivo circular txt as mensagens.
	*/
	const int HEADER_SIZE = sizeof(int) * 2;
	const int MSG_SIZE = 256;
	std::fstream file(file_path, std::ios::binary | std::ios::in | std::ios::out); // Modos binários, leitura e escrita
	int head, tail;
	file.read(reinterpret_cast<char*>(&head), sizeof(int)); // Lê os 4 primeiros bytes (sizeof(int)) do arquivo e armazena em head
	file.read(reinterpret_cast<char*>(&tail), sizeof(int)); // Lê os próximos 4 bytes (sizeof(int)) do arquivo e armazena em tail

	int next_tail = (tail + 1) % CAP_BUFF;
	if (next_tail == head) {
		std::cout << "Buffer cheio, não é possível escrever a mensagem." << std::endl;
		file.close();
		return 1;
	}

	file.seekp(HEADER_SIZE + tail * MSG_SIZE); // Seekp posiciona o ponteiro de escrita no arquivo
	std::string fixed_msg = msg;
	fixed_msg.resize(MSG_SIZE, '\0');
	file.write(fixed_msg.c_str(), MSG_SIZE);

	file.seekp(sizeof(int)); // Move o ponteiro de escrita do arquivo
	file.write(reinterpret_cast<const char*>(&next_tail), sizeof(int));
	file.close();
	return 0;
}



DWORD WINAPI keyboard_control_thread(LPVOID) {
	/*
	* Controle do teclado para pausar e continuar a execução das threads.
	*/
	int Tecla = 0;
	int paused = 0;
	DWORD pausedC; 
	DWORD pausedD;
	DWORD pausedH;
	DWORD pausedS;
	DWORD pausedQ;
	printf("Pressione 'c' para pausar/continuar a simulacao ou 'ESC' para encerrar...\n\n");

	while(TRUE) {
		Tecla = _getch();
		
		switch (Tecla) {
			case 'c':
			case 'C':
				pausedC =  WaitForSingleObject(hPauseEventC, 0);
				if (pausedC == WAIT_OBJECT_0) {
					ResetEvent(hPauseEventC);
					std::cout << "Geracao de mensagens PAUSADA." << std::endl;
				}
				else if (pausedC == WAIT_TIMEOUT) {
					SetEvent(hPauseEventC);
					std::cout << "Geracao de mensagens CONTINUANDO." << std::endl;
				}
				else {
					std::cout << "Problema ao pausar a geracao de mensagens: " << pausedC << std::endl;
				}
				break;
			case 'd':
			case 'D':
				pausedD = WaitForSingleObject(hPauseEventD, 0);
				if (pausedD == WAIT_OBJECT_0) {
					ResetEvent(hPauseEventD);
					std::cout << "Captura de dados de sinalizacao PAUSADA." << std::endl;
				}
				else if (pausedD == WAIT_TIMEOUT) {
					SetEvent(hPauseEventD);
					std::cout << "Captura de dados de sinalizacao CONTINUANDO." << std::endl;
				}
				else {
					std::cout << "Problema ao pausar a captura de dados de sinalizacao: " << pausedD << std::endl;
				}
				break;
			case 'h':
			case 'H':
				pausedH = WaitForSingleObject(hPauseEventH, 0);
				if (pausedH == WAIT_OBJECT_0) {
					ResetEvent(hPauseEventH);
					std::cout << "Captura de dados de rodas quentes PAUSADA." << std::endl;
				}
				else if (pausedH == WAIT_TIMEOUT) {
					SetEvent(hPauseEventH);
					std::cout << "Captura de dados de rodas quentes CONTINUANDO." << std::endl;
				}
				else {
					std::cout << "Problema ao pausar a captura de dados de rodas quentes: " << pausedH << std::endl;
				}
				break;
			case 's':
			case 'S':
				pausedS = WaitForSingleObject(hPauseEventS, 0);
				if (pausedS == WAIT_OBJECT_0) {
					ResetEvent(hPauseEventS);
					std::cout << "Exibicao de dados de sinalizacao PAUSADA." << std::endl;
				}
				else if (pausedS == WAIT_TIMEOUT) {
					SetEvent(hPauseEventS);
					std::cout << "Exibicao de dados de sinalizacao CONTINUANDO." << std::endl;
				}
				else {
					std::cout << "Problema ao pausar a exibicao de dados de sinalizacao: " << pausedS << std::endl;
				}
				break;
			case 'q':
			case 'Q':
				pausedQ = WaitForSingleObject(hPauseEventQ, 0);
				if (pausedQ == WAIT_OBJECT_0) {
					ResetEvent(hPauseEventQ);
					std::cout << "Exibicao de rodas quentes PAUSADA." << std::endl;
				}
				else if (pausedQ == WAIT_TIMEOUT) {
					SetEvent(hPauseEventQ);
					std::cout << "Exibicao de rodas quentes CONTINUANDO." << std::endl;
				}
				else {
					std::cout << "Problema ao pausar a exibicao de rodas quentes: " << pausedQ << std::endl;
				}
				break;
			case 27:    //ESC
				SetEvent(hFinishAllEvent);
				printf("Esc pressionado. Encerrando...\n");
				return 0;
			default:
				std::cout << "Entrada invalida!" << std::endl;
				
		}

		

	}
	
	return 0;
}

DWORD WINAPI generate_hotbox_message(LPVOID) {
	// Mensagens provenientes dos detectores de rodas quentes (hotbox).
	
	HANDLE hEvent;
	HANDLE hExecuting[2] = { hFinishAllEvent, hPauseEventC };
	HANDLE hMultObj[2] = { hFinishAllEvent, sem_space };
	DWORD status;
	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	std::string msg;
	std::ostringstream mensagem;
	int threadBloqueada = -1;
	//int counter = 0;  //usado para imprimir mensagens periodicamente

	while (true) {
		// Checa se deve encerrar ou pausar
		DWORD finish = WaitForMultipleObjects(2, hExecuting, FALSE, INFINITE);
		if ((finish - WAIT_OBJECT_0) == 0) {
			std::cout << "FINISH 1 HOTBOX " << std::endl;
			break;
		}
		if ((finish - WAIT_OBJECT_0) != 0 && (finish - WAIT_OBJECT_0) != 1) {
			printf("CLP: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
			return 1;
		}                                                            

		status = WaitForSingleObject(hEvent, 500); // Aguarda o timeout de 500ms para gerar a próxima mensagem
		

		if (status == WAIT_TIMEOUT) {
			// NSEQ
			LONG nseq = InterlockedIncrement(&nseq_counter_hotbox); // Long indica que o tipo de dados deve ter pelo menos 32 bits. Incrementa atomicamente a variável.

			// Tipo
			// int tipo = 99; // Tipo de mensagem (99)
			int msg_type = 99;

			// ID
			std::string id = generate_random_id();
			

			// Estado
			int estado = rand() % 2; // Estado (0 = Normal, 1 = Roda quente)

			// Hora
			SYSTEMTIME st;
			GetLocalTime(&st); // Obtém a hora local do sistema

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

			mensagem.str("");  // limpa o conteúdo
			mensagem.clear();  // reseta flags

			// Espera espaço livre no buffer 
			if (InterlockedCompareExchange(&sem_space_counter, 0, 0) == 0) { //Interlock... returns the initial value of the Destination parameter
				std::cout << "Geracao de mensagens Hotbox bloqueada devido a falta de espaco na lista" << std::endl;
				threadBloqueada = 1;
			}
			
			DWORD dwWaitResult2 = WaitForMultipleObjects(2, hMultObj, FALSE, INFINITE); //Espera comando para finalizar ou espaco no buffer
			if ((dwWaitResult2 - WAIT_OBJECT_0) == 0) {
				std::cout << "FINISH 2 HOTBOX " << std::endl;
				break;
			}
			if ((dwWaitResult2 - WAIT_OBJECT_0) != 0 && (dwWaitResult2 - WAIT_OBJECT_0) != 1) {
				printf("Sinalizacao: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
				return 1;
			}
			InterlockedDecrement(&sem_space_counter);

			if (threadBloqueada == 1) {
				std::cout << "Geracao de mensagens Hotbox desbloqueada" << std::endl;
				threadBloqueada = 0;
			}

			
			deposit_messages(msg, 1); // Deposita a mensagem na lista circular

			/*
			 //Imprime uma mensagem salva na lista a cada 50 mensagens salvas
			if (counter % 50 == 1) {
				printf("Hotbox message: %s\n", msg.c_str());
				counter = 0;
			}
			else {
				counter += 1;
			}
			*/
		}

	}
	CloseHandle(hEvent);
	return 0;
}

DWORD WINAPI generate_remote_message(LPVOID) {
	/*
	* Mensagens de sinalização provenientes das remotas de E/S.
	*/
	HANDLE hEvent;
	HANDLE hExecuting[2] = {hFinishAllEvent, hPauseEventC};
	HANDLE hMultObj[2] = { hFinishAllEvent, sem_space };
	DWORD status;
	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	std::string msg;
	std::ostringstream mensagem;
	int threadBloqueada = -1;
	//int counter = 0;  //usado para imprimir mensagens periodicamente

	while (true) {

		// Checa se deve encerrar ou pausar
		DWORD finish = WaitForMultipleObjects(2, hExecuting, FALSE, INFINITE);
		if ((finish - WAIT_OBJECT_0) ==  0) {
			std::cout << "FINISH 1 REMOTE " << std::endl;
			break;
		}
		if ((finish - WAIT_OBJECT_0) != 0 && (finish - WAIT_OBJECT_0) != 1) {
			printf("CLP: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
			return 1;
		}

		int time_ms = (rand() % 1901) + 100; // Gera um número aleatório entre 100 e 2000
		status = WaitForSingleObject(hEvent, time_ms); // Aguarda o timeout para gerar a próxima mensagem (entre 100 e 2000 ms)
		

		if (status == WAIT_TIMEOUT) {
			// NSEQ
			long nseq = InterlockedIncrement(&nseq_counter_remote); // Long indica que o tipo de dados deve ter pelo menos 32 bits. Incrementa atomicamente a variável.

			// Tipo
			int msg_type = 00;

			// Diag
			int diag = rand() % 2; // (0 = Normal, 1 = Falha de hardware da remota)

			// Remota
			long remota = rand() % 1000; // Número da remota (0 a 999)

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
			GetLocalTime(&st); // Obtém a hora local do sistema


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
			mensagem.str("");  // limpa o conteúdo
			mensagem.clear();  // reseta flags

			// Espera espaço livre no buffer ou finalizacao
			if (InterlockedCompareExchange(&sem_space_counter, 0, 0) == 0) { //Interlock... returns the initial value of the Destination parameter
				std::cout << "Geracao de mensagens das Remotas bloqueada devido a falta de espaco na lista" << std::endl;
				threadBloqueada = 1;
			}
			DWORD dwWaitResult2 = WaitForMultipleObjects(2, hMultObj, FALSE, INFINITE); //Espera comando para finalizar ou espaco no buffer
			if (dwWaitResult2 - WAIT_OBJECT_0 == 0) {
				std::cout << "FINISH 2 REMOTE " << std::endl;
				break;
			}
			if (dwWaitResult2 - WAIT_OBJECT_0 != 0 && dwWaitResult2 - WAIT_OBJECT_0 != 1) {
				printf("Sinalizacao: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
				
			}
			InterlockedDecrement(&sem_space_counter);

			if (threadBloqueada == 1) {
				std::cout << "Geracao de mensagens das Remotas desbloqueada" << std::endl;
				threadBloqueada = 0;
			}
			

			;
			deposit_messages(msg, 0); // Deposita a mensagem na lista circular
			
			/*
			//Imprime uma mensagem a cada 50 
			if (counter % 50 == 1) {
				printf("Remote message: %s\n", msg.c_str());
				counter = 0;
			}
			else {
				counter += 1;
			}
			*/
		}
	}
	CloseHandle(hEvent);
	return 0;

}


/* ----------- THREAD CAPTURA DE DADOS DE SINALIZAÇÃO FERROVIÁRIA ------------------ */
DWORD WINAPI captura_sinalizacao(LPVOID)  // Lê mensagens de sinalização ferroviaria de 40 char
{
	Node* cursor = NULL;                 // posição atual de varredura
	std::vector<std::string> msgVector;   // Vetor para armazenar a mensagem
	int lineCount = 0;
	HANDLE hExecuting[2] = { hFinishAllEvent, hPauseEventD };
	HANDLE hMultObj[2] = { hFinishAllEvent, sem_tipo[0] };
	DWORD writtenBytesSF = 0;       // variavel para armazenar o numero de bytes escritos no mailslot

	

	while (TRUE) {                      
		
		// Checa se deve encerrar ou pausar
		DWORD finish = WaitForMultipleObjects(2, hExecuting, FALSE, INFINITE);
		if ((finish - WAIT_OBJECT_0) == 0) {
			std::cout << "FINISH 1 SINALIZACAO " << std::endl;
			break;
		}
		if ((finish - WAIT_OBJECT_0) != 0 && (finish - WAIT_OBJECT_0) != 1) {
			printf("Sinalizacao: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
			
		}
		
		// Aguarda comando de finalizar ou existir mensagem do meu tipo 
		DWORD dwWaitResult2 = WaitForMultipleObjects(2, hMultObj, FALSE, INFINITE);
		if ((dwWaitResult2 - WAIT_OBJECT_0) == 0) {
			std::cout << "FINISH 2 SINALIZACAO " << std::endl;
			break;
		}
		if ((dwWaitResult2 - WAIT_OBJECT_0) != 0 && (dwWaitResult2 - WAIT_OBJECT_0) != 1) {
			printf("Sinalizacao: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
			
		}

		// Espera permissão para acessar a lista circular
		EnterCriticalSection(&cs_list);

		if (!cursor) {                     // Primeira vez: começa no início
			if (head) {					   // Como ponteiro, head retorna falso apenas se for igual a NULL
				cursor = head->next;
			}
			else {
				cursor = NULL;
				LeaveCriticalSection(&cs_list);
				continue;
			}
		}

		// Percorre a lista até encontrar uma mensagem de 40 char
		while (cursor && cursor->msg.length() != 40)  
			cursor = cursor->next;


		Node* alvo = cursor;             // Nó correspondente à mensagem a ser processada
		cursor = NULL;                   // Reseta para ser usado na próxima busca


		// Remoção do nó da lista circular 
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

		// Verifica o valor de DIAG e dá destino à mensagem
		if (std::stoi(msgVector[2]) == 1) {

			// Enviar mensagem para tarefa 5 por pipes/mailslots
			BOOL retorno = WriteFile(hMailslotVRodasQ, alvo->msg.c_str(), alvo->msg.size(), &writtenBytesSF, NULL);
			if (retorno == 0) {
				std::cout << "Captura SF: Erro na transmissao por mailslot para o Terminal VRQ: " << GetLastError() << std::endl;
			}
			else if (alvo->msg.size() != writtenBytesSF) {
				std::cout << "Captura SF: Mensagem NAO foi enviada por completo para o Terminal VRQ.\n"
					<< "Tamanho da mensagem: " << alvo->msg.size()
					<< "\nTemanho Enviado: " << writtenBytesSF
					<< "\nMensagem: " << alvo->msg << std::endl;
			}
			else if (alvo->msg.size() == writtenBytesSF) {
				//std::cout << "Mensagem de Sinalizacao enviada por pipes: " << alvo->msg << std::endl;
			}
		}
		else {
			//Depositar mensagem no disco
			DWORD dwWaitResultMutex = WaitForSingleObject(hMutexFile, INFINITE);
			if (dwWaitResultMutex == WAIT_OBJECT_0) {
				int resultado = write_messages("sinalizacao.txt", alvo->msg);
				if (resultado == 0) {
					printf("Mensagem de Sinalizacao salva no disco: %s\n", alvo->msg.c_str());
				}
				else if (resultado == 1) {
					std::cout << "Arquivo cheio. Bloqueando tarefa..." << std::endl;
					WaitForSingleObject(hFileFullEvent, INFINITE);
				}

				ReleaseMutex(hMutexFile);
				SetEvent(hRemoteEvent);
			}
		}

		// Esvazia o vetor
		msgVector.clear();

		// Devolve o nó à lista de nós livres e indica espaço livre no buffer
		recycle_node(alvo);
		ReleaseSemaphore(sem_space, 1, NULL);
		InterlockedIncrement(&sem_space_counter);

	}
	
	return 0;
}

/* ----------- THREAD CAPTURA DE DADOS DOS DETECTORES DE RODA QUENTE ------------------ */
DWORD WINAPI captura_rodas_quentes(LPVOID)  // Lê mensagens dos detectores de rodas quentes de 34 char
{
	DWORD writtenBytes = 0;       // variavel para armazenar o numero de bytes escritos no mailslot
	Node* cursor = NULL;                 // posição inicial de varredura
	HANDLE hExecuting[2] = { hFinishAllEvent, hPauseEventH };
	HANDLE hMultObj[2] = { hFinishAllEvent, sem_tipo[1] };

	//Espera Mailslot ser criado
	WaitForSingleObject(hOpenMailslotVRQEvent, INFINITE);
	// Espera o handle para o mailslot do Terminal VRQ ser criado
	while (hMailslotVRodasQ == NULL) {
		Sleep(10);
	}

	while (TRUE) {                

		// Checa se deve encerrar ou pausar
		DWORD finish = WaitForMultipleObjects(2, hExecuting, FALSE, INFINITE);
		if ((finish - WAIT_OBJECT_0) == 0) {
			std::cout << "FINISH 1 RODAS " << std::endl;
			break;
		}
		if ((finish - WAIT_OBJECT_0) != 0 && (finish - WAIT_OBJECT_0) != 1) {
			printf("Rodas Quentes: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
			return 1;
		}

		// Aguarda comando de finalizar ou existir mensagem do meu tipo 
		DWORD dwWaitResult2 = WaitForMultipleObjects(2, hMultObj, FALSE, INFINITE); 
		if ((dwWaitResult2 - WAIT_OBJECT_0) == 0) {
			std::cout << "FINISH 2 RODAS " << std::endl;
			break;
		}
		if ((dwWaitResult2 - WAIT_OBJECT_0) != 0 && (dwWaitResult2 - WAIT_OBJECT_0) != 1) {
			printf("Sinalizacao: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
			return 1;
		}

		// Busca próximo nó do meu tipo na lista circular
		EnterCriticalSection(&cs_list);

		if (!cursor) {                     // Primeira vez: começa no início
			if (head) {          // Como ponteiro, head retorna falso apenas se for igual a NULL
				cursor = head->next;
			}
			else { 
				cursor = NULL;
				LeaveCriticalSection(&cs_list);
				continue;
			}  
		}

		while (cursor && cursor->msg.length() != 34)  // Percorre a lista até encontrar uma mensagem de 34 char
			cursor = cursor->next;


		Node* alvo = cursor;             // Nó correspondente à mensagem a ser processada
		cursor = NULL;           // Avança para ser usado na próxima busca


		// Remoção do nó da lista circular 
		Node* prev = head;
		while (prev->next != alvo) prev = prev->next;

		if (alvo == head) {             // Ajusta head se a cauda está sendo removida 
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
		BOOL retorno = WriteFile(hMailslotVRodasQ, alvo->msg.c_str(), alvo->msg.size(), &writtenBytes, NULL);
		if (retorno == 0) {
			std::cout << "Captura RQ: Erro na transmissao por mailslot para o Terminal VRQ: " << GetLastError() << std::endl;
		}
		else if (alvo->msg.size() != writtenBytes) {
			std::cout << "Captura RQ: Mensagem NAO foi enviada por completo para o Terminal VRQ.\n"
				<< "Tamanho da mensagem: " << alvo->msg.size()
				<< "\nTemanho Enviado: " << writtenBytes
				<< "\nMensagem: " << alvo->msg << std::endl;
		}
		else if (alvo->msg.size() == writtenBytes) {
			//std::cout << "Mensagem de Rodas quentes enviada por pipes: " << alvo->msg << std::endl;
		}

		
		
		//printf("Mensagem de Rodas quentes enviada por pipes: %s\n", alvo->msg.c_str());
		// Devolve o nó à lista de nós livres e indica espaço livre no buffer
		recycle_node(alvo);
		ReleaseSemaphore(sem_space, 1, NULL);
		InterlockedIncrement(&sem_space_counter);
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
	initialize_circular_list();
	initialize_circular_file("sinalizacao.txt");

	hMutexFile = CreateMutex(NULL, FALSE, TEXT("MutexFile"));
	
	//sem_space inicia em 200 => 200 vagas disponíveis           
	//sem_tipo[k] inicia em 0  => nenhuma mensagem disponivel para leitura     
	sem_space = CreateSemaphore(NULL, CAP_BUFF, CAP_BUFF, NULL);
	sem_tipo[0] = CreateSemaphore(NULL, 0, CAP_BUFF, NULL);
	sem_tipo[1] = CreateSemaphore(NULL, 0, CAP_BUFF, NULL);


	hPauseEventC = CreateEvent(
		NULL,   // Atributos de segurança padrão
		TRUE,   // Manual-reset (nós controlamos o reset)
		TRUE,   // Estado inicial (sinalizado = executando)
		NULL    // Sem nome
	);
	if (hPauseEventC == NULL) {
		printf("Erro ao criar evento hPauseEventC: %d\n", GetLastError());
		return 1;
	}

	hPauseEventD = CreateEvent(NULL, TRUE, TRUE, TEXT("PauseEventD"));
	if (hPauseEventD == NULL) {
		printf("Erro ao criar evento hPauseEventD: %d\n", GetLastError());
		return 1;
	}

	hPauseEventH = CreateEventA(NULL, TRUE, TRUE, "PauseEventH");
	if (hPauseEventH == NULL) {
		printf("Erro ao criar evento hPauseEventH: %d\n", GetLastError());
		return 1;
	}

	hPauseEventS = CreateEventA(NULL, TRUE, TRUE, "PauseEventS");
	if (hPauseEventS == NULL) {
		printf("Erro ao criar evento hPauseEventS: %d\n", GetLastError());
		return 1;
	}

	hPauseEventQ = CreateEventA(NULL, TRUE, TRUE, "PauseEventQ");
	if (hPauseEventQ == NULL) {
		printf("Erro ao criar evento hPauseEventQ: %d\n", GetLastError());
		return 1;
	}

	hRemoteEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("RemoteEvent"));
	if (hRemoteEvent == NULL) {
			printf("Erro ao criar evento hRemoteEvent: %d\n", GetLastError());
			return 1;
	}

	hFileFullEvent = CreateEvent(NULL, FALSE, FALSE, TEXT("FileFullEvent"));
	if (hFileFullEvent == NULL) {
		printf("Erro ao criar evento hFileFullEvent: %d\n", GetLastError());
		return 1;
	}

	hFinishAllEvent = CreateEventA(NULL, TRUE, FALSE, "FinishAllEvent");
	if (hFinishAllEvent == NULL) {
			printf("Erro ao criar evento hFinishAllEvent: %d\n", GetLastError());
			return 1;
	}
	
	hOpenMailslotVRQEvent = CreateEventA(NULL, TRUE, FALSE, "OpenMailslotVRQEvent");
	if (hFinishAllEvent == NULL) {
		printf("Erro ao criar evento hMailslotVRQEvent: %d\n", GetLastError());
		return 1;
	}
	

	HANDLE keyboardThread = CreateThread(NULL, 0, keyboard_control_thread, NULL, 0, &dwThreadIdKeyboard);
	HANDLE hotboxThread = CreateThread(NULL, 0, generate_hotbox_message, NULL, 0, &dwThreadIdHotbox);
	HANDLE remoteThread = CreateThread(NULL, 0, generate_remote_message, NULL, 0, &dwThreadIdRemote);
	HANDLE sinalizacaoThread = CreateThread(NULL, 0, captura_sinalizacao, NULL, 0, &dwThreadSinalizacao);
	HANDLE rodasQuentesThread = CreateThread(NULL, 0, captura_rodas_quentes, NULL, 0, &dwThreadRodasQuentes);
	PROCESS_INFORMATION piFerroviaria;
	PROCESS_INFORMATION piRodas;
	// Criação dos processos de exibição dos dados de sinalização ferroviária e de rodas quentes
	createProcess("../x64/Debug/consumidor_sin_ferroviaria.exe");
	createProcess("../x64/Debug/consumidor_rodas_quentes.exe");

	//Espera Mailslot ser criado
	WaitForSingleObject(hOpenMailslotVRQEvent, INFINITE);

	
	// Atributos: Name, DesiredAccess, ShareMode, SecurityAttributes, CreationDisposition, FlagsAndAttributes, TemplateFile
	hMailslotVRodasQ = CreateFileA("\\\\.\\mailslot\\mail_Vrodas_quentes", GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hMailslotVRodasQ == INVALID_HANDLE_VALUE) {
		printf("Erro ao obter handle hMailslotVRodasQ: %d\n", GetLastError());
		//return 1;
	}
	HANDLE hAllThreads[5] = { keyboardThread, hotboxThread, remoteThread, sinalizacaoThread, rodasQuentesThread };

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
		
	}

	//Espera todas as threads terminarem
	WaitForMultipleObjects(5, hAllThreads, TRUE, INFINITE);

	BOOL exitCodeHotbox = GetExitCodeThread(hotboxThread, &dwExitCode);
	if (exitCodeHotbox == 0) {
		printf("Falha ao encerrar thread Hotbox: %d\n", GetLastError());
	}
	else if (exitCodeHotbox == STILL_ACTIVE) {
		printf("Falha ao encerrar thread Hotbox: Thread ATIVA\n");
	}
	else {
		printf("Thread Hotbox ENCERRADA com sucesso\n");
	}
	CloseHandle(hotboxThread);

	BOOL exitCodeRemote = GetExitCodeThread(remoteThread, &dwExitCode);
	if (exitCodeRemote == 0) {
		printf("Falha ao encerrar thread Remote: %d\n", GetLastError());
	}
	else if (exitCodeRemote == STILL_ACTIVE) {
		printf("Falha ao encerrar thread Remote: Thread ATIVA\n");
	}
	else {
		printf("Thread Remote ENCERRADA com sucesso\n");
	}
	CloseHandle(remoteThread);
	
	BOOL exitCodeSinalizacao = GetExitCodeThread(sinalizacaoThread, &dwExitCode);
	if (exitCodeSinalizacao == 0) {
		printf("Falha ao encerrar thread Sinalizacao: %d\n", GetLastError());
	}
	else if (exitCodeSinalizacao == STILL_ACTIVE) {
		printf("Falha ao encerrar thread Sinalizacao: Thread ATIVA\n");
	}
	else {
		printf("Thread Sinalizacao ENCERRADA com sucesso\n");
	}
	CloseHandle(sinalizacaoThread);

	BOOL exitCodeRodas = GetExitCodeThread(rodasQuentesThread, &dwExitCode);
	if (exitCodeRodas == 0) {
		printf("Falha ao encerrar thread Rodas: %d\n", GetLastError());
	}
	else if (exitCodeRodas == STILL_ACTIVE) {
		printf("Falha ao encerrar thread Rodas: Thread ATIVA\n");
	}
	else {
		printf("Thread Rodas ENCERRADA com sucesso\n");
	}
	CloseHandle(rodasQuentesThread);
	
	BOOL exitCodeKeyboard = GetExitCodeThread(keyboardThread, &dwExitCode);
	if (exitCodeKeyboard == 0) {
		printf("Falha ao encerrar thread Keybord: %d\n", GetLastError());
	}
	else if (exitCodeKeyboard == STILL_ACTIVE) {
		printf("Falha ao encerrar thread Keybord: Thread ATIVA\n");
	}
	else {
		printf("Thread Keybord ENCERRADA com sucesso\n");
	}
	CloseHandle(keyboardThread);

	imprime_lista_circular(); //Imprime o conteudo que houver na lista ao encerrar o programa

	CloseHandle(sem_space); 
	CloseHandle(sem_tipo[0]); 
	CloseHandle(sem_tipo[1]);
	CloseHandle(hMutexFile);
	CloseHandle(hRemoteEvent);
	CloseHandle(hFileFullEvent);
	CloseHandle(hFinishAllEvent);
	CloseHandle(hPauseEventC);
	CloseHandle(hPauseEventD);
	CloseHandle(hPauseEventH);
	CloseHandle(hPauseEventS);
	CloseHandle(hPauseEventQ);
	CloseHandle(hOpenMailslotVRQEvent);
	DeleteCriticalSection(&cs_list);

	return EXIT_SUCCESS;
}






