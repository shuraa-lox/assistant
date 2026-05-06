#include <iostream>
#include <sstream>
#include <vector>
#include <portaudio.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <SFML/Audio.hpp>
#include <random>
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

bool contains(const vector<string>& vec, const string& target) { //gpt
    return find(vec.begin(), vec.end(), target) != vec.end();
}

bool containss(const string& text, const string& target) { //gpt
    return text.find(target) != string::npos;
}

string removeWordClean(const string& text, const string& word) { //gpt
    stringstream ss(text);
    string result, temp;

    while (ss >> temp) {
        if (temp != word) {
            if (!result.empty()) result += " ";
            result += temp;
        }
    }

    return result;
}

string askAI(const string& cmd) { //gpt
    array<char, 4096> buffer;
    string result;

    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    try {
        auto j = json::parse(result);

        return j["choices"][0]["message"]["content"].get<string>();
    } catch (...) {
    	cout << "Скорее всего у вас кончились кредиты на аккаунет OpenAi. Отключите \"gpt_redact\" в конфиге либо пополните кредиты." << endl;
        return "Ошибка";
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
				"}\"";

    return p;
}


void say(const string& path, PaStream* stream) { //https://ttsmp3.com/ai
    sf::Music music;
    if (!music.openFromFile(path))
        return;
    Pa_StopStream(stream);

    music.play();

    while (music.getStatus() == sf::SoundSource::Status::Playing) {
        sf::sleep(sf::milliseconds(100));
    }

    Pa_StartStream(stream);
}

void randomOK(PaStream* stream, string soundsPath){
    random_device rd;
    mt19937 gen(rd());   
    uniform_int_distribution<> dist(1, 2);
	if (dist(gen) == 1){
		say(soundsPath + "did.mp3", stream);
		return;
	}
	say(soundsPath + "did2.mp3", stream);
}

void doCommand(const string cmd, bool& voice, const bool debug, PaStream *stream, const vector<string> commands, bool& isPaused, bool& running){
	json config = getConfig();

	string name = config["name"].get<string>();
	string voiceActor = config["voiceActor"].get<string>();
	string soundsPath = "anwsers/" + voiceActor + "/";

	if (contains(commands, cmd)){
		if (isPaused){
			if (cmd == name){
				cout << "Я снова здесь. Что мне делать?" << endl;
				isPaused = false;
				return;
			}
			isPaused = true;
			return;
		}

		if (debug){cout << "Делаю команду \"" << cmd << "\"..." << endl;}

		if (cmd == "привет"){
			cout << "пр че мне делать" << endl;
			if (voice){say(soundsPath + "hi.mp3", stream);}

		}else if(cmd == "открой консоль"){
			system("osascript -e 'tell application \"Terminal\" to do script \"\"' > /dev/null");
			if (voice){randomOK(stream, soundsPath);}

		}else if(cmd == "молчи"){
			if (voice){
				cout << "Ладно" << endl;
				voice = false;
			}else{cout << "Молчу уже мразь" << endl;}

		}else if(cmd == "говори"){
			if (!voice){
				cout << "Акей!" << endl;
				voice = true;
				say(soundsPath + "okay.mp3", stream);
				//return;
			}else{
				cout << "Уже!" << endl;
				say(soundsPath + "speaking.mp3", stream);
			}
			
		}else if(cmd == "матвей"){
			cout << "Вітюк вітючок вітючочок" << endl;
			if(voice){say(soundsPath + "vityuk.mp3", stream);}

		}else if(cmd == "сон"){
			cout << "Засыпаю" << endl;
			isPaused = true;
			//return;

		}else if(cmd == "выключись"){
			cout << "Выключаюсь..." << endl;
			if (voice){say(soundsPath + "bye.mp3", stream);}
			running = false;
		}

		if (debug){cout << "Успешно!" << endl;}
		
	}else{
		if (voice && !isPaused){
			cout << "Команда отсутствует." << endl;
			say(soundsPath + "err.mp3", stream);
		}
	}

}

int main() {
	json config = getConfig();
	bool debug = config["debug"].get<bool>();
	bool voice = config["voice"].get<bool>();
	vosk_set_log_level(-1);

	vector<string> commands = {"привет", "открой консоль", "выключись", "сон", "матвей", "говори", "молчи", config["name"].get<string>()};
	string name = config["name"].get<string>();
	bool isPaused = false;
	bool running = true;
	string model_path = config["model"].get<string>();

	cout << "Успешно запущено!" << endl;

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

    while (running) {

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

    	        	if (containss(cmd, name)){
    	        		if (cmd == name){
    	        			doCommand(name, voice, debug, stream, commands, isPaused, running);
    	        			continue;
    	        		}
    	     			cmd = removeWordClean(cmd, name);
    	        		doCommand(name, voice, debug, stream, commands, isPaused, running);
    	        	}

    	            if (!contains(commands, cmd)){
    	            	if (config["gpt_redact"].get<bool>()){
    	          			cmd = askAI(gptRedact(cmd, commands).c_str());
    	          			if (debug){cout << "GPT_COMMAND: " << cmd << endl;}
    	            	}
    	        	}
    	            doCommand(cmd, voice, debug, stream, commands, isPaused, running);
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