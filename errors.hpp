#pragma once

#include <stdexcept>

namespace michaelcc {
	class compilation_error : public std::exception {
	private:
		std::string m_msg;
		const size_t m_row;
		const size_t m_col;
		const std::string m_file_name;

	public:
		compilation_error(const std::string msg, const size_t row, const size_t col, const std::string file_name);

		const size_t row() const noexcept {
			return m_row;
		}

		const size_t col() const noexcept {
			return m_col;
		}

		const std::string file_name() const noexcept {
			return m_file_name;
		}
	};
}