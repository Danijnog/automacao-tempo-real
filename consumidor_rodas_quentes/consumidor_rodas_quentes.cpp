// consumidor_rodas_quentes.cpp : Este arquivo contém a função 'main'. A execução do programa começa e termina ali.
//

#include <iostream>
#include <windows.h>
#include <sstream>

HANDLE hPauseEvent_Q; // Handle para controle de pausa/continuação
HANDLE hFinish_All_Event; // Evento para encerrar todas as threads do programa

int main() {
	SYSTEMTIME st;
	GetLocalTime(&st);
	std::cout << "Visualizacao de rodas quentes - TESTE" << std::endl;
	std::ostringstream oss;
	std::string message;
	
	hPauseEvent_Q = OpenEventA(EVENT_ALL_ACCESS, FALSE,  "PauseEventQ");
	if (hPauseEvent_Q == NULL) {
		printf("Erro ao criar evento hPauseEventQ: %d\n", GetLastError());
		//return 1;
	}

	hFinish_All_Event = OpenEventA(EVENT_ALL_ACCESS, FALSE, "FinishAllEvent");
	if (hFinish_All_Event == NULL) {
		printf("Erro ao criar evento hFinishAllEvent: %d\n", GetLastError());
		//return 1;
	}
	
	HANDLE h_Executing[2] = { hFinish_All_Event, hPauseEvent_Q };

	while (TRUE) {
		DWORD finish = WaitForMultipleObjects(2, h_Executing, FALSE, INFINITE);
		if ((finish - WAIT_OBJECT_0) == 0) {
			std::cout << "FINISH 1 RODAS " << std::endl;
			break;
		}
		if ((finish - WAIT_OBJECT_0) != 0 && (finish - WAIT_OBJECT_0) != 1) {
			printf("Visualizacao de Rodas Quentes: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
			//return 1;
		}
		oss << st.wHour << ":" << st.wMinute << ":" << st.wSecond << ":" << st.wMilliseconds
			<< " NSEQ: " << " ######## " << " REMOTA " << " ## " <<
			" FALHA DE HARDWARE ";
		message = oss.str();
		oss.str("");
		oss.clear();
		std::cout << message << std::endl;
		Sleep(1000);

		oss << st.wHour << ":" << st.wMinute << ":" << st.wSecond << " NSEQ: "
			<< " ######## " << " DETECTOR " << " ######## " << " TEMP. DENTRO DA FAIXA";
		message = oss.str();
		std::cout << message << std::endl;
		oss.str("");
		oss.clear();
		Sleep(3000);
	}

	CloseHandle(hPauseEvent_Q);
	CloseHandle(hFinish_All_Event);
	return 0;
}

