// consumidor_rodas_quentes.cpp : Este arquivo contém a função 'main'. A execução do programa começa e termina ali.
//

#include <iostream>
#include <windows.h>
#include <sstream>
#include <string>
#include <vector>
#include <tchar.h>


HANDLE hPauseEvent_Q; // Handle para controle de pausa/continuação
HANDLE hFinish_All_Event; // Evento para encerrar todas as threads do programa
HANDLE hMailSlotRodasQ; //Handle para o mailslot da tarefa de visualizaçao de rodas quentes
HANDLE hOpenMailslotVRQEvent;  //Handle para o evento que indica que o mailslot da tarefa Visualizacao de Rodas Quentes foi criado


static void msgToVector(const char* mensagem, std::vector<std::string>& msgVector) {  // Transforma a mensagem em um vetor de strings
	/*
	// Converter de wchar_t* para std::string (ANSI)
	int len = WideCharToMultiByte(CP_ACP, 0, mensagem, -1, NULL, 0, NULL, NULL);
	if (len == 0) {
		std::cout << "Erro na conversão: GetLastError() = " << GetLastError() << std::endl;
		return;
	}
	std::string converted(len, 0);
	WideCharToMultiByte(CP_ACP, 0, mensagem, -1, &converted[0], len, NULL, NULL);

	std::stringstream msg(converted);
	*/
	std::stringstream msg(mensagem);
	std::string campo;
	
	//std::cout << "Entrei em msgToVector" << std::endl;
	//std::cout << "Conteúdo da mensagem: [" << msg.str() << "]" << std::endl;
	while (std::getline(msg, campo, ';')) {
		msgVector.push_back(campo);
		//std::cout << "Campo add vector: " << campo << std::endl;
	}
}

static void ReceiveAndPrint(void) {
	DWORD sizeNextMessage = 0;
	DWORD numMessage = 0;
	BOOL fResult = 0;
	std::stringstream oss;  //Variavel para formatacao da mensagem que sera impressa
	std::vector<std::string> msgVector;    //Vetor contendo a mensagem para impressão 
	//LPTSTR msg = new TCHAR[41];   //Variavel para armazenar a mensagem proveniente do mailslot
	char msg[41];
	DWORD bytesLidos = 0;    //Variavel para armazenar a quantidade de bytes lidos do mailslot

	//msg[0] = 0;  // Inicializa a variavel vazia

	fResult = GetMailslotInfo(hMailSlotRodasQ,  // mailslot handle 
		NULL,						  // no maximum message size 
		&sizeNextMessage,                   // size of next message 
		&numMessage,                    // number of messages 
		NULL);						  // no read time-out 
	if (!fResult)
	{
		printf("Erro em GetMailslotInfo: %d.\n", GetLastError());
		//return;
	}
	else if (sizeNextMessage == MAILSLOT_NO_MESSAGE){
		//std::cout << "No message" << std::endl;
		return;
	}
	else if (sizeNextMessage == 34) {
		// Leitura do mailslot
		BOOL retorno = ReadFile(hMailSlotRodasQ,  // mailslot handle 
			msg,               // bufer para armazenamento da mensagem
			sizeNextMessage,    // numero maximo de bytes a serem lidos
			&bytesLidos,        // numero de bytes lidos
			NULL);              // ponteiro para estrutura overlapped (p/ operacao assincrona)
		if (!retorno){
			printf("ReadFile failed with %d.\n", GetLastError());
		}
		else if (sizeNextMessage != bytesLidos) {
			//std::cout << "bytes << std::endl;
		}
		// Adiciona terminador nulo
		msg[bytesLidos] = '\0';
		// Formatacao e print da mensagem
		msgToVector(msg, msgVector);
		if (msgVector.size() < 5) {
			std::cout << "Vetor 34 sem tamanho adequado: " << msgVector.size() << std::endl;
		}
		else {
			oss << msgVector[4] << " NSEQ: " << msgVector[0]
				<< " DETECTOR: " << msgVector[2];
			if (std::stoi(msgVector[3]) == 0) {
				oss << " TEMP. DENTRO DA FAIXA";
			}
			else if (std::stoi(msgVector[3]) == 1) {
				oss << " RODA QUENTE DETECTADA";
			}
			std::cout << oss.str() << std::endl;
		}
		
	}
	else if (sizeNextMessage == 40) {
		// Leitura do mailslot
		BOOL retorno = ReadFile(hMailSlotRodasQ,  // mailslot handle 
			msg,               // bufer para armazenamento da mensagem
			sizeNextMessage,    // numero maximo de bytes a serem lidos
			&bytesLidos,        // numero de bytes lidos
			NULL);              // ponteiro para estrutura overlapped (p/ operacao assincrona)
		if (!retorno) {
			printf("ReadFile failed with %d.\n", GetLastError());
		}
		// Adiciona terminador nulo
		msg[bytesLidos] = '\0';
		// Formatacao e print da mensagem
		//std::cout << "Conteúdo da mensagem: " << msg << std::endl;
		msgToVector(msg, msgVector);
		if (msgVector.size() < 7) {
			std::cout << "Vetor 40 sem tamanho adequado: " << msgVector.size() << std::endl;
		}
		else {
			oss << msgVector[6] << " NSEQ: " << msgVector[0]
				<< " REMOTA: " << msgVector[3] << " FALHA DE HARDWARE ";
			std::cout << oss.str() << std::endl;
		}
		
	}
	else {
		// Leitura do mailslot
		BOOL retorno = ReadFile(hMailSlotRodasQ,  // mailslot handle 
			msg,               // bufer para armazenamento da mensagem
			sizeNextMessage,    // numero maximo de bytes a serem lidos
			&bytesLidos,        // numero de bytes lidos
			NULL);              // ponteiro para estrutura overlapped (p/ operacao assincrona)
		if (!retorno) {
			printf("ReadFile failed with %d.\n", GetLastError());
		}
		// Adiciona terminador nulo
		msg[bytesLidos] = '\0';
		// Print  da mensagem
		std::cout << "Mensagem de tamanho desconhecido. Tam: " << sizeNextMessage << "Msg: " << msg << std::endl;
	}

	//Limpeza de stringstreams e vetores
	oss.str("");
	oss.clear();
	msgVector.clear();
}

