#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <iostream>

Load< Sound::Sample > pop_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("pop.wav"));
});
Load< Sound::Sample > chirp_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("chirp.wav"));
});
Load< Sound::Sample > boop_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("boop.wav"));
});
Load< Sound::Sample > quack_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("quack.wav"));
});
Load< Sound::Sample > drum_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("drum.wav"));
});
Load< Sound::Sample > squeak_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("squeak.wav"));
});
Load< Sound::Sample > bop_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("bop.wav"));
});
Load< Sound::Sample > bark_sample(LoadTagDefault, []() -> Sound::Sample const * {
	return new Sound::Sample(data_path("bark.wav"));
});

PlayMode::PlayMode(Client &client_) : client(client_) {
	srand(time(NULL));

	key_samples.emplace_back(*pop_sample);
	key_samples.emplace_back(*chirp_sample);
	key_samples.emplace_back(*boop_sample);
	key_samples.emplace_back(*quack_sample);
	key_samples.emplace_back(*drum_sample);
	key_samples.emplace_back(*squeak_sample);
	key_samples.emplace_back(*bop_sample);
	key_samples.emplace_back(*bark_sample);

	//----- allocate OpenGL resources -----
	//taken from game0 code!
	{ //vertex buffer:
		glGenBuffers(1, &vertex_buffer);
		//for now, buffer will be un-filled.

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //vertex array mapping buffer for color_texture_program:
		//ask OpenGL to fill vertex_buffer_for_color_texture_program with the name of an unused vertex array object:
		glGenVertexArrays(1, &vertex_buffer_for_color_texture_program);

		//set vertex_buffer_for_color_texture_program as the current vertex array object:
		glBindVertexArray(vertex_buffer_for_color_texture_program);

		//set vertex_buffer as the source of glVertexAttribPointer() commands:
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

		//set up the vertex array object to describe arrays of PlayMode::Vertex:
		glVertexAttribPointer(
			color_texture_program.Position_vec4, //attribute
			3, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 0 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Position_vec4);
		//[Note that it is okay to bind a vec3 input to a vec4 attribute -- the w component will be filled with 1.0 automatically]

		glVertexAttribPointer(
			color_texture_program.Color_vec4, //attribute
			4, //size
			GL_UNSIGNED_BYTE, //type
			GL_TRUE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Color_vec4);

		glVertexAttribPointer(
			color_texture_program.TexCoord_vec2, //attribute
			2, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte *)0 + 4*3 + 4*1 //offset
		);
		glEnableVertexAttribArray(color_texture_program.TexCoord_vec2);

		//done referring to vertex_buffer, so unbind it:
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//done setting up vertex array object, so unbind it:
		glBindVertexArray(0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //solid white texture:
		//ask OpenGL to fill white_tex with the name of an unused texture object:
		glGenTextures(1, &white_tex);

		//bind that texture object as a GL_TEXTURE_2D-type texture:
		glBindTexture(GL_TEXTURE_2D, white_tex);

		//upload a 1x1 image of solid white to the texture:
		glm::uvec2 size = glm::uvec2(1,1);
		std::vector< glm::u8vec4 > data(size.x*size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

		//set filtering and wrapping parameters:
		//(it's a bit silly to mipmap a 1x1 texture, but I'm doing it because you may want to use this code to load different sizes of texture)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		//since texture uses a mipmap and we haven't uploaded one, instruct opengl to make one for us:
		glGenerateMipmap(GL_TEXTURE_2D);

		//Okay, texture uploaded, can unbind it:
		glBindTexture(GL_TEXTURE_2D, 0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}
}

PlayMode::~PlayMode() {

	//----- free OpenGL resources -----
	//taken from game0 code!
	glDeleteBuffers(1, &vertex_buffer);
	vertex_buffer = 0;

	glDeleteVertexArrays(1, &vertex_buffer_for_color_texture_program);
	vertex_buffer_for_color_texture_program = 0;

	glDeleteTextures(1, &white_tex);
	white_tex = 0;
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	//taken from game0 code!
	if (evt.type == SDL_MOUSEMOTION) {
		//convert mouse from window pixels (top-left origin, +y is down) to clip space ([-1,1]x[-1,1], +y is up):
		glm::vec2 clip_mouse = glm::vec2(
			(evt.motion.x + 0.5f) / window_size.x * 2.0f - 1.0f,
			(evt.motion.y + 0.5f) / window_size.y *-2.0f + 1.0f
		);
		position.x = (clip_to_court * glm::vec3(clip_mouse, 1.0f)).x;
		position.y = (clip_to_court * glm::vec3(clip_mouse, 1.0f)).y;
	}
    else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_SPACE) {
    	space.downs += 1;
		space.pressed = true;
		return true;
    }
    else if (evt.type == SDL_KEYUP && evt.key.keysym.sym == SDLK_SPACE) {
    	space.pressed = false;
    	return true;
    }

	return false;
}

void PlayMode::update(float elapsed) {

	//queue data for sending to server:
	//send a 10-byte message of type 'b':
	client.connections.back().send('b');				//1 byte
	client.connections.back().send(space.downs);		//1 byte
	client.connections.back().send(position.x);			//4 bytes
	client.connections.back().send(position.y);			//4 bytes

	//reset button press counters:
	space.downs = 0;

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush();
			//expecting message(s) like 'm' + 3-byte length + length bytes of text:
			while (c->recv_buffer.size() >= 4) {
				//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush();
				char type = c->recv_buffer[0];
				if (type != 'm') {
					throw std::runtime_error("Server sent unknown message type '" + std::to_string(type) + "'");
				}
				uint32_t size = (
					(uint32_t(c->recv_buffer[1]) << 16) | (uint32_t(c->recv_buffer[2]) << 8) | (uint32_t(c->recv_buffer[3]))
				);
				if (c->recv_buffer.size() < 4 + size) break; //if whole message isn't here, can't process
				//whole message *is* here, so set current server message:
				server_message = std::string(c->recv_buffer.begin() + 4, c->recv_buffer.begin() + 4 + size);

				//and consume this part of the buffer:
				c->recv_buffer.erase(c->recv_buffer.begin(), c->recv_buffer.begin() + 4 + size);

				//and reset player_positions and player_colors vectors
				player_positions.clear();
				player_colors.clear();

				// and parse message
				std::string server_message_cpy = server_message;
				while (server_message_cpy.length() > 0) {
					if (server_message_cpy[0] == '(') {										//we are processing the player's coordinates
						server_message_cpy = server_message_cpy.substr(1);					//get rid of '('

						size_t delimiter_pos = server_message_cpy.find(",");				//find the first ','
						std::string pos_x = server_message_cpy.substr(0, delimiter_pos);	//get the player's x position
						server_message_cpy = server_message_cpy.substr(delimiter_pos + 1);	//cut off the x position

						delimiter_pos = server_message_cpy.find(")");						//find the first ')'
						std::string pos_y = server_message_cpy.substr(0, delimiter_pos);	//get the player's y position
						server_message_cpy = server_message_cpy.substr(delimiter_pos + 1);	//cut off the y position

						assert(server_message_cpy[0] == '<');								//now we should be processing the player's color

						server_message_cpy = server_message_cpy.substr(1);					//get rid of the '<'

						delimiter_pos = server_message_cpy.find(",");						//find the first ','
						std::string r = server_message_cpy.substr(0, delimiter_pos);		//get the r-value of the color
						server_message_cpy = server_message_cpy.substr(delimiter_pos + 1);	//cut off the r-value

						delimiter_pos = server_message_cpy.find(",");						//find the first ','
						std::string g = server_message_cpy.substr(0, delimiter_pos);		//get the g-value of the color
						server_message_cpy = server_message_cpy.substr(delimiter_pos + 1);	//cut off the g-value

						delimiter_pos = server_message_cpy.find(",");						//find the first ','
						std::string b = server_message_cpy.substr(0, delimiter_pos);		//get the b-value of the color
						server_message_cpy = server_message_cpy.substr(delimiter_pos + 1);	//cut off the b-value

						delimiter_pos = server_message_cpy.find(">");						//find the first '>'
						std::string a = server_message_cpy.substr(0, delimiter_pos);		//get the a-value of the color
						server_message_cpy = server_message_cpy.substr(delimiter_pos + 1);	//cut off the a value

						glm::vec2 player_position = glm::vec2(std::stof(pos_x), std::stof(pos_y));
						glm::u8vec4 player_color = glm::u8vec4(stoi(r), stoi(g), stoi(b), stoi(a));

						player_positions.emplace_back(player_position);
						player_colors.emplace_back(player_color);

					}
					if (server_message_cpy[0] == 'k') { //we are processing the piano keys pressed
						//process which keys were played - they are indexed from 0 to 7
						server_message_cpy = server_message_cpy.substr(1);
						for (size_t i=0;i<server_message_cpy.size();i++) {
							int key_id = server_message_cpy[i] - '0';
							Sound::play(key_samples[key_id]);
						}
						server_message_cpy = "";
					}
				}

				assert(player_positions.size() == player_colors.size());
			}
		}
	}, 0.0);
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//taken from game0 code!
	//draw some things
	#define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))
	const glm::u8vec4 bg_color = HEX_TO_U8VEC4(0xf6eee9ff);	//Isabelline
	const glm::u8vec4 red = HEX_TO_U8VEC4(0xf5a69fff);
	const glm::u8vec4 orange = HEX_TO_U8VEC4(0xf5c69fff);
	const glm::u8vec4 yellow = HEX_TO_U8VEC4(0xf5f49fff);
	const glm::u8vec4 green = HEX_TO_U8VEC4(0xbdf59fff);
	const glm::u8vec4 light_blue = HEX_TO_U8VEC4(0x9feaf5ff);
	const glm::u8vec4 blue = HEX_TO_U8VEC4(0x9fc0f5ff);
	const glm::u8vec4 purple = HEX_TO_U8VEC4(0xc49ff5ff);
	const glm::u8vec4 pink = HEX_TO_U8VEC4(0xea9ff5ff);
	#undef HEX_TO_U8VEC4

	//other useful drawing constants:
	const float key_radius = 2.0f;
	const float wall_radius = 0.05f;
	const float padding = 0.14f; //padding between outside of walls and edge of window

	//---- compute vertices to draw ----

	//vertices will be accumulated into this list and then uploaded+drawn at the end of this function:
	std::vector< Vertex > vertices;

	//inline helper function for rectangle drawing:
	auto draw_rectangle = [&vertices](glm::vec2 const &center, glm::vec2 const &radius, glm::u8vec4 const &color) {
		//draw rectangle as two CCW-oriented triangles:
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));

		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y-radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x+radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x-radius.x, center.y+radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
	};

	//draw keys
	draw_rectangle(glm::vec2(-court_radius.x + key_radius, 0.0f), glm::vec2(key_radius, court_radius.y + 2.0f * key_radius), red);
	draw_rectangle(glm::vec2(-court_radius.x + key_radius * 3.0f, 0.0f), glm::vec2(key_radius, court_radius.y + 2.0f * key_radius), orange);
	draw_rectangle(glm::vec2(-court_radius.x + key_radius * 5.0f, 0.0f), glm::vec2(key_radius, court_radius.y + 2.0f * key_radius), yellow);
	draw_rectangle(glm::vec2(-court_radius.x + key_radius * 7.0f, 0.0f), glm::vec2(key_radius, court_radius.y + 2.0f * key_radius), green);
	draw_rectangle(glm::vec2(-court_radius.x + key_radius * 9.0f, 0.0f), glm::vec2(key_radius, court_radius.y + 2.0f * key_radius), light_blue);
	draw_rectangle(glm::vec2(-court_radius.x + key_radius * 11.0f, 0.0f), glm::vec2(key_radius, court_radius.y + 2.0f * key_radius), blue);
	draw_rectangle(glm::vec2(-court_radius.x + key_radius * 13.0f, 0.0f), glm::vec2(key_radius, court_radius.y + 2.0f * key_radius), purple);	
	draw_rectangle(glm::vec2(-court_radius.x + key_radius * 15.0f, 0.0f), glm::vec2(key_radius, court_radius.y + 2.0f * key_radius), pink);

	//draw player cursors
	for (size_t i=0;i<player_positions.size();i++) {
		draw_rectangle(player_positions[i], glm::vec2(0.25f, 0.60f), player_colors[i]);
	}
	
	//------ compute court-to-window transform ------

	//compute area that should be visible:
	glm::vec2 scene_min = glm::vec2(
		-court_radius.x - 2.0f * wall_radius - padding,
		-court_radius.y - 2.0f * wall_radius - padding
	);
	glm::vec2 scene_max = glm::vec2(
		court_radius.x + 2.0f * wall_radius + padding,
		court_radius.y + 2.0f * wall_radius + 3.0f + padding
	);

	//compute window aspect ratio:
	float aspect = drawable_size.x / float(drawable_size.y);
	//we'll scale the x coordinate by 1.0 / aspect to make sure things stay square.

	//compute scale factor for court given that...
	float scale = std::min(
		(2.0f * aspect) / (scene_max.x - scene_min.x), //... x must fit in [-aspect,aspect] ...
		(2.0f) / (scene_max.y - scene_min.y) //... y must fit in [-1,1].
	);

	glm::vec2 center = 0.5f * (scene_max + scene_min);

	//build matrix that scales and translates appropriately:
	glm::mat4 court_to_clip = glm::mat4(
		glm::vec4(scale / aspect, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(-center.x * (scale / aspect), -center.y * scale, 0.0f, 1.0f)
	);
	//NOTE: glm matrices are specified in *Column-Major* order,
	// so each line above is specifying a *column* of the matrix(!)

	//also build the matrix that takes clip coordinates to court coordinates (used for mouse handling):
	clip_to_court = glm::mat3x2(
		glm::vec2(aspect / scale, 0.0f),
		glm::vec2(0.0f, 1.0f / scale),
		glm::vec2(center.x, center.y)
	);

	//---- actual drawing ----

	//clear the color buffer:
	glClearColor(bg_color.r / 255.0f, bg_color.g / 255.0f, bg_color.b / 255.0f, bg_color.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	//don't use the depth test:
	glDisable(GL_DEPTH_TEST);

	//upload vertices to vertex_buffer:
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer); //set vertex_buffer as current
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STREAM_DRAW); //upload vertices array
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//set color_texture_program as current program:
	glUseProgram(color_texture_program.program);

	//upload OBJECT_TO_CLIP to the proper uniform location:
	glUniformMatrix4fv(color_texture_program.OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(court_to_clip));

	//use the mapping vertex_buffer_for_color_texture_program to fetch vertex data:
	glBindVertexArray(vertex_buffer_for_color_texture_program);

	//bind the solid white texture to location zero so things will be drawn just with their colors:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, white_tex);

	//run the OpenGL pipeline:
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));

	//unbind the solid white texture:
	glBindTexture(GL_TEXTURE_2D, 0);

	//reset vertex array to none:
	glBindVertexArray(0);

	//reset current program to none:
	glUseProgram(0);
	

	GL_ERRORS(); //PARANOIA: print errors just in case we did something wrong.
}
