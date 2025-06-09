#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

HANDLE hRemoteEvent;
HANDLE semTxtSpace;
HANDLE hPauseEvent_S; // Handle para controle de pausa/continuação
HANDLE hFinishAll_Event; // Evento para encerrar todas as threads do programa

// Estado do sensor: devemos ter pelo menos 20 estados reais aleatórios possíveis
std::vector<std::string> states = {
	"Tunel", "Ponte", "Passagem de Nivel", "Pare", "Siga",
	"Linha Impedida", "Buzine", "Desvio Atuado", "Desvio Livre", "Livre",
	"Manobra Proibida", "Velocidade Reduzida", "Ponte", "Homens Trabalhando", "Advertencia de Parada Total",
	"Termino de Precaucao", "Reassuma Velocidade", "Manutencao Mecanica", "Limite de Manobra", "Inicio de CTC"
};

void processMessages(const std::string& line) {
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

void consumeFirstMessage() {
	/*
	* Processa o arquivo em disco, consome a primeira mensagem no arquivo e reescreve as outras mensagens restantes no arquivo.
	*/
	std::string file_path = "sinalizacao.txt";
	std::vector<std::string> lines;
	std::string line;
	std::ifstream infile(file_path);

	if (!infile)
		std::cerr << "Erro ao abrir o arquivo em disco para leitura!" << file_path << std::endl;

	while (getline(infile, line))
		lines.push_back(line);
	infile.close();

	// Processa a primeira mensagem e reescreve o arquivo com as demais linhas que sobraram
	if (!lines.empty()) {
		processMessages(lines[0]); // Processa a primeira mensagem do arquivo

		std::ofstream outfile(file_path);
		for (size_t i = 1; i < lines.size(); i++) 
			outfile << lines[i] << "\n";
		
		outfile.close();
		
		ReleaseSemaphore(semTxtSpace, 1, NULL);
		
		
	}
}

int main() {
	hRemoteEvent = OpenEvent(SYNCHRONIZE, FALSE, TEXT("RemoteEvent"));

	hPauseEvent_S = OpenEventA(EVENT_ALL_ACCESS, FALSE, "PauseEventS");
	if (hPauseEvent_S == NULL) {
		printf("Erro ao criar evento hPauseEventS: %d\n", GetLastError());
		//return 1;
	}

	hFinishAll_Event = OpenEventA(EVENT_ALL_ACCESS, FALSE, "FinishAllEvent");
	if (hFinishAll_Event == NULL) {
		printf("Erro ao criar evento hFinishAllEvent: %d\n", GetLastError());
		//return 1;
	}

	semTxtSpace = OpenSemaphoreA(SEMAPHORE_ALL_ACCESS, FALSE, "SemaforoEspacoDisco");
	if (!semTxtSpace) {
		printf("Erro ao abrir semaforo do processo de exibir dados de sinalizacao ferroviaria: %d\n", GetLastError());
		//return 1;
	}

	HANDLE hExecuting_[2] = { hFinishAll_Event, hPauseEvent_S };
	HANDLE hMult_Obj[2] = { hFinishAll_Event, semTxtSpace };

	if (!hRemoteEvent) {
		printf("Erro ao abrir evento do processo de exibir dados de sinalizacao ferroviaria: %d\n", GetLastError());
		//return 1;
	}


	while (true) {
		DWORD finish = WaitForMultipleObjects(2, hExecuting_, FALSE, INFINITE);
		if ((finish - WAIT_OBJECT_0) == 0) {
			std::cout << "FINISH 1 RODAS " << std::endl;
			break;
		}
		if ((finish - WAIT_OBJECT_0) != 0 && (finish - WAIT_OBJECT_0) != 1) {
			printf("Visualizacao de Sinalizacao1: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
			//return 1;
		}
		
		DWORD dwWaitResult = WaitForMultipleObjects(2, hMult_Obj, FALSE, INFINITE);
		if ((dwWaitResult - WAIT_OBJECT_0) == 0) {
			std::cout << "FINISH 1 RODAS " << std::endl;
			break;
		}
		if ((dwWaitResult - WAIT_OBJECT_0) != 0 && (dwWaitResult - WAIT_OBJECT_0) != 1) {
			printf("Visualizacao de Sinalizacao2: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
			//return 1;
		}
		if (dwWaitResult - WAIT_OBJECT_0 == 1) {
			if (WaitForSingleObject(hRemoteEvent, INFINITE) == WAIT_OBJECT_0)
				consumeFirstMessage();
			//std::cout << "Consumi mensagem" << std::endl;
		}
	}

	CloseHandle(hRemoteEvent);
	CloseHandle(semTxtSpace);
	CloseHandle(hPauseEvent_S);
	CloseHandle(hFinishAll_Event);
	return 0;
}