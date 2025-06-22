#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

HANDLE hPauseEvent_S; // Handle para controle de pausa/continuação
HANDLE hFinishAll_Event; // Evento para encerrar todas as threads do programa
HANDLE hMutexFile; // Handle para exclusão mútua ao acesso do arquivo de log (txt)
HANDLE hRemoteEvent; // Handle para sinalizar que há mensagens no arquivo de disco para o Terminal VSF
HANDLE hFileFullEvent; // Evento para sinalizar que o arquivo está cheio e não pode ser escrito
HANDLE semMsgDisco_SF;          // Semaforo para contagem de mensagens disponiveis no arquivo em disco
HANDLE semEspacoDisco_SF;          // Semaforo para contagem de espacos disponiveis no arquivo em disco

// Estado do sensor: devemos ter pelo menos 20 estados reais aleatórios possíveis
std::vector<std::string> states = {
	"Tunel", "Ponte", "Passagem de Nivel", "Pare", "Siga",
	"Linha Impedida", "Buzine", "Desvio Atuado", "Desvio Livre", "Livre",
	"Manobra Proibida", "Velocidade Reduzida", "Ponte", "Homens Trabalhando", "Advertencia de Parada Total",
	"Termino de Precaucao", "Reassuma Velocidade", "Manutencao Mecanica", "Limite de Manobra", "Inicio de CTC"
};

// Formata e imprime a mensagem recebida por parametro
void process_messages(const std::string& line) {
	std::istringstream ss(line);
	std::string nseq, tipo, diag, id_remota, id_sensor, estado, hora;

	srand(time(0));
	int tam = states.size();
	int random_index = rand() % tam;

	// Separa os campos usando delimitar ';'
	getline(ss, nseq, ';'); // Lê dados de um fluxo de entrada (ss) e coloca dentro de nseq com a partir do delimitador
	getline(ss, tipo, ';');
	getline(ss, diag, ';');
	getline(ss, id_remota, ';');
	getline(ss, id_sensor, ';');
	getline(ss, estado, ';');
	getline(ss, hora, ';');

	std::cout << hora << " NSEQ: " << nseq << " REMOTA: " << id_remota
		<< " SENSOR: " << id_sensor << " ESTADO: " << states[random_index] << std::endl;
}

// Consome uma mensagem do arquivo circular de mensagens
std::string consume_message(std::string file_path) {
	const int HEADER_SIZE = sizeof(int) * 2;
	const int MSG_SIZE = 256;
	const int CAP_BUFF = 200;

	std::fstream file(file_path, std::ios::binary | std::ios::in | std::ios::out); // Abre o arquivo em modo binário, permitindo leitura e escrita
	if (!file.is_open()) {
		std::cerr << "Erro ao abrir o arquivo: " << file_path << std::endl;
		return "";
	}

	int head, tail;
	file.read(reinterpret_cast<char*>(&head), sizeof(int)); // Lê os primeiros 4 bytes do arquivo e armazena em head
	file.read(reinterpret_cast<char*>(&tail), sizeof(int)); // Lê os próximos 4 bytes do arquivo e armazena em tail

	if (head == tail) {
		std::cout << "Buffer vazio!\n";
		file.close();
		return "";
	}

	file.seekg(HEADER_SIZE + head * MSG_SIZE); // Permite buscar uma posição no arquivo (move o ponteiro de leitura do arquivo para a próxima mensagem a ser consumida)
	char buffer[MSG_SIZE + 1] = { 0 };
	file.read(buffer, MSG_SIZE);

	std::string message(buffer);

	// Apaga a mensagem
	file.seekp(HEADER_SIZE + head * MSG_SIZE); // Move o ponteiro de escrita do arquivo para o mesmo lugar da mensagem consumida
	std::string empty(MSG_SIZE + 1, '\0');
	file.write(empty.c_str(), MSG_SIZE); // Sobreescreve os 256 bytes da mensagem consumida com espaços vazios

	int new_head = (head + 1) % CAP_BUFF; 
	file.seekp(0); 
	file.write(reinterpret_cast<char*>(&new_head), sizeof(int));

	file.close();
	return message;
}


