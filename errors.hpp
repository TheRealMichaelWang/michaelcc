#pragma once

#include <stdexcept>
#include <filesystem>

namespace michaelcc {
	struct source_location {
		const size_t row;
		const size_t col;
		const std::filesystem::path file_name;

		source_location(const size_t row, const size_t col, const std::filesystem::path file_name) : row(row), col(col), file_name(file_name) {

		}

		const std::string to_string() const;
	};

	class compilation_error : public std::exception {
	private:
		std::string m_msg;
		const source_location m_location;

	public:
		compilation_error(const std::string msg, const source_location location);

		const source_location location() const noexcept {
			return m_location;
		}

		const std::string msg() const noexcept {
			return m_msg;
		}

		const char* what() const override {
			return m_msg.c_str();
		}
	};
}