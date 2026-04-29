#include <iostream>
#include <vector>
#include <portaudio.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>
#include "vosk_api.h"

using namespace std;
using json = nlohmann::json;

#define SAMPLE_RATE 16000
#define FRAMES 4000


json getConfig(){
	ifstream file("config.json");
	json config;

	file >> config;
	return config;
}

bool contains(const vector<string>& vec, const string& target) {
    return find(vec.begin(), vec.end(), target) != vec.end();
}

string askAI(const string& cmd) {
    array<char, 4096> buffer;
    string result;

    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    // 🔥 Парсим JSON ответа ChatGPT
    try {
        auto j = json::parse(result);

        return j["choices"][0]["message"]["content"].get<string>();
    } catch (...) {
        return "Ошибка";
        cout << "Проверьте наличие кредитов на аккаунте в OpenAI" << endl;
    }
}

string gptRedact(const string command, const vector<string> commands){
	json config = getConfig();

    string gptPrompt = config["prompt"].get<string>() + " [";
    for (string cmd : commands){
    	gptPrompt += cmd;
    	if (commands[commands.size()-1] == cmd){break;}
    	gptPrompt += ", ";
    }
    gptPrompt = gptPrompt + "]; Фраза: " + command;
    string key = config["gpt_key"];

    string p = "curl -s https://api.openai.com/v1/chat/completions "
				"-H \"Content-Type: application/json\" "
				"-H \"Authorization: Bearer " + key + "\" "
				"-d \"{"
				"\\\"model\\\":\\\"gpt-5.4-mini\\\","
				"\\\"messages\\\":[{\\\"role\\\":\\\"user\\\",\\\"content\\\":\\\"" + gptPrompt + "\\\"}]"
				"}\"";;

    return p;
}

void say(const string text){}

void doCommand(const string cmd, const json config, const bool debug, const vector<string> commands){
	bool voice  = config["voice"].get<bool>();
	if (contains(commands, cmd)){
		if (debug){cout << "Делаю команду \"" << cmd << "\"..." << endl;}
			
		if (cmd == "привет"){
			cout << "пр чд кд че мне делать натуре" << endl;
		}else if(cmd == "открой консоль"){
			system("osascript -e 'tell application \"Terminal\" to do script \"\"'");
		}

		json config = getConfig();
		if (voice){say(cmd);}

		if (debug){cout << "Успешно!" << endl;}

	}else{if (voice){say("Команда отсутствует.");}}

	
}

int main() {
	json config = getConfig();
	bool debug = config["debug"].get<bool>();
	vosk_set_log_level(-1);

	vector<string> commands = {"привет", "открой консоль"};
	string model_path = "models/" + config["model"].get<string>();

	cout << "Successfully started!" << endl;

	if (debug){
    	cout << model_path << endl;
		for (auto& [key, value] : config.items()) {
        	cout << "CONFIG: " <<  key << " = " << value << endl;
    	}
	}

    Pa_Initialize();
    PaStream *stream;
    Pa_OpenDefaultStream(&stream, 1, 0, paInt16, SAMPLE_RATE, FRAMES, NULL, NULL);
    Pa_StartStream(stream);

    
    VoskModel *model = vosk_model_new(model_path.c_str());
    VoskRecognizer *rec = vosk_recognizer_new(model, SAMPLE_RATE);

    int16_t buffer[FRAMES];

    while (true) {
    	Pa_ReadStream(stream, buffer, FRAMES);
	
    	if (vosk_recognizer_accept_waveform(rec, (const char*)buffer, sizeof(buffer))) {
    	    string result = vosk_recognizer_result(rec);
	
    	    size_t start = result.find("\"text\" : \"");
    	    if (start != string::npos) {
    	        start += 10;
    	        size_t end = result.find("\"", start);
	
    	        string cmd = result.substr(start, end - start);
				
    	        if (!cmd.empty()) {
    	        	if (debug){cout << "SAYS: " << cmd << endl;}
    	        	// transform(cmd.begin(), cmd.end(), cmd.begin(), [](unsigned char c){ return tolower(c); }); // Пиздецовое преобразование в маленький регистр

    	            if (!contains(commands, cmd)){
    	            	if (config["gpt_redact"].get<bool>()){
    	          			cmd = askAI(gptRedact(cmd, commands).c_str());
    	          			if (debug){cout << "GPT_COMMAND: " << cmd << endl;}
    	            	}
    	        	}
    	        	
    	            doCommand(cmd, config, debug, commands);
    	        }
    	    }
    	}
	}

    Pa_StopStream(stream);
    Pa_Terminate();

    vosk_recognizer_free(rec);
    vosk_model_free(model);

    return 0;
}