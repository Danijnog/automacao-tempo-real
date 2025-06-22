#include <iostream>
#include <windows.h>
#include <sstream>
#include <string>
#include <vector>
#include <tchar.h>


HANDLE hPauseEvent_Q;          // Handle para controle de pausa/continuação
HANDLE hFinish_All_Event;      // Evento para encerrar todas as threads do programa
HANDLE hMailSlotRodasQ;        //Handle para o mailslot da tarefa de visualizaçao de rodas quentes
HANDLE hOpenMailslotVRQEvent;  //Handle para o evento que indica que o mailslot da tarefa Visualizacao de Rodas Quentes foi criado

 // Transforma a mensagem em um vetor de strings
static void msgToVector(const char* mensagem, std::vector<std::string>& msgVector) { 
	
	std::stringstream msg(mensagem);
	std::string campo;

	while (std::getline(msg, campo, ';')) {
		msgVector.push_back(campo);
	}
}

// Recebe uma mensagem do mailslot, formata e imprime no console
static void ReceiveAndPrint(void) {
	DWORD sizeNextMessage = 0;
	DWORD numMessage = 0;
	BOOL fResult = 0;
	std::stringstream oss;                 //Variavel para formatacao da mensagem que sera impressa
	std::vector<std::string> msgVector;    //Vetor contendo a mensagem para impressão 
	char msg[41];					  	   // Variavel para armazenar a mensagem lida do mailslot
	DWORD bytesLidos = 0;                  //Variavel para armazenar a quantidade de bytes lidos do mailslot

	fResult = GetMailslotInfo(hMailSlotRodasQ,  // mailslot handle 
		NULL,						            // no maximum message size 
		&sizeNextMessage,                       // size of next message 
		&numMessage,                            // number of messages 
		NULL);						            // no read time-out 
	if (!fResult)
	{
		printf("Erro em GetMailslotInfo: %d.\n", GetLastError());
		return;
	}
	else if (sizeNextMessage == MAILSLOT_NO_MESSAGE){
		return;
	}

	//Leitura do mailslot
	BOOL retorno = ReadFile(hMailSlotRodasQ,  // mailslot handle 
		msg,               // bufer para armazenamento da mensagem
		sizeNextMessage,    // numero maximo de bytes a serem lidos
		&bytesLidos,        // numero de bytes lidos
		NULL);              // ponteiro para estrutura overlapped (p/ operacao assincrona)
	if (!retorno) {
		printf("ReadFile falhou com erro: %d.\n", GetLastError());
	}
	else if (sizeNextMessage != bytesLidos) {
		std::cout << "Nao foram lidos todos os bytes." << " Bytes da mensagem: " << sizeNextMessage << " Bytes lidos: " << bytesLidos<< std::endl;
	}
	// Adicao do terminador nulo
	msg[bytesLidos] = '\0';
	// Transforma a mensagem em um vetor de strings para possibilitar a formatacao
	msgToVector(msg, msgVector);

	
	if (sizeNextMessage == 34) {
		// Formata e imprime mensagens RQ
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
		// Formata e imprime mensagens SF
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
		// Imprime mensagens recebidas de tamanho desconhecido
		std::cout << "Mensagem de tamanho desconhecido. Tam: " << sizeNextMessage << "Msg: " << msg << std::endl;
	}

	//Limpeza de stringstreams e vetores
	oss.str("");
	oss.clear();
	msgVector.clear();
}

int main() {
	std::cout << "Terminal de Visualizacao de Rodas Quentes" << std::endl;
	std::stringstream oss;                 //Variavel para formatacao da mensagem que sera impressa
	std::vector<std::string> msgVector;    //Vetor contendo a mensagem para impressão 
	std::stringstream msg;   //Variavel para armazenar a mensagem proveniente do mailslot
	DWORD bytesLidos = 0;    //Variavel para armazenar a quantidade de bytes lidos do mailslot
	
	hPauseEvent_Q = OpenEventA(EVENT_ALL_ACCESS, FALSE,  "PauseEventQ");
	if (hPauseEvent_Q == NULL) {
		printf("Erro ao criar evento hPauseEventQ: %d\n", GetLastError());
	}

	hFinish_All_Event = OpenEventA(EVENT_ALL_ACCESS, FALSE, "FinishAllEvent");
	if (hFinish_All_Event == NULL) {
		printf("Erro ao criar evento hFinishAllEvent: %d\n", GetLastError());
	}
	
	hOpenMailslotVRQEvent = OpenEventA(EVENT_ALL_ACCESS, FALSE, "OpenMailslotVRQEvent");
	if (hOpenMailslotVRQEvent == NULL) {
		printf("Erro ao criar evento hMailslotVRQEvent: %d\n", GetLastError());
	}

	SECURITY_ATTRIBUTES secAttribMailslot;
	secAttribMailslot.nLength = sizeof(SECURITY_ATTRIBUTES);
	secAttribMailslot.lpSecurityDescriptor = NULL;  // seguranca padrao
	secAttribMailslot.bInheritHandle = TRUE;

	hMailSlotRodasQ = CreateMailslotA("\\\\.\\mailslot\\mail_Vrodas_quentes", 0, 0, &secAttribMailslot); // (Name, MaxMessageSize, ReadTimeout, SecurityAtributes)
	if (hMailSlotRodasQ == INVALID_HANDLE_VALUE) {
		printf("Erro ao criar mailslot hMailSlotRodasQ: %d\n", GetLastError());
	}
	SetEvent(hOpenMailslotVRQEvent);

	HANDLE h_Executing[2] = { hFinish_All_Event, hPauseEvent_Q };
	int contaloop = 0;

	while (TRUE) {
		// Checa se deve encerrar ou pausar
		DWORD finish = WaitForMultipleObjects(2, h_Executing, FALSE, INFINITE);
		if ((finish - WAIT_OBJECT_0) == 0) {
			break;
		}
		if ((finish - WAIT_OBJECT_0) != 0 && (finish - WAIT_OBJECT_0) != 1) {
			printf("Terminal VRQ: Erro nos objetos sincronizacao de execucao: %d\n", GetLastError());
			return 1;
		}

		//Leitura do mailslot e print no console
		ReceiveAndPrint();
	}

	CloseHandle(hPauseEvent_Q);
	CloseHandle(hFinish_All_Event);
	CloseHandle(hOpenMailslotVRQEvent);
	CloseHandle(hMailSlotRodasQ);

	return 0;
}

