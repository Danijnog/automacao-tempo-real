#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

HANDLE hRemoteEvent;
HANDLE semTxtSpace;

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
	std::string file_path = "../../TP_ATR/sinalizacao.txt";
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
		for (size_t i = 1; i < lines.size(); i++) // Loop com o tipo size_t é o ideal para lidar com o tamanho de uma string em c++
			outfile << lines[i] << "\n";
		
		outfile.close();
		semTxtSpace = OpenSemaphore(SEMAPHORE_MODIFY_STATE, NULL, TEXT("SemaforoEspacoDisco"));
		if (semTxtSpace) {
			ReleaseSemaphore(semTxtSpace, 1, NULL);
			CloseHandle(semTxtSpace);
		}
	}
}

int main() {
	hRemoteEvent = OpenEvent(SYNCHRONIZE, FALSE, TEXT("RemoteEvent"));
	if (!hRemoteEvent) {
		std::cerr << "Erro ao abrir evento do processo de exibir dados de sinalizacao ferroviaria." << std::endl;
		return 1;
	}

	while (true) {
		DWORD dwWaitResult = WaitForSingleObject(hRemoteEvent, INFINITE);
		if (dwWaitResult == WAIT_OBJECT_0) {
			consumeFirstMessage();
		}
	}
	CloseHandle(hRemoteEvent);
	return 0;
}