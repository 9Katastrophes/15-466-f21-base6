
#include "Connection.hpp"

#include "hex_dump.hpp"

#include <chrono>
#include <stdexcept>
#include <iostream>
#include <cassert>
#include <unordered_map>
#include <glm/gtc/type_ptr.hpp>
#include <random>
#include <time.h>
#include <bit>

glm::u8vec4 generate_random_color() {
	//random number generation code inspiration taken from here: https://stackoverflow.com/questions/686353/random-float-number-generation
	int r = rand() % 256;
	int g = rand() % 256;
	int b = rand() % 256;
	int a = 255;

	return glm::u8vec4(r, g, b, a);
}

bool in_bounds(glm::vec2 pos, std::pair<glm::vec2, glm::vec2> bounds) {
	float min_x = std::min(bounds.first.x, bounds.second.x);
	float max_x = std::max(bounds.first.x, bounds.second.x);
	float min_y = std::min(bounds.first.y, bounds.second.y);
	float max_y = std::max(bounds.first.y, bounds.second.y);

	if (min_x < pos.x && pos.x < max_x && min_y < pos.y && pos.y < max_y)
		return true;
	return false;
}

#ifdef _WIN32
extern "C" { uint32_t GetACP(); }
#endif
int main(int argc, char **argv) {
#ifdef _WIN32
	{ //when compiled on windows, check that code page is forced to utf-8 (makes file loading/saving work right):
		//see: https://docs.microsoft.com/en-us/windows/apps/design/globalizing/use-utf8-code-page
		uint32_t code_page = GetACP();
		if (code_page == 65001) {
			std::cout << "Code page is properly set to UTF-8." << std::endl;
		} else {
			std::cout << "WARNING: code page is set to " << code_page << " instead of 65001 (UTF-8). Some file handling functions may fail." << std::endl;
		}
	}

	//when compiled on windows, unhandled exceptions don't have their message printed, which can make debugging simple issues difficult.
	try {
#endif

	//------------ argument parsing ------------

	if (argc != 2) {
		std::cerr << "Usage:\n\t./server <port>" << std::endl;
		return 1;
	}

	//------------ initialization ------------

	Server server(argv[1]);
	srand(time(NULL));

	//------------ main loop ------------
	constexpr float ServerTick = 1.0f / 60.0f;

	//server state:
	//key regions
	std::vector<std::pair<glm::vec2, glm::vec2>> key_bounds; //the area of each key is defined by the upper left corner and bottom right corner
	glm::vec2 court_radius = glm::vec2(16.0f, 4.0f);
	const float key_radius = 2.0f;

	for (size_t i=0;i<8;i++) {
		glm::vec2 center = glm::vec2(-court_radius.x + key_radius * (2.0f * i + 1.0f), 0.0f);
		glm::vec2 radius = glm::vec2(key_radius, court_radius.y + 2.0f * key_radius);

		key_bounds.emplace_back(std::pair(center - radius, center + radius));
	}

	//per-client state:
	struct PlayerInfo {
		PlayerInfo() {
			static uint32_t next_player_id = 1;
			next_player_id += 1;
		}
		glm::vec2 position = glm::vec2(0.0f);
		glm::u8vec4 color = glm::u8vec4(255, 255, 255, 255);

		uint32_t space_presses = 0;
	};
	std::unordered_map< Connection *, PlayerInfo > players;

	while (true) {
		static auto next_tick = std::chrono::steady_clock::now() + std::chrono::duration< double >(ServerTick);
		//process incoming data from clients until a tick has elapsed:
		while (true) {
			auto now = std::chrono::steady_clock::now();
			double remain = std::chrono::duration< double >(next_tick - now).count();
			if (remain < 0.0) {
				next_tick += std::chrono::duration< double >(ServerTick);
				break;
			}
			server.poll([&](Connection *c, Connection::Event evt){
				if (evt == Connection::OnOpen) {
					//client connected:

					//create some player info for them:
					players.emplace(c, PlayerInfo());
					players[c].color = generate_random_color();

				} else if (evt == Connection::OnClose) {
					//client disconnected:

					//remove them from the players list:
					auto f = players.find(c);
					assert(f != players.end());
					players.erase(f);


				} else { assert(evt == Connection::OnRecv);
					//got data from client:
					//std::cout << "got bytes:\n" << hex_dump(c->recv_buffer); std::cout.flush();

					//look up in players list:
					auto f = players.find(c);
					assert(f != players.end());
					PlayerInfo &player = f->second;

					//handle messages from client:
					while (c->recv_buffer.size() >= 10) {
						//expecting 10-byte messages 'b' (space count) (position.x) (position.y)
						char type = c->recv_buffer[0];
						if (type != 'b') {
							std::cout << " message of non-'b' type received from client!" << std::endl;
							//shut down client connection:
							c->close();
							return;
						}

						uint8_t space_count = static_cast<uint8_t>(c->recv_buffer[1]);
						player.space_presses += space_count;

						float player_pos_x;
						float player_pos_y;

						std::memcpy(&player_pos_x, &c->recv_buffer[2], 4);
						std::memcpy(&player_pos_y, &c->recv_buffer[6], 4);
	
						player.position.x = player_pos_x;
						player.position.y = player_pos_y;						

						c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 10);
					}
				}
			}, remain);
		}

		//update current game state
		std::string status_message = "";
		std::string keys_pressed = "k";
		for (auto &[c, player] : players) {
			(void)c; //work around "unused variable" warning on whatever version of g++ github actions is running
			while(player.space_presses > 0) {
				//check if the press was on a key
				//if so, add the key id to keys_pressed
				for (size_t i=0;i<key_bounds.size();i++) {
					std::pair<glm::vec2, glm::vec2> bounds = key_bounds[i];
					if (in_bounds(player.position, bounds)) {
						keys_pressed += std::to_string(i);
					}
				}
				player.space_presses -= 1;
			}
			status_message += "(" + std::to_string(player.position.x) + "," + std::to_string(player.position.y) + ")"; //player position
			status_message += "<" + std::to_string(player.color[0]) + "," + std::to_string(player.color[1]) + "," + std::to_string(player.color[2]) + "," + std::to_string(player.color[3]) + ">";
		}
		status_message += keys_pressed;
		//std::cout << status_message << std::endl; //DEBUG

		//send updated game state to all clients
		for (auto &[c, player] : players) {
			(void)player; //work around "unused variable" warning on whatever g++ github actions uses
			//send an update starting with 'm', a 24-bit size, and a blob of text:
			c->send('m');
			c->send(uint8_t(status_message.size() >> 16));
			c->send(uint8_t((status_message.size() >> 8) % 256));
			c->send(uint8_t(status_message.size() % 256));
			c->send_buffer.insert(c->send_buffer.end(), status_message.begin(), status_message.end());
		}
	}


	return 0;

#ifdef _WIN32
	} catch (std::exception const &e) {
		std::cerr << "Unhandled exception:\n" << e.what() << std::endl;
		return 1;
	} catch (...) {
		std::cerr << "Unhandled exception (unknown type)." << std::endl;
		throw;
	}
#endif
}