int main() {
	std::cout << "Visualizacao de rodas quentes - TESTE" << std::endl;
	std::stringstream oss;  //Variavel para formatacao da mensagem que sera impressa
	std::vector<std::string> msgVector;    //Vetor contendo a mensagem para impressão 
	std::stringstream msg;   //Variavel para armazenar a mensagem proveniente do mailslot
	DWORD bytesLidos = 0;    //Variavel para armazenar a quantidade de bytes lidos do mailslot
	
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
	
	hOpenMailslotVRQEvent = OpenEventA(EVENT_ALL_ACCESS, FALSE, "OpenMailslotVRQEvent");
	if (hOpenMailslotVRQEvent == NULL) {
		printf("Erro ao criar evento hMailslotVRQEvent: %d\n", GetLastError());
		//return 1;
	}

	SECURITY_ATTRIBUTES secAttribMailslot;
	secAttribMailslot.nLength = sizeof(SECURITY_ATTRIBUTES);
	secAttribMailslot.lpSecurityDescriptor = NULL;  // seguranca padrao
	secAttribMailslot.bInheritHandle = TRUE;

	hMailSlotRodasQ = CreateMailslotA("\\\\.\\mailslot\\mail_Vrodas_quentes", 0, 0, &secAttribMailslot); // (Name, MaxMessageSize, ReadTimeout, SecurityAtributes)
	if (hMailSlotRodasQ == INVALID_HANDLE_VALUE) {
		printf("Erro ao criar mailslot hMailSlotRodasQ: %d\n", GetLastError());
		//return 1;
	}
	SetEvent(hOpenMailslotVRQEvent);

	HANDLE h_Executing[2] = { hFinish_All_Event, hPauseEvent_Q };
	int contaloop = 0;

	while (TRUE) {
		//std::cout << "Executa while: " << contaloop << std::endl;
		//contaloop += 1;

		// Checa se deve encerrar ou pausar
		DWORD finish = WaitForMultipleObjects(2, h_Executing, FALSE, INFINITE);
		if ((finish - WAIT_OBJECT_0) == 0) {
			std::cout << "FINISH 1 RODAS " << std::endl;
			break;
		}
		if ((finish - WAIT_OBJECT_0) != 0 && (finish - WAIT_OBJECT_0) != 1) {
			printf("Visualizacao de Rodas Quentes: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
			return 1;
		}

		//Leitura do mailslot e print no console
		ReceiveAndPrint();
	}

	CloseHandle(hPauseEvent_Q);
	CloseHandle(hFinish_All_Event);
	CloseHandle(hOpenMailslotVRQEvent);
	CloseHandle(hMailSlotRodasQ);

	system("pause"); // Pausa a execução

	return 0;
}

