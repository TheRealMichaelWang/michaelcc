#ifndef MICHAELCC_ERRORS_HPP
#define MICHAELCC_ERRORS_HPP

#include <filesystem>
#include <string>
#include <exception>

namespace michaelcc {

class source_location {
	size_t m_row = 1;
	size_t m_col = 1;
	std::filesystem::path m_file;

public:
	source_location() = default;

	source_location(size_t row, size_t col, std::filesystem::path file)
		: m_row(row), m_col(col), m_file(std::move(file)) {}

	size_t row() const noexcept { return m_row; }
	size_t col() const noexcept { return m_col; }
	const std::filesystem::path& file() const noexcept { return m_file; }

	std::string to_string() const;

	void next_line() noexcept { ++m_row; m_col = 1; }
	void set_col(size_t col) noexcept { m_col = col; }
};

class compile_error : public std::exception {
	std::string m_message;
	source_location m_location;
	std::string m_formatted;

public:
	compile_error(std::string message, source_location location);

	const source_location& location() const noexcept { return m_location; }
	const std::string& message() const noexcept { return m_message; }
	const char* what() const noexcept override { return m_formatted.c_str(); }
};

}

#endif
