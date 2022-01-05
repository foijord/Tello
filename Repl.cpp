
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include <boost/asio.hpp>
#include <boost/program_options.hpp>
namespace bpo = boost::program_options;

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <queue>
#include <format>
#include <iostream>


class MessageQueue {
public:
	bool push(std::string message)
	{
		std::lock_guard lock(this->mutex);
		bool empty = this->queue.empty();
		this->queue.push(std::make_shared<std::string>(message));
		return empty;
	}

	std::shared_ptr<std::string> pop()
	{
		std::lock_guard lock(this->mutex);
		auto message = this->queue.front();
		this->queue.pop();
		return message;
	}

	bool empty()
	{
		std::lock_guard lock(this->mutex);
		return this->queue.empty();
	}

private:
	std::mutex mutex;
	std::queue<std::shared_ptr<std::string>> queue;
};


class Connection {
public:
	Connection(
		boost::asio::ip::udp::socket socket,
		boost::asio::ip::udp::endpoint endpoint) :
		socket(std::move(socket)),
		endpoint(endpoint)
	{
		this->receive_buffer.resize(1518);
	}

	void async_receive()
	{
		this->socket.async_receive_from(
			boost::asio::buffer(this->receive_buffer),
			this->endpoint,
			[this](boost::system::error_code ec, size_t bytes_received) {
			if (!ec) {
				auto message = this->receive_buffer.substr(0, bytes_received);
				std::cout << message << std::endl << "> " << std::flush;
				this->async_receive();
			}
			else {
				std::cout << "error receiving: " << ec.message() << std::endl;
			}
		});
	}

	void async_send()
	{
		auto message = this->send_queue.pop();

		this->socket.async_send_to(
			boost::asio::buffer(*message),
			this->endpoint,
			[this, message](boost::system::error_code ec, size_t bytes_sent) {
			if (!ec) {
				if (!this->send_queue.empty()) {
					this->async_send();
				}
			}
			else {
				std::cerr << "error sending: " << ec.message() << std::endl;
			}
		});
	}

	void send(std::string message)
	{
		if (this->send_queue.push(std::move(message))) {
			this->async_send();
		}
	}

	std::string receive_buffer;
	MessageQueue send_queue;
	boost::asio::ip::udp::socket socket;
	boost::asio::ip::udp::endpoint endpoint;
};

static int CONNECTED_JOYSTICK_ID = -1;

void joystick_callback(int jid, int event)
{
	BOOST_LOG_TRIVIAL(trace) << "joystick_callback";
	switch (event) {
	case GLFW_CONNECTED:
		CONNECTED_JOYSTICK_ID = jid;
		BOOST_LOG_TRIVIAL(trace) << "joystick connected";
		break;
	case GLFW_DISCONNECTED:
		CONNECTED_JOYSTICK_ID = -1;
		BOOST_LOG_TRIVIAL(trace) << "joystick disconnected";
		break;
	}
}


int main(int argc, char* argv[])
{
	try {
		boost::log::add_file_log(
			boost::log::keywords::file_name = "tello.log",
			boost::log::keywords::format = "[%TimeStamp%] [%Severity%] %Message%");

		boost::log::add_console_log(
			std::cout,
			boost::log::keywords::format = "[%TimeStamp%] [%Severity%] %Message%");

		boost::log::add_common_attributes();

		boost::log::core::get()->set_filter(
			boost::log::trivial::severity >= boost::log::trivial::trace);

		boost::asio::io_context io_context;
		auto work_guard = boost::asio::make_work_guard(io_context);

		boost::asio::ip::udp::endpoint remote_endpoint(boost::asio::ip::address::from_string("192.168.10.1"), 8889);
		boost::asio::ip::udp::endpoint local_endpoint(boost::asio::ip::udp::v4(), 9000);

		boost::asio::ip::udp::socket socket(io_context);
		socket.open(boost::asio::ip::udp::v4());
		socket.bind(local_endpoint);

		auto connection = std::make_shared<Connection>(std::move(socket), remote_endpoint);
		connection->async_receive();

		glfwSetErrorCallback([](int error, const char* description) {
			BOOST_LOG_TRIVIAL(error) << "GLFW error: " << error << " description: " << description;
			});

		if (!glfwInit()) {
			throw std::runtime_error("unable to initialize GLFW!");
		}

		glfwSetJoystickCallback(joystick_callback);

		if (glfwJoystickPresent(GLFW_JOYSTICK_1) == GLFW_TRUE) {
			CONNECTED_JOYSTICK_ID = GLFW_JOYSTICK_1;
		}
		else {
			BOOST_LOG_TRIVIAL(warning) << "no joystick connected";
		}

		GLFWgamepadstate state;

		while (true) {
			glfwPollEvents();
			io_context.poll();

			glfwGetGamepadState(GLFW_JOYSTICK_1, &state);

			if (state.buttons[GLFW_GAMEPAD_BUTTON_A] == GLFW_PRESS) {
				std::string command("command");
				BOOST_LOG_TRIVIAL(trace) << "command: " << command;
				connection->send(command);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_B] == GLFW_PRESS) {
				std::string command("takeoff");
				BOOST_LOG_TRIVIAL(trace) << "command: " << command;
				connection->send(command);
			}

			if (state.buttons[GLFW_GAMEPAD_BUTTON_X] == GLFW_PRESS) {
				std::string command("land");
				BOOST_LOG_TRIVIAL(trace) << "command: " << command;
				connection->send(command);
			}

			int a = std::clamp(static_cast<int>(state.axes[2] * 100), -100, 100);
			int b = std::clamp(static_cast<int>(state.axes[3] * 100), -100, 100);
			int c = std::clamp(static_cast<int>(state.axes[1] * 100), -100, 100);
			int d = std::clamp(static_cast<int>(state.axes[0] * 100), -100, 100);

			auto command = std::format("rc {} {} {} {}", a, b, c, d);
			BOOST_LOG_TRIVIAL(trace) << "command: " << command;
			connection->send(command);

			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
	return EXIT_SUCCESS;
}
