#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

HANDLE hPauseEvent_S; // Handle para controle de pausa/continuação
HANDLE hFinishAll_Event; // Evento para encerrar todas as threads do programa

// Estado do sensor: devemos ter pelo menos 20 estados reais aleatórios possíveis
std::vector<std::string> states = {
	"Tunel", "Ponte", "Passagem de Nivel", "Pare", "Siga",
	"Linha Impedida", "Buzine", "Desvio Atuado", "Desvio Livre", "Livre",
	"Manobra Proibida", "Velocidade Reduzida", "Ponte", "Homens Trabalhando", "Advertencia de Parada Total",
	"Termino de Precaucao", "Reassuma Velocidade", "Manutencao Mecanica", "Limite de Manobra", "Inicio de CTC"
};

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

	HANDLE hExecuting_[2] = { hFinishAll_Event, hPauseEvent_S };
	

	while (true) {
		DWORD finish = WaitForMultipleObjects(2, hExecuting_, FALSE, INFINITE);
		if ((finish - WAIT_OBJECT_0) == 0) {
			std::cout << "FINISH 1 RODAS " << std::endl;
			break;
		}
		if ((finish - WAIT_OBJECT_0) != 0 && (finish - WAIT_OBJECT_0) != 1) {
			printf("Visualizacao de Sinalizacao1: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());

		}
		int Estado = rand() % 20;
		oss << st.wHour << ":" << st.wMinute << ":" << st.wSecond << ":" << st.wMilliseconds
			<< " NSEQ: " << " ######## " << " REMOTA " << " ## " <<
			" SENSOR: " << "########" << " ESTADO: " << states[Estado];
		message = oss.str();
		oss.str("");
		oss.clear();
		std::cout << message << std::endl;

		
		Sleep(5000);
		
	}

	
	CloseHandle(hPauseEvent_S);
	CloseHandle(hFinishAll_Event);
	return 0;
}