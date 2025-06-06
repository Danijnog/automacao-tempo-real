// consumidor_rodas_quentes.cpp : Este arquivo contém a função 'main'. A execução do programa começa e termina ali.
//

#include <iostream>
#include <windows.h>
#include <sstream>

int main() {
	SYSTEMTIME st;
	GetLocalTime(&st);
	std::cout << "Visualizacao de rodas quentes - TESTE" << std::endl;
	std::ostringstream oss;
	std::string message;

	while (1) {
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
}