int main() {
	SYSTEMTIME st;
	GetLocalTime(&st);
	std::ostringstream oss;
	std::string message;

	hPauseEvent_S = OpenEventA(EVENT_ALL_ACCESS, FALSE, "PauseEventS");
	if (hPauseEvent_S == NULL) {
		printf("Erro ao criar evento hPauseEventS: %d\n", GetLastError());
		
	}

	hFinishAll_Event = OpenEventA(EVENT_ALL_ACCESS, FALSE, "FinishAllEvent");
	if (hFinishAll_Event == NULL) {
		printf("Erro ao criar evento hFinishAllEvent: %d\n", GetLastError());
		
	}

	hRemoteEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("RemoteEvent"));
	if (hRemoteEvent == NULL) {
		std::cout << "Erro ao abrir o evento remoto de sinalizacacao de escrita no arquivo txt: " << GetLastError() << std::endl;
		return 1;
	}

	hFileFullEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("FileFullEvent"));
	if (hFileFullEvent == NULL) {
		std::cout << "Erro ao abrir o evento de sinalizacao de arquivo cheio: " << GetLastError() << std::endl;
		return 1;
	}

	hMutexFile = OpenMutex(MUTEX_ALL_ACCESS, FALSE, TEXT("MutexFile"));
	if (hMutexFile == NULL) {
		std::cout << "Erro ao abrir o mutex para acessar arquivo txt: " << GetLastError() << std::endl;
		return 1;
	}

	semMsgDisco_SF = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, "semMsgDiscoSF");
	if (semMsgDisco_SF == NULL) {
		std::cout << "Erro ao abrir o semaphoro de contagem de mensagens em disco: " << GetLastError() << std::endl;
		return 1;
	}

	semEspacoDisco_SF = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, "semEspacoDiscoSF");
	if (semEspacoDisco_SF == NULL) {
		std::cout << "Erro ao abrir o semaphoro de contagem de espacos no disco: " << GetLastError() << std::endl;
		return 1;
	}

	HANDLE hExecuting[2] = { hFinishAll_Event, hPauseEvent_S };
	HANDLE hMultObj[2] = { hFinishAll_Event, semMsgDisco_SF };
	LONG semEspDiscoPrevCount = 0; 

	while (true) {
		// Checa se deve encerrar ou pausar
		DWORD finish = WaitForMultipleObjects(2, hExecuting, FALSE, 1);
		if (finish == WAIT_TIMEOUT) {
			finish = WaitForMultipleObjects(2, hExecuting, FALSE, INFINITE);
		}

		DWORD result = finish - WAIT_OBJECT_0;

		// Evento ESC foi capturado
		if (result == 0) {
			break;
		}

		// Aguarda comando de finalizar ou haver mensagem no arquivo
		DWORD dwWaitResult2 = WaitForMultipleObjects(2, hMultObj, FALSE, INFINITE);
		if ((dwWaitResult2 - WAIT_OBJECT_0) == 0) {
			break;
		}
		// Evento de que há mensagens no arquivo foi sinalizado
		if ((dwWaitResult2 - WAIT_OBJECT_0) == 1) {
			// Conquista mutex para acesso ao arquivo
			DWORD dwWaitResultMutex = WaitForSingleObject(hMutexFile, INFINITE);
			if (dwWaitResultMutex == WAIT_OBJECT_0) { // Conquistou o mutex 
				std::string consumed_msg = consume_message("sinalizacao.txt");
				process_messages(consumed_msg);
				ReleaseMutex(hMutexFile);
				ReleaseSemaphore(semEspacoDisco_SF, 1, &semEspDiscoPrevCount);
			}
		}
		if ((dwWaitResult2 - WAIT_OBJECT_0) != 0 && (dwWaitResult2 - WAIT_OBJECT_0) != 1) {
			printf("Captura SF: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());

		}
	}

	CloseHandle(hPauseEvent_S);
	CloseHandle(hFinishAll_Event);
	CloseHandle(hRemoteEvent);
	CloseHandle(hFileFullEvent);
	CloseHandle(hMutexFile);
	CloseHandle(semMsgDisco_SF);
	CloseHandle(semEspacoDisco_SF);
	return 0;
}