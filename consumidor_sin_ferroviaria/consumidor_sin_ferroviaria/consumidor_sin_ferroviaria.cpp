#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

HANDLE hPauseEvent_S; // Handle para controle de pausa/continuação
HANDLE hFinishAll_Event; // Evento para encerrar todas as threads do programa
HANDLE hMutexFile; // Handle para exclusão mútua ao acesso do arquivo de log (txt)
HANDLE hRemoteEvent; // Handle para sinalizar que há mensagens no arquivo de disco para a tarefa 4

// Estado do sensor: devemos ter pelo menos 20 estados reais aleatórios possíveis
std::vector<std::string> states = {
	"Tunel", "Ponte", "Passagem de Nivel", "Pare", "Siga",
	"Linha Impedida", "Buzine", "Desvio Atuado", "Desvio Livre", "Livre",
	"Manobra Proibida", "Velocidade Reduzida", "Ponte", "Homens Trabalhando", "Advertencia de Parada Total",
	"Termino de Precaucao", "Reassuma Velocidade", "Manutencao Mecanica", "Limite de Manobra", "Inicio de CTC"
};

void process_messages(const std::string& line) {
	/*
	* Formata as mensagens de uma linha do arquivo de forma requisitada no trabalho.
	*/
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

std::string consume_message(std::string file_path) {
	/*
	* Consome uma mensagem do arquivo circular de mensagens.
	*/
	const int HEADER_SIZE = sizeof(int) * 2;

	std::fstream file(file_path, std::ios::binary | std::ios::in | std::ios::out);
	if (!file.is_open()) {
		std::cerr << "Erro ao abrir o arquivo: " << file_path << std::endl;
		return 0;
	}

	int head, tail;
	file.read(reinterpret_cast<char*>(&head), sizeof(int));
	file.read(reinterpret_cast<char*>(&tail), sizeof(int));

	if (head == tail) {
		std::cout << "Buffer vazio!\n";
		file.close();
		return 0;
	}

	file.seekg(HEADER_SIZE + head * 40); // Permite buscar uma posição no arquivo (move o ponteiro de leitura do arquivo)
	char buffer[41 + 1] = { 0 };
	file.read(buffer, 41);

	std::string message(buffer);
	//std::cout << "Consumi: " << message << std::endl;

	// Apaga a mensagem
	file.seekp(HEADER_SIZE + head * 40); // Move o ponteiro de escrita do arquivo
	std::string empty(41, '\0');
	file.write(empty.c_str(), 40);

	int new_head = (head + 1) % 200;
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
		std::cerr << "Erro ao abrir o evento remoto de sinalizacacao de escrita no arquivo txt: " << GetLastError() << std::endl;
		return 1;
	}

	HANDLE hExecuting[2] = { hFinishAll_Event, hPauseEvent_S };
	while (true) {
		DWORD finish = WaitForMultipleObjects(2, hExecuting, FALSE, INFINITE);
		DWORD result = finish - WAIT_OBJECT_0;

		// Evento ESC foi capturado
		if (result == 0) {
			std::cout << "FINISH 1 SINALIZACAO " << std::endl;
			break;
		}

		// Evento de que há mensagens no arquivo foi sinalizado
		if (WaitForSingleObject(hRemoteEvent, 0) == WAIT_OBJECT_0) {
			hMutexFile = OpenMutex(SYNCHRONIZE, FALSE, TEXT("MutexFile"));
			if (hMutexFile == NULL) {
				std::cerr << "Erro ao abrir o mutex para acessar arquivo txt: " << GetLastError() << std::endl;
				return 1;
			}

			DWORD dwWaitResultMutex = WaitForSingleObject(hMutexFile, INFINITE);
			if (dwWaitResultMutex == WAIT_OBJECT_0) { // Conquistou o mutex e o evento está sinalizado
				std::string consumed_msg = consume_message("sinalizacao.txt");
				process_messages(consumed_msg);
				ReleaseMutex(hMutexFile);
			}
		}
	}

	CloseHandle(hPauseEvent_S);
	CloseHandle(hFinishAll_Event);
	CloseHandle(hRemoteEvent);
	CloseHandle(hMutexFile);
	return 0;
}