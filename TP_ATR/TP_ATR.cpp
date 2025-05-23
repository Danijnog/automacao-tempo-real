#include <iostream>
#include <time.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <windows.h>
#include <conio.h>

#define MAX_MSG 200

// Variável global para NSEQ do hotbox e da remota
static LONG nseq_counter_hotbox = 0;
static LONG nseq_counter_remote = 0;

HANDLE hPauseEvent; // Handle para controle de pausa/continuação

typedef struct {
	char content[100]; // Conteúdo da mensagem
	int type;
} Message;

typedef struct {
	Message m[MAX_MSG];
	int head;
	int tail;
	int count;
	HANDLE hMutex;
	HANDLE isNotFull;	
} CircularList;

CircularList cl;

void setConsoleColor(WORD color) {
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, color);
}

void initialize_circular_list(CircularList *circular_list) {
	circular_list->head = 0;
	circular_list->tail = 0;
	circular_list->count = 0;
	circular_list->hMutex = CreateMutex(NULL, FALSE, NULL);
	circular_list->isNotFull = CreateEvent(NULL, TRUE, TRUE, NULL); // Inicializa o evento como sinalizado
}

void display_circular_list(CircularList *circular_list) {
	/*
	* Exibe o conteúdo da lista circular.
	*/
	WaitForSingleObject(circular_list->hMutex, INFINITE);

	printf("\nTamanho da lista circular: %d\n", circular_list->count);
	printf("Conteudo da lista circular:\n");
	for (int i = 0; i < circular_list->count; i++) {
		//int index = (circular_list->tail + i) % 200;
		printf("[%d] %s\n", i, circular_list->m[i].content);
	}

	ReleaseMutex(circular_list->hMutex);
}

void deposit_messages(CircularList *circular_list, Message msg) {
	/*
	* Deposita mensagens na lista circular.
	*/
	WaitForSingleObject(circular_list->isNotFull, INFINITE); // Espera até que a lista não esteja cheia
	WaitForSingleObject(circular_list->hMutex, INFINITE);

	circular_list->m[circular_list->head] = msg;
	circular_list->head = (circular_list->head + 1) % 200;
	circular_list->count++;

	if (circular_list->count == MAX_MSG - 1) {
		SetEvent(circular_list->isNotFull); // Sinaliza que a lista está cheia
		printf("Lista cheia! \n");
	}

	ReleaseMutex(circular_list->hMutex);
}

void generate_random_id(char *buffer, size_t size) {
	/*
	* Função para gerar um ID aleatório no formato XXX-XXXX, onde XXX são letras maiúsculas e XXXX são dígitos.
	*/
	
	// Gerar 3 letras maiúsculas aleatórias
	char letters[4];
	for (int i = 0; i < 3; i++) {


		letters[i] = 'A' + (rand() % 26);
	}
	letters[3] = '\0';

	// Gerar 4 dígitos aleatórios
	int numbers = rand() % 10000;

	// Formatar no buffer fornecido
	snprintf(buffer, size, "%s-%04d", letters, numbers);
}

