#ifndef MICHAELCC_ERRORS_HPP
#define MICHAELCC_ERRORS_HPP

#include <filesystem>

namespace michaelcc {
	struct source_location {
	private:
		size_t m_row;
		size_t m_col;
		std::filesystem::path m_file_name;

	public:
		source_location(const size_t row, const size_t col, const std::filesystem::path file_name) : m_row(row), m_col(col), m_file_name(file_name) {

		}

		source_location() : source_location(1, 1, std::filesystem::path()) { }

		const size_t row() const noexcept {
			return m_row;
		}

		const size_t col() const noexcept {
			return m_col;
		}

		const std::filesystem::path filename() const noexcept {
			return m_file_name;
		}

		const std::string to_string() const;

		void increment_line() noexcept {
			m_row++;
		}

		void set_col(uint32_t col) noexcept {
			m_col = col;
		}
	};

	class compilation_error : public std::exception {
	private:
		std::string m_msg, m_display_msg;
		const source_location m_location;

	public:
		compilation_error(const std::string msg, const source_location location);

		const source_location location() const noexcept {
			return m_location;
		}

		const std::string msg() const noexcept {
			return m_msg;
		}

		const char* what() const noexcept override {
			return m_display_msg.c_str();
		}
	};
}

#endif