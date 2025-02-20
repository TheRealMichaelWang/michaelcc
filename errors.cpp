#include <sstream>
#include "errors.hpp"

using namespace michaelcc;

compilation_error::compilation_error(const std::string msg, const size_t row, const size_t col, const std::string file_name) : m_row(row), m_col(col), m_file_name(file_name)
{
	std::stringstream ss;
	ss << msg << " \"" << file_name << "\" @ " << row << ':' << col;
	m_msg = ss.str();
}