DWORD WINAPI keyboard_control_thread(LPVOID) {
	/*
	* Controle do teclado para pausar e continuar a execução das threads.
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
	Message msg;

	while (true) {
		status = WaitForSingleObject(hEvent, 500); // Aguarda o timeout de 500ms para gerar a próxima mensagem
		
		WaitForSingleObject(hPauseEvent, INFINITE); // Espera até que o evento seja sinalizado (executando)
		if (status == WAIT_TIMEOUT) {
			// NSEQ
			LONG nseq = InterlockedIncrement(&nseq_counter_hotbox); // Long indica que o tipo de dados deve ter pelo menos 32 bits. Incrementa atomicamente a variável.

			// Tipo
			// int tipo = 99; // Tipo de mensagem (99)
			msg.type = 99;

			// ID
			char id[9];
			generate_random_id(id, sizeof(id));

			// Estado
			int estado = rand() % 2; // Estado (0 = Normal, 1 = Roda quente)

			// Hora
			SYSTEMTIME st;
			GetLocalTime(&st); // Obtém a hora local do sistema

			snprintf(msg.content, sizeof(msg),
				"%07d;%02d;%s;%d;%02d:%02d:%02d:%03d",
				nseq,
				msg.type,
				id,
				estado,
				st.wHour,
				st.wMinute,
				st.wSecond,
				st.wMilliseconds);

			printf("Hotbox message: %s\n", msg.content);
			if (cl.count < MAX_MSG) {
				deposit_messages(&cl, msg); // Deposita a mensagem na lista circular
			}
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
	DWORD status;
	hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	Message msg;

	while (true) {
		int time_ms = (rand() % 1901) + 100; // Gera um número aleatório entre 100 e 2000
		status = WaitForSingleObject(hEvent, time_ms); // Aguarda o timeout para gerar a próxima mensagem (entre 100 e 2000 ms)

		WaitForSingleObject(hPauseEvent, INFINITE); // Espera até que o evento seja sinalizado (executando)
		if (status == WAIT_TIMEOUT) {
			// NSEQ
			long nseq = InterlockedIncrement(&nseq_counter_remote); // Long indica que o tipo de dados deve ter pelo menos 32 bits. Incrementa atomicamente a variável.

			// Tipo
			msg.type = 00;

			// Diag
			int diag = rand() % 2; // (0 = Normal, 1 = Falha de hardware da remota)

			// Remota
			long remota = rand() % 1000; // Número da remota (0 a 999)

			// ID 
			char id[9];
			generate_random_id(id, sizeof(id));

			// Estado
			int estado = (rand() % 2) + 1; // Estado do sensor (1 = False, 2 = True)

			if (diag == 1) {
				strncpy_s(id, "XXXXXXXX", sizeof(id));
				estado = 0;
			}

			// Hora
			SYSTEMTIME st;
			GetLocalTime(&st); // Obtém a hora local do sistema

			snprintf(msg.content, sizeof(msg.content),
				"%07d;%02d;%d;%03d;%s;%d;%02d:%02d:%02d:%03d",
				nseq,
				msg.type,
				diag,
				remota,
				id,
				estado,
				st.wHour,
				st.wMinute,
				st.wSecond,
				st.wMilliseconds);

			printf("Remote message: %s\n", msg.content);
			if (cl.count < MAX_MSG) {
				deposit_messages(&cl, msg); // Deposita a mensagem na lista circular
			}
		}
	}
	CloseHandle(hEvent);
	return 0;
}

int main() {

	DWORD dwThreadIdKeyboard;
	DWORD dwThreadIdHotbox;
	DWORD dwThreadIdRemote;
	DWORD dwExitCode;

	initialize_circular_list(&cl);

	hPauseEvent = CreateEvent(
		NULL,   // Atributos de segurança padrão
		TRUE,   // Manual-reset (nós controlamos o reset)
		TRUE,   // Estado inicial (sinalizado = executando)
		NULL    // Sem nome
	);

	if (hPauseEvent == NULL) {
		printf("Erro ao criar evento: %d\n", GetLastError());
		return 1;
	}

	HANDLE keyboardThread = CreateThread(NULL, 0, keyboard_control_thread, NULL, 0, &dwThreadIdKeyboard);
	HANDLE hotboxThread = CreateThread(NULL, 0, generate_hotbox_message, NULL, 0, &dwThreadIdHotbox);
	HANDLE remoteThread = CreateThread(NULL, 0, generate_remote_message, NULL, 0, &dwThreadIdRemote);

	if (hotboxThread) {
		printf("Thread hotbox criada com ID = %0x \n", dwThreadIdHotbox);
	}

	if (remoteThread) {
		printf("Thread remota criada com ID = %0x \n", dwThreadIdRemote);
	}

	if (keyboardThread) {
		printf("Thread de controle do teclado criada com ID = %0x \n", dwThreadIdKeyboard);
		WaitForSingleObject(keyboardThread, INFINITE);
	}

	display_circular_list(&cl); // Exibe o conteúdo da lista circular

	GetExitCodeThread(hotboxThread, &dwExitCode);
	CloseHandle(hotboxThread);

	GetExitCodeThread(remoteThread, &dwExitCode);
	CloseHandle(remoteThread);

	GetExitCodeThread(keyboardThread, &dwExitCode);
	CloseHandle(keyboardThread);

	return EXIT_SUCCESS;
}